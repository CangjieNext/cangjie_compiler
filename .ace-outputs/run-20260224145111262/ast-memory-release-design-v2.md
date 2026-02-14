# AST 内存释放方案 — 迭代优化设计 v2

## 修订说明

本文档是对 v1 设计方案（`ast-memory-release-design.md`）的迭代优化，针对攻击方发现的 12 个确认缺陷（4 P0 / 5 P1 / 2 P2 / 1 P3）和评审方的裁决意见进行补全。

**核心结论不变**：方案 A（CHIR Lowering 后显式释放 AST）的架构方向正确，CHIR IR 不持有 AST 指针的核心假设经代码审计验证成立。

**本次迭代重点**：解决 4 个 P0 阻塞项 + 5 个 P1 改进项。

---

## 1. P0 阻塞项修复设计

### 1.1 [D-01/O-01] CodeGen 入口重构 — 脱离 srcPkgs 依赖

#### 问题根因

`DefaultCIImpl::PerformCodeGen()` 通过 `ci.GetSourcePackages()` 遍历 `srcPkgs` 驱动 CodeGen，`CodegenOnePackage` 签名为 `AST::Package&`。AST 释放后 `srcPkgs` 为空，CodeGen 循环体不执行，编译器静默成功但不产出目标代码。

#### 代码分析

```cpp
// DefaultCompilerInstance.cpp:305-316 — 当前实现
bool DefaultCIImpl::PerformCodeGen()
{
    RearrangeImportedPackageDependence();
    bool ret = true;
    for (auto& srcPkg : ci.GetSourcePackages()) {   // srcPkgs 已被清空
        ret = ret && CodegenOnePackage(*srcPkg, false);
    }
    return ret;
}

// DefaultCompilerInstance.cpp:222-254 — CodegenOnePackage 实际访问的 AST 字段
bool DefaultCIImpl::CodegenOnePackage(Package& pkg, bool enableIncrement)
{
    if (pkg.IsEmpty()) { return true; }              // 仅检查空包
    // ...
    // EmitLLVMSimilarBytecode 内部：
    //   GenPackageModules(builder, ci.chirData, ...) — 不传 AST::Package
    //   pkg.fullPackageName — 仅用于生成文件路径
}
```

**关键发现**：`CodegenOnePackage` 对 `AST::Package` 的实际依赖仅有两点：
1. `pkg.IsEmpty()` — 判断是否跳过空包
2. `pkg.fullPackageName` — 用于生成 .bc 文件路径

而 `GenPackageModules()` 的签名（`EmitPackageIR.cpp:587`）接收的是 `CHIRData&`，不依赖 `AST::Package`。

#### 修复方案

**策略：在 AST 释放前，将 CodeGen 所需的最小元信息提取到独立结构中。**

```cpp
// CompilerInstance.h — 新增
struct PackageCodeGenInfo {
    std::string fullPackageName;
    bool isEmpty;
};

class CompilerInstance {
    // ...
    std::vector<PackageCodeGenInfo> codeGenPkgInfos;  // AST 释放前填充
};
```

```cpp
// CompilerInstance.cpp — ReleaseASTMemory() 中，在 srcPkgs.clear() 之前
void CompilerInstance::PrepareCodeGenInfo()
{
    codeGenPkgInfos.clear();
    for (auto& srcPkg : srcPkgs) {
        codeGenPkgInfos.push_back({srcPkg->fullPackageName, srcPkg->IsEmpty()});
    }
}
```

```cpp
// DefaultCompilerInstance.cpp — 重构后的 PerformCodeGen
bool DefaultCIImpl::PerformCodeGen()
{
    RearrangeImportedPackageDependenceV2();  // 重构版本，见下文
    bool ret = true;
    if (ci.IsASTReleased()) {
        // AST 已释放，通过 codeGenPkgInfos 驱动
        for (auto& info : ci.codeGenPkgInfos) {
            ret = ret && CodegenOnePackageV2(info, false);
        }
    } else {
        // 兼容旧路径（--no-release-ast 或 AST 未释放场景）
        for (auto& srcPkg : ci.GetSourcePackages()) {
            ret = ret && CodegenOnePackage(*srcPkg, false);
        }
    }
    return ret;
}
```

```cpp
// DefaultCompilerInstance.cpp — 新增 CodegenOnePackageV2
bool DefaultCIImpl::CodegenOnePackageV2(
    const PackageCodeGenInfo& pkgInfo, bool enableIncrement)
{
    if (pkgInfo.isEmpty) { return true; }
    if (ci.invocation.globalOptions.disableCodeGen) { return true; }

    auto backend = ci.invocation.globalOptions.backend;
    switch (backend) {
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
        case Triple::BackendType::CJNATIVE: {
            // EmitLLVMSimilarBytecode 重构：接受 pkgName 而非 AST::Package&
            if (!EmitLLVMSimilarBytecodeV2(pkgInfo.fullPackageName, enableIncrement)) {
                return false;
            }
            break;
        }
#endif
        // ...
    }
    return true;
}
```

