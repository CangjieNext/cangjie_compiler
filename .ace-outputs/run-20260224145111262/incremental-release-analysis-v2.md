# AST 内存释放 — 补充分析 v2：TranslateToCHIR 过程中边翻译边释放的可行性

## 1. 反馈问题

> 能不能在 TranslateToCHIR 执行过程中，一边翻译一边释放？

## 2. 修订结论

**在函数体（`FuncBody::body`）粒度上可行**。

前一版分析的结论过于保守。经过更深入的代码审计，发现虽然 AST 声明节点（`FuncDecl`、`VarDecl` 等）在整个 lowering 过程中被反复引用，但函数体内部的代码块（`FuncBody::body: OwnedPtr<Block>`）和变量初始化表达式（`VarDeclAbstract::initializer: OwnedPtr<Expr>`）在翻译完成后不再被访问。

## 3. 关键代码结构

```
FuncDecl (声明节点 — 必须保留)
├── identifier, mangledName, ty, attrs  ← 签名信息，被 globalCache/AST2CHIRCheck 引用
├── funcBody: OwnedPtr<FuncBody>
│   ├── paramLists  ← 被 GlobalDeclAnalysis 引用（line 46-48）
│   ├── retType     ← 签名信息
│   └── body: OwnedPtr<Block>  ← 实际代码块，翻译后可释放 ✅
│       ├── statements
│       ├── expressions
│       └── nested blocks

VarDeclAbstract (声明节点 — 必须保留)
├── identifier, mangledName, ty, attrs  ← 签名信息
├── type: OwnedPtr<Type>  ← 类型信息，保留
└── initializer: OwnedPtr<Expr>  ← 初始化表达式，翻译后可释放 ✅
```

## 4. 逐步骤分析：函数体何时被读取

| 步骤 | 操作 | 是否读取 `funcBody->body` | 是否读取 `initializer` |
|------|------|--------------------------|----------------------|
| Step 1: CollectTopLevelDecls | 收集声明指针 | ❌ 不读取 | ❌ 不读取 |
| Step 2: SortGlobalVarDecl | 依赖分析 | ⚠️ 读取 `funcBody->paramLists`（非 body） | ✅ **读取** init 表达式内的引用 |
| Step 3: CacheSomeDeclsToGlobalSymbolTable | 创建签名 | ❌ 不读取 | ❌ 不读取 |
| Step 4a: TranslateNominalDecls | 翻译类型方法体 | ✅ **读取并翻译** | — |
| Step 4b: TranslateInitOfGlobalVars | 翻译全局变量初始化 | — | ✅ **读取并翻译** |
| Step 4c: TranslateTopLevelDecls | 翻译函数体 | ✅ **读取并翻译** | — |
| Step 5: AST2CHIRCheck | 验证 | ❌ 仅读取 `decl.ty`, `decl.identifier`, `decl.mangledName` | ❌ 仅读取 `decl.ty` |

**关键发现**：

1. `AST2CHIRCheck`（Step 5）**不读取函数体和初始化表达式**，只检查签名级信息（`CheckFunc` 在 `AST2CHIRChecker.cpp:481-507` 仅访问 `decl.outerDecl`、`decl.ty`、`decl.TestAttr()`）

2. `GlobalDeclAnalysis`（Step 2）读取 `funcBody->paramLists`（用于默认参数依赖分析，`GlobalDeclAnalysis.cpp:46-48`），但**不读取 `funcBody->body`**

3. Step 4 中翻译函数体时，`Translator::TranslateASTNode(*decl, trans)` 读取当前函数的 body，但通过 `globalCache` 引用其他函数时**只查找 CHIR 签名，不读取其他函数的 AST body**

## 5. 可行方案：函数体级增量释放（方案 A-3）

### 核心思路

在 `TranslateAllDecls`（Step 4）中，每翻译完一个函数体/变量初始化表达式，立即释放对应的 AST 子树。声明节点本身（`FuncDecl`、`VarDecl`）保留，因为 `globalCache`、`AST2CHIRCheck` 和 `CompilationCache` 仍需引用它们。

### 释放时机

```
Step 4a: TranslateNominalDecls(pkg)
  for each nominal decl (class/struct/enum):
    for each method in decl:
      Translator::TranslateASTNode(*method, trans)
      method->funcBody->body.reset()          ← 释放方法体 ✅

Step 4b: TranslateInitOfGlobalVars(pkg, initOrder)
  GlobalVarInitializer::Run()
    for each global var in initOrder:
      Translator::TranslateASTNode(*decl.initializer, trans)
      decl.initializer.reset()                ← 释放初始化表达式 ✅

Step 4c: TranslateTopLevelDecls (串行或并行)
  for each top-level func:
    Translator::TranslateASTNode(*decl, trans)
    decl.funcBody->body.reset()               ← 释放函数体 ✅
```

