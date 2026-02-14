# AST 内存释放 — 补充分析：TranslateToCHIR 过程中边翻译边释放的可行性

## 1. 反馈问题

> 能不能在 TranslateToCHIR 执行过程中，一边翻译一边释放？

## 2. 结论

**不可行**。TranslateToCHIR 内部的多遍处理、指针缓存和并行翻译机制决定了 AST 节点必须在整个 lowering 过程中保持存活。以下是详细分析。

## 3. TranslateToCHIR 内部流程分析

`ToCHIRPackage()` 的执行流程（`AST2CHIR.cpp:506-545`）：

```
Step 1: CollectTopLevelDecls(pkg)
  ├─ CollectImplicitFuncs()           ← 收集隐式导入函数
  ├─ CollectImportedDecls(pkg)        ← 遍历所有 imported packages 的 AST 节点
  └─ CollectDeclsInCurPkg(pkg)        ← 遍历当前包所有文件的声明

Step 2: SortGlobalVarDecl(pkg)
  └─ GlobalDeclAnalysis::Run()        ← 分析所有全局变量的初始化依赖（需要看到全部声明）

Step 3: CacheSomeDeclsToGlobalSymbolTable()
  ├─ CacheCustomTypeDefToGlobalSymbolTable()  ← 为所有 nominal 类型创建 CHIR shell
  ├─ TranslateAllCustomTypeTy()               ← 翻译所有自定义类型（查找 AST::Decl 指针）
  └─ CacheTopLevelDeclToGlobalSymbolTable()   ← 为所有顶层函数/变量创建签名

Step 4: TranslateAllDecls(pkg, initOrder)
  ├─ TranslateNominalDecls(pkg)               ← 翻译 nominal 类型的方法体
  ├─ TranslateInitOfGlobalVars(pkg, initOrder)← 翻译全局变量初始化
  └─ TranslateTopLevelDeclsInParallel()       ← 并行翻译所有函数体
      └─ 每个函数体可引用任意已缓存的 AST 声明

Step 5: AST2CHIRCheck()                       ← 验证
```

## 4. 不可行的六个原因

### 4.1 指针缓存机制

lowering 使用两个核心缓存，均以 **AST 节点指针为 key**：

```cpp
// AST2CHIR.h:385 — 全局声明缓存
AST2CHIRNodeMap<Value> globalCache;
// key: const AST::Node* → value: CHIR::Value*

// CHIRType.h:19-29 — 类型缓存
std::unordered_map<AST::Ty*, Type*> typeMap;
// key: AST::Ty* → value: CHIR::Type*

AST2CHIRNodeMap<CustomTypeDef> globalNominalCache;
// key: const AST::Node* → value: CHIR::CustomTypeDef*
```

如果在翻译过程中释放 AST 节点，这些 map 的 key 会变成悬空指针。后续查找（如翻译函数体时查找被调用函数的 CHIR 签名）会触发 use-after-free。

### 4.2 多遍处理

lowering 分 5 步执行，后续步骤依赖前序步骤收集的 AST 信息：

- Step 1 收集所有声明 → Step 2 分析全局变量依赖 → Step 3 创建签名 → Step 4 翻译函数体
- Step 4 翻译函数体时，需要通过 `globalCache.Get(*astFunc)` 查找其他函数的 CHIR 签名，而 `globalCache` 的 key 是 Step 3 中缓存的 AST 节点指针

**即使某个声明在 Step 3 完成签名创建后"看似不再需要"，Step 4 中其他函数体仍可能通过指针引用它。**

### 4.3 并行函数体翻译

```cpp
// AST2CHIR.cpp:371-372
if (opts.GetJobs() > 1) {
    TranslateTopLevelDeclsInParallel();  // 多线程并行翻译
}
```

所有函数体共享同一个 `globalCache`，任何函数体都可能引用任何其他声明。在并行场景下，无法确定某个 AST 节点何时"不再被任何线程需要"。

### 4.4 全局变量初始化依赖分析

```cpp
// AST2CHIR.cpp:530
auto [initOrder, result] = SortGlobalVarDecl(node);
```

`GlobalDeclAnalysis::Run()` 需要同时看到**所有全局变量和函数声明**来构建依赖图并检测循环依赖。这是一个全局分析，无法增量执行。

### 4.5 类型翻译的交叉引用

```cpp
// CHIRType.cpp:53
auto def = chirTypeCache.globalNominalCache.Get(*structTy.declPtr);
```