```cpp
// DefaultCompilerInstance.cpp — EmitLLVMSimilarBytecodeV2
bool DefaultCIImpl::EmitLLVMSimilarBytecodeV2(
    const std::string& fullPackageName, bool enableIncrement)
{
    CHIR::CHIRBuilder builder(ci.chirData.GetCHIRContext());
    // GenPackageModules 不依赖 AST::Package，签名已验证
    llvmModules = CodeGen::GenPackageModules(
        builder, ci.chirData, ci.invocation.globalOptions, ci, enableIncrement);

    Utils::ProfileRecorder recorder("CodeGen", "Save bc file");
    ci.invocation.globalOptions.UpdateCachedDirName(fullPackageName);
    // ... 后续文件路径生成使用 fullPackageName 字符串
    if (llvmModules.size() == 1) {
        auto filePath = GenerateBCFilePathAndUpdateToInvocation(
            TempFileKind::T_BC, fullPackageName);
        if (filePath.empty()) { return false; }
        CodeGen::SavePackageModule(*llvmModules[0], filePath);
    } else {
        // 多模块并行保存逻辑不变，仅替换 pkg.fullPackageName → fullPackageName
        // ...
    }
    if (ci.invocation.globalOptions.enIncrementalCompilation) {
        auto fileName = GenerateFileName(fullPackageName, "");
        ci.cachedInfo.bitcodeFilesName = std::vector<std::string>{fileName};
    }
    return true;
}
```

**`RearrangeImportedPackageDependence` 重构**：

```cpp
// DefaultCompilerInstance.cpp — 当前实现访问 AST::Package 指针
void DefaultCIImpl::RearrangeImportedPackageDependence()
{
    std::vector<Ptr<Package>> allImportedPackages;
    for (auto pd : ci.importManager.GetAllImportedPackages(true)) {
        CJC_ASSERT(pd && pd->srcPackage);
        allImportedPackages.push_back(pd->srcPackage);  // AST 指针
    }
    ci.packageManager->ResolveDependence(allImportedPackages);
}
```

修复策略：在 AST 释放前将依赖关系物化，CodeGen 阶段跳过此步骤。

```cpp
void CompilerInstance::MaterializePackageDependence()
{
    // 在 AST 释放前调用，将包依赖关系物化到 packageManager 中
    std::vector<Ptr<AST::Package>> allImportedPackages;
    for (auto pd : importManager.GetAllImportedPackages(true)) {
        if (pd && pd->srcPackage) {
            allImportedPackages.push_back(pd->srcPackage);
        }
    }
    packageManager->ResolveDependence(allImportedPackages);
    packageDependenceResolved = true;
}

// DefaultCompilerInstance.cpp — 重构后
void DefaultCIImpl::RearrangeImportedPackageDependenceV2()
{
    if (ci.packageDependenceResolved) {
        return;  // 已在 AST 释放前完成
    }
    // 旧路径 fallback
    RearrangeImportedPackageDependence();
}
```

**影响范围**：`DefaultCompilerInstance.cpp`、`CompilerInstance.h/cpp`
**向后兼容**：通过 `IsASTReleased()` 分支保留旧路径，`--no-release-ast` 模式下行为不变。

---

### 1.2 [D-02/O-02] 完整 AST 指针清理 Checklist

#### 问题根因

v1 设计的 `ReleaseASTMemory()` 只清理了 7 个持有者，遗漏了 CompilerInstance 中的 7 个直接成员和三大管理器中的 20+ 个 AST 指针字段。

#### 完整清理 Checklist

以下是经代码审计确认的所有 AST 指针持有者，按清理顺序排列：