### 串行模式实现

```cpp
// AST2CHIR.cpp — TranslateAllDecls 修改
void AST2CHIR::TranslateAllDecls(const AST::Package& pkg, const InitOrder& initOrder)
{
    // step 1: translate nominal decls
    TranslateNominalDecls(pkg);
    // nominal decl 方法体在 TranslateNominalDecls 内部逐个释放

    // step 2: translate initialization of global var
    TranslateInitOfGlobalVars(pkg, initOrder);
    // 全局变量 initializer 在 GlobalVarInitializer 内部逐个释放

    // step 3: translate body of all funcs
    if (opts.GetJobs() > 1) {
        TranslateTopLevelDeclsInParallelWithRelease();  // 并行版本
    } else {
        for (auto decl : globalAndMemberFuncs) {
            if (!NeedTranslate(*decl)) { continue; }
            auto trans = CreateTranslator();
            trans.SetTopLevel(*decl);
            Translator::TranslateASTNode(*decl, trans);
            if (decl->TestAttr(AST::Attribute::GLOBAL)) {
                trans.CollectValueAnnotation(*decl);
            }
            // 翻译完成后立即释放函数体代码块
            ReleaseFuncBody(const_cast<AST::Decl&>(*decl));
        }
        for (auto decl : std::as_const(localConstFuncs.stableOrderValue)) {
            auto trans = CreateTranslator();
            trans.SetTopLevel(*decl);
            Translator::TranslateASTNode(*decl, trans);
            ReleaseFuncBody(const_cast<AST::Decl&>(*decl));
        }
    }
    // ...
}
```

```cpp
// AST2CHIR — 新增辅助方法
void AST2CHIR::ReleaseFuncBody(AST::Decl& decl)
{
    if (auto* funcDecl = DynamicCast<AST::FuncDecl*>(&decl)) {
        if (funcDecl->funcBody) {
            funcDecl->funcBody->body.reset();  // 释放 Block，保留 paramLists 和 retType
        }
    }
}

void AST2CHIR::ReleaseVarInitializer(AST::VarDeclAbstract& decl)
{
    decl.initializer.reset();  // 释放初始化表达式
}
```

### 并行模式实现

并行翻译时，每个线程翻译完自己的函数后释放该函数的 body。由于每个函数的 body 只被翻译它的线程访问，不存在竞争条件。

```cpp
void AST2CHIR::TranslateTopLevelDeclsInParallelWithRelease()
{
    // ParallelUtil 内部为每个 decl 创建独立的 Translator
    // 翻译完成后的回调中释放 body
    Utils::ParallelUtil allDeclsParallel(builder, opts.GetJobs());
    allDeclsParallel.RunAST2CHIRInParallelWithRelease(
        globalAndMemberFuncs, chirType, opts, gim, globalCache,
        localConstVars, localConstFuncs, kind, deserializedVals,
        Translator::TranslateASTNode, maybeUnreachable,
        isComputingAnnos, initFuncsForAnnoFactory, types, annoFactoryFuncs,
        // 新增：翻译完成后的释放回调
        [this](const AST::Decl& decl) {
            ReleaseFuncBody(const_cast<AST::Decl&>(decl));
        });
}
```

**线程安全分析**：
- 每个函数的 `funcBody->body` 只被翻译该函数的线程读取
- `body.reset()` 只修改当前函数的 `OwnedPtr`，不影响其他函数
- `globalCache` 是只读的（在 Step 3 已填充完毕），不受 body 释放影响
- `chirType.TranslateType()` 使用 `AST::Ty*` 作为 key，`Ty` 在声明节点上，不在 body 中

### 不能释放的部分

| AST 组件 | 能否释放 | 原因 |
|---------|---------|------|
| `FuncDecl` 节点本身 | ❌ | `globalCache` key、`AST2CHIRCheck` 引用 |
| `FuncDecl::funcBody` | ❌ | `GlobalDeclAnalysis` 访问 `paramLists` |
| `FuncBody::paramLists` | ❌ | `GlobalDeclAnalysis` 在 Step 2 访问 |
| `FuncBody::retType` | ❌ | 签名信息 |
| `FuncBody::body` (Block) | ✅ | 翻译后不再访问 |
| `VarDecl::initializer` | ✅ | 翻译后不再访问（Step 2 已完成依赖分析） |
| `VarDecl::type` | ❌ | 签名信息 |
| `LambdaExpr::funcBody->body` | ✅ | 翻译后不再访问 |

### 内存收益估算

函数体代码块（`Block` 及其子树：statements、expressions、nested blocks）通常占 AST 总内存的 **60-80%**。声明骨架（名称、类型签名、属性、参数列表）占 20-40%。