翻译一个 struct 类型时，需要查找其 AST::Decl 指针对应的 CHIR CustomTypeDef。如果该 Decl 已被释放，查找会失败。类型之间存在复杂的交叉引用（如 `class A { var b: B }` 和 `class B { var a: A }`），无法确定安全的释放顺序。

### 4.6 Imported Packages 的 AST 遍历

```cpp
// ASTPackage2CHIR.cpp:561-576
for (auto& importPkg : importManager.GetAllImportedPackages()) {
    for (auto& file : importPkg->srcPackage->files) {
        for (auto& decl : file->decls) {
            CollectImportedDeclUsedInCurPkg(*decl);  // 遍历 imported 包的 AST
        }
    }
}
```

每个源包的 lowering 都会遍历所有 imported packages 的 AST 节点。即使源包 A 的 lowering 完成，imported packages 的 AST 仍被源包 B 的 lowering 需要。

## 5. 可行的替代方案

虽然"边翻译边释放单个 AST 节点"不可行，但有两个粒度更粗的增量释放策略值得考虑：

### 5.1 方案 A-1：源包级增量释放（可行，收益有限）

在多源包编译场景下，源包 A 的 lowering 完成后，源包 A 的 AST 可以释放（因为源包 B 的 lowering 只访问 imported packages，不访问其他源包）。

```cpp
bool CompilerInstance::PerformCHIRCompilation()
{
    auto sourcePackagesCHIR = GetSourcePackages();
    auto pkgsOrderedCHIR = packageManager->GetBuildOrders();

    for (auto& pkgsInfos : pkgsOrderedCHIR) {
        auto pkg = FindSourcePackage(sourcePackagesCHIR, pkgsInfos);
        if (pkg != sourcePackagesCHIR.end()) {
            ret = ret && GenerateCHIRForPkg(**pkg);

            // 源包 lowering 完成后，可以释放该源包的 AST
            // 但需要先提取 CodeGen 所需信息
            PrepareCodeGenInfoForPackage(**pkg);
            ReleaseSourcePackageAST(**pkg);  // 释放单个源包

            sourcePackagesCHIR.erase(pkg);
        }
    }
    // imported packages 仍需保留到所有源包 lowering 完成
    // ...
}
```

**局限性**：
- 大多数项目只有 1 个源包，此方案无收益
- imported packages（通常占 AST 内存的 30-50%）无法提前释放
- 实现复杂度增加（需要逐包管理 ASTContext 和相关引用）

### 5.2 方案 A-2：lowering 后立即释放（推荐，v2 方案的微调）

将 AST 释放点从独立的 `RELEASE_AST` 阶段提前到 `PerformCHIRCompilation()` 的末尾：

```cpp
bool CompilerInstance::PerformCHIRCompilation()
{
    // ... 现有 lowering 逻辑 ...
    diag.EmitCategoryDiagnostics(DiagCategory::CHIR);

    // lowering 全部完成后，立即释放 AST
    // 不需要等到独立的 RELEASE_AST 阶段
    if (!invocation.globalOptions.noReleaseAST) {
        ReleaseASTMemory();
    }
    return ret;
}
```

**优势**：
- 不需要新增 `CompileStage::RELEASE_AST` 枚举
- 释放时机更早（在 CHIR 阶段内部而非之后）
- 实现更简单，不影响流水线结构

**与 v2 方案的关系**：这是 v2 方案的实现细节优化，核心清理逻辑（5 阶段 checklist）不变。

## 6. 最终建议

| 策略 | 可行性 | 内存收益 | 复杂度 | 建议 |
|------|--------|---------|--------|------|
| 边翻译边释放单个 AST 节点 | ❌ 不可行 | — | — | 放弃 |
| 源包级增量释放（A-1） | ⚠️ 有限可行 | 低（多数项目单包） | 高 | 暂不实施 |
| lowering 后立即释放（A-2） | ✅ 可行 | 高（90-95%） | 低 | **推荐** |
| 独立 RELEASE_AST 阶段（v2 原方案） | ✅ 可行 | 高（90-95%） | 中 | 备选 |

**推荐方案 A-2**：在 `PerformCHIRCompilation()` 末尾调用 `ReleaseASTMemory()`，这是最早的安全释放点，且不需要修改编译流水线结构。v2 方案中设计的完整 5 阶段清理 checklist 完全适用。