```cpp
void CompilerInstance::ReleaseASTMemory()
{
    if (astReleased) { return; }

    // ═══════════════════════════════════════════════════
    // Phase 0: 前置操作（必须在 AST 释放前完成）
    // ═══════════════════════════════════════════════════

    // 0a. 提取 CodeGen 所需的最小元信息
    PrepareCodeGenInfo();

    // 0b. 物化包依赖关系
    MaterializePackageDependence();

    // 0c. 物化 CompilationCache 中的 AST 引用
    MaterializeCachedASTReferences();

    // 0d. CHIR 输出模式下提前保存 CJO
    if (invocation.globalOptions.outputMode == GlobalOptions::OutputMode::CHIR) {
        PerformResultsSaving();
        resultsSaved = true;
    }

    // ═══════════════════════════════════════════════════
    // Phase 1: 清除 CHIR 层的 AST 反向引用
    // ═══════════════════════════════════════════════════

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    // 1a. CHIRType 的 AST::Ty* → CHIR::Type* 映射
    chirData.ClearASTTypeCache();

    // 1b. DiagAdapter 中的 AST::Node 指针（D-06）
    for (auto* chirPkg : chirData.GetAllCHIRPackages()) {
        if (chirPkg) {
            chirPkg->diag.posRange2MacroCallMap.clear();
        }
    }
#endif

    // 1c. 全局 g_annoInfo（D-04，防御性清理）
    // 注意：g_annoInfo 是匿名命名空间的 static 变量，
    // 需要在 AnnotationChecker.h 中暴露清理接口
    CHIR::ClearGlobalAnnotationInfo();

    // ═══════════════════════════════════════════════════
    // Phase 2: 清除 CompilerInstance 直接成员的 AST 引用
    // ═══════════════════════════════════════════════════

    // 2a. astPkg2chirPkgMap → chirPkgByName 替换
    for (auto& [astPkg, chirPkg] : astPkg2chirPkgMap) {
        chirPkgByName[astPkg->fullPackageName] = chirPkg;
    }
    astPkg2chirPkgMap.clear();

    // 2b. 遗漏的 7 个直接成员（D-02）
    currentPkg = nullptr;
    mangledName2DeclMap.clear();
    rawMangleName2DeclMap.clear();
    declsWithDuplicatedRawMangleName.clear();
    directExtends.clear();
    order.clear();
    chirInfo.ccTys.clear();

    // 2c. 合并后的包列表
    pkgs.clear();

    // ═══════════════════════════════════════════════════
    // Phase 3: 清理/释放管理器（D-03）
    // ═══════════════════════════════════════════════════

    // 3a. TypeManager — 持有 15+ 个 AST 指针集合
    //     选择 delete 而非逐字段清理，因为 CHIR/CodeGen 不依赖 TypeManager
    delete typeManager;
    typeManager = nullptr;

    // 3b. TypeChecker — 持有 TypeManager 引用，必须在 TypeManager 之后或同时释放
    delete typeChecker;
    typeChecker = nullptr;

    // 3c. GenericInstantiationManager
    delete gim;
    gim = nullptr;

    // 3d. TestManager — 持有 ImportManager 和 TypeManager 引用
    delete testManager;
    testManager = nullptr;

    // 3e. BaseMangler — ManglerContext 持有 6 个 AST 指针 map
    mangler.reset();

    // 3f. ImportManager — 栈成员，不能 delete，调用清理方法
    importManager.ClearASTReferences();

    // 3g. PackageManager — 持有包依赖关系（已物化），可释放
    //     注意：如果 CodeGen 仍需要 packageManager，则仅清理 AST 引用
    packageManager->ClearASTReferences();

    // ═══════════════════════════════════════════════════
    // Phase 4: 释放 AST 核心数据（严格顺序）
    // ═══════════════════════════════════════════════════

    // 4a. 先释放 AST 节点（触发 Symbol detach）
    srcPkgs.clear();

    // 4b. 再释放 ASTContext（依赖 AST 节点已析构）
    pkgCtxMap.clear();

    // ═══════════════════════════════════════════════════
    // Phase 5: 标记完成
    // ═══════════════════════════════════════════════════
    astReleased = true;
}
```

#### 管理器清理接口设计

```cpp
// ImportManager — 新增方法
void ImportManager::ClearASTReferences()
{
    curPackage = nullptr;
    // CjoManagerImpl 持有 importedPackages（OwnedPtr<AST::Package>）
    // 释放 imported packages 以回收内存（D-08）
    cjoManager->ClearImportedPackages();
    // 清除 packageNameMap 中的 AST::Package 指针
    cjoManager->ClearPackagePointers();
}

// PackageManager — 新增方法
void PackageManager::ClearASTReferences()
{
    // 清除内部的 AST::Package 指针引用
    // 依赖关系已通过 MaterializePackageDependence() 物化
    packageDependence.clear();
}
```

---

### 1.3 [D-03] 三大管理器 AST 指针清理策略

#### TypeManager（delete 策略）

**理由**：TypeManager 持有 15+ 个 AST 指针集合（`TypeManager.h:429-476`），逐字段清理成本高且易遗漏。CodeGen 阶段不依赖 TypeManager（`GenPackageModules` 不接收 TypeManager 参数）。直接 `delete typeManager` 是最安全的选择。

**风险点**：`GenPackageModules` 接收 `DefaultCompilerInstance&` 引用，内部是否间接访问 `typeManager`？

**验证结果**：`GenPackageModules`（`EmitPackageIR.cpp:587`）内部通过 `CHIRData` 和 `GlobalOptions` 工作，不访问 `typeManager`。CodeGen 的类型信息完全来自 CHIR IR 的类型系统（`CHIR::Type`），与 AST 类型系统（`AST::Ty`）独立。

**析构顺序约束**：
- `TypeChecker` 持有 `TypeManager&` 引用 → 必须先 delete `typeChecker`，再 delete `typeManager`
- 或者同时 delete（当前析构函数中 typeChecker 在 typeManager 之前）