在 Step 4 过程中逐步释放 body，可以实现：
- **峰值内存降低**：不需要等到整个 lowering 完成，翻译一个释放一个
- **内存曲线更平滑**：AST 内存随翻译进度线性下降，而非最后一次性释放

```
内存
 ^
 |  ┌──────────────────┐
 |  │   AST (全量)      │ ← 当前：AST 存活到编译结束
 |  │                    │
 |  └──────────────────────────────────────────►
 |
 |  ┌──────────┐
 |  │ AST 全量  │╲
 |  │           │  ╲  ← 方案 A-3：边翻译边释放 body
 |  │           │    ╲
 |  │           │ 骨架 ╲──────────────────────►
 |  └──────────┘
 |  Step1-3    Step4                    CodeGen
```

## 6. 方案 A-3 与 v2 方案的组合

方案 A-3（函数体级增量释放）和 v2 方案（lowering 后完整释放）不是互斥的，而是互补的：

```
时间线：
  Step1-3          Step4                    CHIR完成后
  ─────────────────────────────────────────────────────►
  │ AST 全量存活   │ 逐步释放 body/init     │ 释放剩余骨架  │
  │                │ (方案 A-3)             │ (v2 方案)     │
  │                │ 释放 60-80% AST 内存   │ 释放剩余 20-40%│
```

**组合策略**：
1. Step 4 中：每翻译完一个函数体，立即 `funcBody->body.reset()`（方案 A-3）
2. Step 4 中：每翻译完一个全局变量初始化，立即 `initializer.reset()`（方案 A-3）
3. `PerformCHIRCompilation()` 末尾：调用 `ReleaseASTMemory()` 释放剩余的声明骨架、管理器、ASTContext（v2 方案）

**优势**：
- 峰值内存在 Step 4 过程中就开始下降，而非等到 CHIR 完成后
- 对于大型函数（如编译器自身的某些 2000+ 行函数），单个函数体释放就能回收可观内存
- v2 方案的完整清理 checklist 仍然适用，负责清理骨架和管理器

## 7. 风险分析

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 翻译后仍有代码路径访问 body | 低 | 高（crash） | ASan 测试 + `body.reset()` 后 `body` 为 null，`OwnedPtr` 解引用抛异常 |
| 并行翻译中释放时序问题 | 低 | 高（crash） | 每个函数的 body 只被一个线程访问，释放在翻译完成后 |
| `GlobalDeclAnalysis` 访问已释放的 body | 不可能 | — | Step 2 在 Step 4 之前执行，时序上不可能 |
| Lambda 表达式的 body 释放 | 中 | 中 | Lambda body 在翻译外层函数时一起翻译，外层函数翻译完成后可一起释放 |
| `annoFactoryFuncs` 的串行翻译 | 低 | 低 | 在并行翻译之后执行，同样可以逐个释放 |

**最大风险点**：`CollectDepInCallExpr`（`GlobalDeclAnalysis.cpp:46`）访问 `callExpr.resolvedFunction->funcBody->paramLists[0]->params[i]->desugarDecl`。这里访问的是 `funcBody->paramLists`，不是 `funcBody->body`。只要我们只释放 `body`（Block）而保留 `funcBody`（FuncBody 节点及其 paramLists），就是安全的。

## 8. 实现复杂度评估

| 维度 | 评估 |
|------|------|
| 代码改动量 | 小（~50 行，主要在 AST2CHIR.cpp 的 TranslateAllDecls 中） |
| 侵入性 | 低（只在翻译循环中添加 reset 调用，不改变翻译逻辑） |
| 可回退性 | 高（删除 reset 调用即可回退） |
| 测试策略 | ASan 全量测试 + 内存峰值对比 |
| 与 v2 方案兼容性 | 完全兼容（A-3 释放 body，v2 释放骨架） |

## 9. 最终建议

| 方案 | 释放时机 | 释放粒度 | 峰值内存收益 | 复杂度 | 建议 |
|------|---------|---------|------------|--------|------|
| A-3 | Step 4 中逐个释放 | `funcBody->body`, `initializer` | 60-80% AST | 低 | **推荐，优先实施** |
| v2 | CHIR 完成后一次性释放 | 整棵 AST + 管理器 | 90-95% AST | 中 | **推荐，与 A-3 组合** |
| A-3 + v2 组合 | Step 4 中 + CHIR 后 | 全部 | 95%+ AST | 中 | **最优方案** |

**推荐实施顺序**：
1. 先实施 A-3（改动小、风险低、立即见效）
2. 再实施 v2 的完整清理（释放剩余骨架和管理器）
3. A-3 可以在 1-2 天内完成，v2 需要 2-3 周