```cpp
// 正确顺序（与现有析构函数一致）
delete typeChecker;   typeChecker = nullptr;
delete typeManager;   typeManager = nullptr;
```

#### BaseMangler（reset 策略）

**理由**：`BaseMangler` 通过 `unique_ptr<BaseMangler> mangler` 持有。`ManglerContext`（`BaseMangler.h:160-169`）包含 6 个 AST 指针 map：
- `file2GlobalWildcardVar`: `map<Ptr<const AST::File>, vector<Ptr<AST::VarWithPatternDecl>>>`
- `node2Lambda`: `map<Ptr<const AST::Node>, vector<Ptr<AST::LambdaExpr>>>`
- `node2LocalVar`, `node2LocalFunc`, `file2ExtendDecl`, `node2LocalType`

Mangling 阶段在 CHIR 之前完成，后续不再需要 mangler。直接 `mangler.reset()` 释放。

#### ImportManager（清理方法策略）

**理由**：`ImportManager` 是 `CompilerInstance` 的栈成员（非指针），不能 delete。需要提供 `ClearASTReferences()` 方法。

**关键子问题 — imported packages 释放（D-08）**：

`CjoManagerImpl::importedPackages`（`CjoManagerImpl.h:184`）以 `OwnedPtr<AST::Package>` 持有 imported packages。这些包独立于 `srcPkgs`，不释放会导致内存收益从 90-95% 降至 50-60%。

**释放策略**：
1. 在 `MaterializePackageDependence()` 中已完成依赖关系物化
2. `RearrangeImportedPackageDependenceV2()` 在 CodeGen 前检查 `packageDependenceResolved` 标记，不再访问 imported packages
3. 因此可以安全释放 imported packages

```cpp
void CjoManagerImpl::ClearImportedPackages()
{
    importedPackages.clear();  // 释放所有 imported AST::Package
    for (auto& [name, info] : packageNameMap) {
        info.pkg = nullptr;    // 清除 Ptr<AST::Package> 引用
    }
}
```

---

### 1.4 [D-05/O-03] IncrementalCompilerInstance 适配

#### 问题根因

`IncrementalCompilerInstance::PerformResultsSaving()`（`IncrementalCompilerInstance.cpp:519-538`）在所有模式下都调用 `GetSourcePackages()` 和 `ExportASTSignature(*srcPkg)`，不仅限于 CHIR 模式。

`IncrementalCompilerInstance` 还有额外的 AST 指针成员：
- `changedTypes`: `unordered_set<Ptr<const AST::StructTy>>`（`IncrementalCompilerInstance.h:49`）
- `declsToBeReCompiled`: `unordered_set<Ptr<AST::Decl>>`（`IncrementalCompilerInstance.h:52`）

#### 修复方案

**策略：`IncrementalCompilerInstance` override `PerformASTRelease()`，在释放前完成所有 AST 依赖操作。**

```cpp
// IncrementalCompilerInstance.h — 新增
bool PerformASTRelease() override;

// IncrementalCompilerInstance.cpp
bool IncrementalCompilerInstance::PerformASTRelease()
{
    // 1. 增量编译特有：在 AST 释放前完成 AST 签名导出和 CJO 保存
    //    （不论是否 CHIR 模式，增量编译都需要）
    if (!PerformResultsSaving()) {
        return false;
    }
    resultsSaved = true;

    // 2. 清理增量编译特有的 AST 指针成员
    changedTypes.clear();
    declsToBeReCompiled.clear();
    cacheMangles.clear();  // 如果包含 AST 指针

    // 3. 调用基类释放逻辑
    ReleaseASTMemory();
    return true;
}
```

**`PerformResultsSaving` 的幂等性保证**：

```cpp
bool IncrementalCompilerInstance::PerformResultsSaving()
{
    if (resultsSaved) {
        return true;  // 已在 PerformASTRelease 中完成
    }
    // ... 原有逻辑（ExportASTSignature、SaveCjo 等）
}
```

**影响范围**：`IncrementalCompilerInstance.h/cpp`
**向后兼容**：增量编译的行为不变，只是 AST 签名导出的时机从 `SAVE_RESULTS` 阶段提前到 `RELEASE_AST` 阶段。

---

## 2. P1 改进项修复设计

### 2.1 [D-04] 全局 g_annoInfo 清理

**问题**：`g_annoInfo`（`AnnotationChecker.cpp:47`）是匿名命名空间的 `static` 变量，持有 `CHIR::ClassDef*` 键和 `AnnotationInfo`（含 `const ClassDecl*`、`AST::Annotation*`）值。

**使用范围验证**：`g_annoInfo` 在 `GlobalVarInitializer`（CHIR lowering 期间）和 `BuildCheckMap`（CHIR annotation checking 期间）中使用，均在 CHIR lowering 阶段内完成。

**修复**：在 `AnnotationChecker.h` 中暴露清理接口，在 CHIR lowering 完成后调用。

```cpp
// AnnotationChecker.h — 新增
namespace Cangjie::CHIR {
void ClearGlobalAnnotationInfo();
}

// AnnotationChecker.cpp
void Cangjie::CHIR::ClearGlobalAnnotationInfo()
{
    g_annoInfo.clear();
}
```

在 `ReleaseASTMemory()` 的 Phase 1 中调用（已包含在 1.2 的 checklist 中）。

---

### 2.2 [D-06] DiagAdapter 的 posRange2MacroCallMap 清理

**问题**：`DiagAdapter` 是 `CHIR::ToCHIR` 的成员（`CHIR.h:214`），而 `ToCHIR` 对象的生命周期可能贯穿 CodeGen。`posRange2MacroCallMap`（`DiagAdapter.h:101`）持有 `Ptr<AST::Node>`。

**修复**：在 `ReleaseASTMemory()` 中遍历所有 CHIR::Package 清理 DiagAdapter（已包含在 1.2 的 checklist Phase 1b 中）。

```cpp
for (auto* chirPkg : chirData.GetAllCHIRPackages()) {
    if (chirPkg) {
        chirPkg->diag.posRange2MacroCallMap.clear();
    }
}
```

**影响评估**：清理后，如果 CodeGen 阶段对宏展开代码产生诊断，将无法回溯到原始宏调用位置。但诊断仍然可以通过 CHIR 的 `DebugLocation` 提供文件名和行号信息，只是缺少宏展开上下文。这是可接受的降级。

---

### 2.3 [D-08] Imported Packages 释放策略

**问题**：`CjoManagerImpl::importedPackages` 独立持有 imported AST::Package 的所有权，不释放会导致内存收益降低 30-40%。

**修复**：已在 1.3 节的 ImportManager 清理策略中设计。核心要点：
1. `MaterializePackageDependence()` 在释放前完成依赖关系物化
2. `CjoManagerImpl::ClearImportedPackages()` 释放所有 imported packages
3. `packageNameMap` 中的 `Ptr<AST::Package>` 引用置空

---

### 2.4 [D-09] rawMangleName 空键物化冲突

**问题**：设计方案使用 `rawMangledName`（拼写错误），实际字段名为 `rawMangleName`（`Node.h:940`）。合成声明/泛型实例化场景下该字段可能为空，导致物化键冲突。

**修复**：

```cpp
void CompilerInstance::MaterializeCachedASTReferences()
{
    MaterializedVarFuncDep matDep;
    for (auto& [decl, deps] : cachedInfo.varAndFuncDep) {
        // 使用正确的字段名 rawMangleName
        // 空键 fallback：使用 mangledName（Mangling 阶段已填充）
        std::string key = decl->rawMangleName.empty()
            ? decl->mangledName : decl->rawMangleName;
        CJC_ASSERT(!key.empty() && "Decl has neither rawMangleName nor mangledName");

        std::vector<std::string> depNames;
        for (auto& dep : deps) {
            std::string depKey = dep->rawMangleName.empty()
                ? dep->mangledName : dep->rawMangleName;
            depNames.push_back(std::move(depKey));
        }
        matDep.emplace_back(std::move(key), std::move(depNames));
    }

    // 完整性校验：物化前后 entry 数量一致
    CJC_ASSERT(matDep.size() == cachedInfo.varAndFuncDep.size()
        && "Materialization lost entries");

    cachedInfo.varAndFuncDep.clear();
    cachedInfo.materializedVarFuncDep = std::move(matDep);

    // semaInfo.usages 同理...
}
```

---

### 2.5 [D-11] 析构函数与 ReleaseASTMemory 的交互

**问题**：`~CompilerInstance()` 中的清理逻辑与 `ReleaseASTMemory()` 存在重叠。如果 AST 已释放，析构函数中的 `srcPkgs.clear()` 和 `delete typeManager` 等操作会对已清空/已 null 的对象重复操作。

**修复**：在析构函数中检查 `astReleased` 标记，跳过已释放的资源。

```cpp
CompilerInstance::~CompilerInstance()
{
    if (!astReleased) {
        // AST 未释放（--no-release-ast 模式或编译中途退出）
        // 按原有顺序清理
        srcPkgs.clear();
        pkgCtxMap.clear();
        delete typeChecker;   typeChecker = nullptr;
        delete typeManager;   typeManager = nullptr;
        delete testManager;   testManager = nullptr;
        delete gim;           gim = nullptr;
    }
    // 以下资源无论是否释放 AST 都需要清理
    delete compileStrategy;   compileStrategy = nullptr;
    delete packageManager;    packageManager = nullptr;
    // mangler 是 unique_ptr，自动析构
    // importManager 是栈成员，自动析构
}
```

**关键不变量**：
- `delete nullptr` 是安全的 no-op（C++ 标准保证）
- `vector::clear()` 对空 vector 是安全的 no-op
- 因此即使不加 `astReleased` 检查也不会 crash，但加检查可以避免不必要的操作和潜在的析构顺序问题

---

## 3. P2/P3 项处理

### 3.1 [D-10] Release 构建 AST 访问保护（P2）

**裁决**：降级为 Low。Debug 构建使用 `CJC_ASSERT` 宏，Release 构建依赖 ASan 测试覆盖。这与 LLVM/Clang 的实践一致。

**额外措施**：在 `GetSourcePackages()` 中添加低成本的 Release 构建保护：

```cpp
std::vector<Ptr<Package>> CompilerInstance::GetSourcePackages()
{
    if (astReleased) {
        // Release 构建中也能捕获误调用，返回空而非访问已释放内存
        return {};
    }
    // ... 原有逻辑
}
```

### 3.2 [D-12] --no-release-ast 与增量编译交互（P2）

**设计**：`--no-release-ast` 是调试开关，语义简单化：
- 设置后，`PerformASTRelease()` 直接返回 true，不执行任何释放
- 增量编译模式下行为与当前完全一致（AST 存活到 CompilerInstance 析构）
- 不需要额外的交互设计

### 3.3 [D-13] Ptr 模板无 use-after-free 检测（P3）

**裁决**：不在本次范围内。长期可考虑在 Debug 构建中为 `Ptr<T>` 添加 epoch-based 有效性检查，但这是基础设施级别的改动，与 AST 释放方案解耦。

---

## 4. 修订后的完整架构图

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CompilerInstance::Compile() — 修订后                    │
│                                                                            │
│  ┌─────┐  ┌──────┐  ┌────┐  ┌────┐  ┌───────┐  ┌────┐  ┌───────────┐  │
│  │PARSE├─►│IMPORT├─►│SEMA├─►│MANG├─►│SAVE   ├─►│CHIR├─►│RELEASE_AST│  │
│  └─────┘  └──────┘  └────┘  └────┘  │CJO    │  └────┘  └─────┬─────┘  │
│                                       └───────┘                │         │
│                                                                ▼         │
│                                                    ┌────────────────┐    │
│                                                    │    CODEGEN     │    │
│                                                    │ (CHIR-driven)  │    │
│                                                    └───────┬────────┘    │
│                                                            ▼             │
│                                                    ┌────────────────┐    │
│                                                    │  SAVE_RESULTS  │    │
│                                                    │  (幂等/跳过)    │    │
│                                                    └────────────────┘    │
└──────────────────────────────────────────────────────────────────────────┘

RELEASE_AST 阶段内部执行顺序：
┌─────────────────────────────────────────────────────────────────┐
│ Phase 0: 前置操作                                                │
│   ├─ PrepareCodeGenInfo()         提取包名/空包标记              │
│   ├─ MaterializePackageDependence() 物化包依赖关系               │
│   ├─ MaterializeCachedASTReferences() 物化 Cache AST 引用       │
│   └─ [条件] PerformResultsSaving()  CHIR/增量模式提前保存        │
│                                                                   │
│ Phase 1: 清除 CHIR 层 AST 反向引用                               │
│   ├─ CHIRData::ClearASTTypeCache()                               │
│   ├─ DiagAdapter::posRange2MacroCallMap.clear()                  │
│   └─ ClearGlobalAnnotationInfo()                                 │
│                                                                   │
│ Phase 2: 清除 CompilerInstance 直接成员                           │
│   ├─ astPkg2chirPkgMap → chirPkgByName                           │
│   ├─ currentPkg, mangledName2DeclMap, rawMangleName2DeclMap      │
│   ├─ declsWithDuplicatedRawMangleName, directExtends, order      │
│   ├─ chirInfo.ccTys                                               │
│   └─ pkgs                                                         │
│                                                                   │
│ Phase 3: 清理/释放管理器                                          │
│   ├─ delete typeChecker → delete typeManager (顺序关键)          │
│   ├─ delete gim, delete testManager                              │
│   ├─ mangler.reset()                                              │
│   ├─ importManager.ClearASTReferences() (含 imported packages)   │
│   └─ packageManager->ClearASTReferences()                        │
│                                                                   │
│ Phase 4: 释放 AST 核心数据（严格顺序）                            │
│   ├─ srcPkgs.clear()    (先释放 AST 节点)                        │
│   └─ pkgCtxMap.clear()  (再释放 ASTContext)                      │
│                                                                   │
│ Phase 5: astReleased = true                                       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. 修订后的接口契约

```cpp
/**
 * @brief 释放 AST 阶段使用的内存。
 *
 * @pre  CHIR lowering 已完成
 * @pre  SAVE_CJO 已完成（标准模式）或将在此方法内完成（CHIR/增量模式）
 * @post srcPkgs 为空，pkgCtxMap 为空
 * @post typeChecker, typeManager, gim, testManager 已释放
 * @post mangler 已释放
 * @post importManager 中的 AST 引用已清除，imported packages 已释放
 * @post CompilationCache 中的 AST 引用已物化为字符串键
 * @post codeGenPkgInfos 已填充，CodeGen 可通过此结构驱动
 * @post packageDependenceResolved == true
 * @post IsASTReleased() 返回 true
 *
 * @invariant CHIR IR 节点（Package, Func, Block, Expression, Value）不受影响
 * @invariant CHIRData 中的 CHIR::Package 和 CHIR::Type 不受影响
 * @invariant DiagnosticEngine 不受影响（使用 SourceManager，非 AST）
 * @invariant SourceManager 不受影响
 * @invariant CompilerInvocation/GlobalOptions 不受影响
 *
 * @note 此方法是 virtual 的，子类可 override：
 *       - DefaultCompilerInstance: 标准行为
 *       - IncrementalCompilerInstance: 在释放前完成 AST 签名导出
 *       - LSP 子类: 可 override 为 no-op（保留 AST 供 IDE 使用）
 *
 * @note 通过 --no-release-ast 编译选项可禁用此方法
 */
virtual bool CompilerInstance::PerformASTRelease();
```

---

## 6. 修订后的关键修改文件清单

| 文件 | 修改类型 | 说明 | 关联缺陷 |
|------|---------|------|---------|
| `include/cangjie/Frontend/CompilerInstance.h` | 修改 | 新增 `ReleaseASTMemory()`, `PrepareCodeGenInfo()`, `MaterializePackageDependence()`, `PackageCodeGenInfo`, `astReleased`, `chirPkgByName`, `codeGenPkgInfos`, `packageDependenceResolved`, `resultsSaved`; `CompileStage` 新增 `RELEASE_AST`; `PerformASTRelease()` 声明为 virtual | D-01,D-02,D-11 |
| `src/Frontend/CompilerInstance.cpp` | 修改 | 实现 `ReleaseASTMemory()` 完整 5 阶段清理、`PrepareCodeGenInfo()`、`MaterializePackageDependence()`、`MaterializeCachedASTReferences()`、流水线注册、析构函数适配 | D-01~D-03,D-09,D-11 |
| `include/cangjie/FrontendTool/DefaultCompilerInstance.h` | 修改 | Override `PerformASTRelease()`; 新增 `CodegenOnePackageV2()`, `EmitLLVMSimilarBytecodeV2()`, `RearrangeImportedPackageDependenceV2()` | D-01 |
| `src/FrontendTool/DefaultCompilerInstance.cpp` | 修改 | 实现 CodeGen 双路径（AST/CHIR 驱动）、V2 方法 | D-01 |
| `include/cangjie/FrontendTool/IncrementalCompilerInstance.h` | 修改 | Override `PerformASTRelease()`; 清理 `changedTypes`, `declsToBeReCompiled` | D-05 |
| `src/FrontendTool/IncrementalCompilerInstance.cpp` | 修改 | 实现增量编译的 AST 释放前保存逻辑、`PerformResultsSaving()` 幂等性 | D-05 |
| `include/cangjie/IncrementalCompilation/CompilationCache.h` | 修改 | 新增 `materializedVarFuncDep`, `materializedSemaUsages` 数据结构 | D-09 |
| `include/cangjie/Modules/ImportManager.h` | 修改 | 新增 `ClearASTReferences()` 方法 | D-03,D-08 |
| `src/Modules/CjoManagerImpl.h` | 修改 | 新增 `ClearImportedPackages()`, `ClearPackagePointers()` | D-08 |
| `include/cangjie/Modules/PackageManager.h` | 修改 | 新增 `ClearASTReferences()` 方法 | D-03 |
| `include/cangjie/CHIR/IR/Type/CHIRType.h` | 修改 | 新增 `ClearASTTypeCache()` | D-03 |
| `include/cangjie/CHIR/Checker/AnnotationChecker.h` | 修改 | 暴露 `ClearGlobalAnnotationInfo()` 接口 | D-04 |
| `src/CHIR/Checker/AnnotationChecker.cpp` | 修改 | 实现 `ClearGlobalAnnotationInfo()` | D-04 |
| `include/cangjie/Frontend/CompilerInvocation.h` | 修改 | 新增 `--no-release-ast` 编译选项 | D-12 |

---

## 7. 修订后的风险矩阵

| 风险 | v1 评估 | v2 评估 | 变化原因 |
|------|---------|---------|---------|
| CodeGen 入口未重构导致无输出 | 极高（必现） | **已消除** | D-01 修复：双路径 CodeGen 驱动 |
| 遗漏 AST 指针导致 use-after-free | 高 | **低** | D-02/D-03 修复：完整 5 阶段 checklist |
| 增量编译缓存损坏 | 高 | **低** | D-05 修复：IncrementalCI override |
| 析构函数双重释放 | 中 | **已消除** | D-11 修复：astReleased 守卫 |
| imported packages 内存未释放 | 中 | **已消除** | D-08 修复：ClearImportedPackages |
| 物化键冲突 | 低 | **已消除** | D-09 修复：fallback + 断言 |
| DiagAdapter 宏诊断降级 | — | **低（可接受）** | D-06 修复：清理 map，降级为行号诊断 |
| 未知的 AST 引用遗漏 | 中 | **低** | ASan 全量测试 + Debug 守卫覆盖 |

---

## 8. 修订后的实施路线图

### Phase 1：基础设施与 CodeGen 重构（第 1 周）

1. `CompileStage` 枚举新增 `RELEASE_AST`
2. `CompilerInstance` 新增 `astReleased`、`PackageCodeGenInfo`、`chirPkgByName` 等基础设施
3. 新增 `--no-release-ast` 编译选项
4. **CodeGen 入口重构**（D-01 核心修复）：
   - 实现 `PrepareCodeGenInfo()`
   - 实现 `MaterializePackageDependence()`
   - 实现 `CodegenOnePackageV2()` 和 `EmitLLVMSimilarBytecodeV2()`
   - 实现 `RearrangeImportedPackageDependenceV2()`
5. 实现空的 `PerformASTRelease()` 框架，验证流水线集成

### Phase 2：完整清理实现（第 2 周）

1. 实现 `ReleaseASTMemory()` 完整 5 阶段清理逻辑
2. 实现 `MaterializeCachedASTReferences()`（含空键 fallback）
3. 实现管理器清理接口：
   - `ImportManager::ClearASTReferences()`
   - `CjoManagerImpl::ClearImportedPackages()`
   - `PackageManager::ClearASTReferences()`
   - `CHIRType::ClearASTTypeCache()`
   - `CHIR::ClearGlobalAnnotationInfo()`
4. 适配 `~CompilerInstance()` 析构函数
5. 适配 `DefaultCompilerInstance::PerformASTRelease()`

### Phase 3：增量编译适配与测试（第 3 周）

1. 实现 `IncrementalCompilerInstance::PerformASTRelease()`
2. `PerformResultsSaving()` 幂等性改造
3. 全量 grep 验证：`Ptr<.*AST::` 在 CHIR/CodeGen 目录中的残留引用
4. ASan/MSan 全量测试
5. 增量编译专项测试
6. 峰值内存对比测试
7. 现有测试套件全量回归

### Phase 4：加固与长期演进（第 4 周，可选）

1. `GetSourcePackages()` Release 构建保护
2. `AST_ACCESS_GUARD` 宏在关键路径添加
3. `ProfileRecorder` 记录 AST 释放耗时和内存量
4. 评估方案 B（选择性瘦身）作为 IDE/LSP 补充
5. 评估方案 C（Arena 分配器）作为长期架构目标

---

## 9. 缺陷修复追踪矩阵

| 缺陷 ID | 严重程度 | 优先级 | 修复状态 | 修复位置 | 验证方式 |
|---------|---------|--------|---------|---------|---------|
| D-01 | Critical | P0 | ✅ 已设计 | §1.1 CodeGen 入口重构 | 编译产出验证 + 集成测试 |
| D-02 | High | P0 | ✅ 已设计 | §1.2 完整清理 Checklist | ASan + Debug 守卫 |
| D-03 | Critical | P0 | ✅ 已设计 | §1.3 管理器清理策略 | ASan + 内存对比 |
| D-04 | Medium | P1 | ✅ 已设计 | §2.1 g_annoInfo 清理 | ASan |
| D-05 | High | P0 | ✅ 已设计 | §1.4 IncrementalCI 适配 | 增量编译专项测试 |
| D-06 | High | P1 | ✅ 已设计 | §2.2 DiagAdapter 清理 | ASan + 宏诊断测试 |
| D-07 | — | — | ❌ 驳回 | — | 评审方验证有 MANGLING 守卫 |
| D-08 | High | P1 | ✅ 已设计 | §2.3 / §1.3 imported packages | 内存对比测试 |
| D-09 | Medium | P1 | ✅ 已设计 | §2.4 空键 fallback | 增量编译 + 泛型测试 |
| D-10 | Low | P2 | ✅ 已设计 | §3.1 GetSourcePackages 保护 | 代码审查 |
| D-11 | Medium | P1 | ✅ 已设计 | §2.5 析构函数适配 | ASan + 退出测试 |
| D-12 | Low | P2 | ✅ 已设计 | §3.2 语义简单化 | 手动测试 |
| D-13 | Low | P3 | ⏸ 延后 | — | 长期基础设施改进 |
