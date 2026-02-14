# AST 阶段结束后内存释放 — 技术方案设计

## 1. 需求分析与约束梳理

### 1.1 需求背景

仓颉编译器当前的内存管理策略是"惰性释放"：AST 数据在整个编译生命周期内存活，直到 `CompilerInstance` 析构时才统一释放。对于大型项目，AST 阶段（Parse → Sema → GenericInstantiation → Mangling）结束后，AST 数据仍占据大量内存，而后续的 CHIR 和 CodeGen 阶段又会分配新的内存，导致峰值内存显著偏高。

**核心目标**：在 AST 阶段完成、CHIR lowering 结束后，尽早释放 AST 相关内存，降低编译器峰值内存占用。

### 1.2 AST 数据生命周期分析

通过代码分析，AST 数据的消费者和生命周期如下：

```
编译阶段                    AST 数据消费情况
─────────────────────────────────────────────────────────
PARSE                      创建 AST::Package, AST::File, AST::Decl 等节点
CONDITION_COMPILE          读写 AST 节点
IMPORT_PACKAGE             读写 AST 节点，创建 imported packages
MACRO_EXPAND               读写 AST 节点，展开宏
AST_DIFF                   读 AST 节点（增量编译）
SEMA                       读写 AST 节点，填充类型信息，构建 ASTContext
DESUGAR_AFTER_SEMA         读写 AST 节点
GENERIC_INSTANTIATION      读写 AST 节点，创建实例化声明
OVERFLOW_STRATEGY          读写 AST 节点
MANGLING                   读 AST 节点，写入 mangledName
SAVE_CJO                   读 AST 节点（序列化）
─── AST 阶段结束分界线 ───
CHIR                       读 AST 节点（AST2CHIR lowering），创建 CHIR IR
CODEGEN                    仅读 CHIR IR（不直接访问 AST）
SAVE_RESULTS               读 AST 节点（仅 CHIR 输出模式下保存 CJO）
```

### 1.3 跨阶段 AST 引用分析（关键约束）

| 引用位置 | 引用类型 | 生命周期需求 | 可解耦性 |
|---------|---------|------------|---------|
| `AST2CHIR` 类内部 | `Ptr<AST::Decl>`, `AST::Package&` 等 | 仅 CHIR lowering 期间 | 天然临时，lowering 结束即可释放 |
| `CHIRTypeCache::typeMap` | `AST::Ty* → CHIR::Type*` | CHIR lowering 期间 | lowering 后可清除 map，CHIR::Type 独立存活 |
| `AST2CHIRNodeMap` | `const AST::Node* → T*` | CHIR lowering 期间 | lowering 后可清除 |
| `CompilationCache::varAndFuncDep` | `Ptr<const AST::Decl>` | 增量编译缓存写入 | 需在 SAVE_CJO 前完成序列化 |
| `CompilationCache::semaInfo.usages` | `Ptr<const AST::Decl>` | 增量编译缓存写入 | 需在 SAVE_CJO 前完成序列化 |
| `astPkg2chirPkgMap` | `Ptr<AST::Package> → Ptr<CHIR::Package>` | CodeGen 阶段 | 可替换为 string key |
| `CompilerInstance::srcPkgs` | `OwnedPtr<AST::Package>` | 所有权根 | 释放此处即释放整棵 AST |
| `CompilerInstance::pkgCtxMap` | `unique_ptr<ASTContext>` | 符号表、类型缓存 | 依赖 AST 节点指针 |
| `CompilerInstance::pkgs` | `Ptr<AST::Package>` | 非所有权引用 | 随 srcPkgs 释放而悬空 |
| CHIR IR 节点 (`Value`, `Expression` 等) | **无 AST 引用** | — | CHIR IR 使用独立的 `DebugLocation` |
| CodeGen (`EmitLLVMSimilarBytecode`) | 仅通过 `CHIR::Package` | — | 不依赖 AST |

**关键发现**：
1. CHIR IR 节点本身**不持有任何 AST 指针**，使用独立的 `CHIR::DebugLocation` 和 `CHIR::Position`
2. CodeGen 阶段**仅通过 CHIRData 访问 CHIR IR**，不直接访问 AST
3. AST 引用集中在 `AST2CHIR` 翻译层（临时）和 `CompilationCache`（需序列化）
4. `SAVE_CJO` 阶段需要读 AST 进行序列化，但在标准编译流中排在 CHIR 之前

### 1.4 析构顺序约束

```cpp
// CompilerInstance::~CompilerInstance() 中的关键注释：
// AST must be released before ASTContext for correct symbol detaching.
srcPkgs.clear();    // 先释放 AST 节点
pkgCtxMap.clear();  // 再释放 ASTContext
```

此约束在任何 AST 释放方案中都必须遵守。

---

## 2. 备选方案

### 2.1 编译流水线总览

```
┌──────────────────────────────────────────────────────────────────┐
│                    CompilerInstance::Compile()                     │
│                                                                    │
│  ┌─────┐  ┌──────┐  ┌──────┐  ┌────┐  ┌────┐  ┌───────┐  ┌───┐ │
│  │PARSE├─►│IMPORT├─►│MACRO ├─►│SEMA├─►│MANG├─►│SAVE   ├─►│CHI│ │
│  │     │  │      │  │      │  │    │  │    │  │CJO    │  │R  │ │
│  └─────┘  └──────┘  └──────┘  └────┘  └────┘  └───────┘  └─┬─┘ │
│                                                               │   │
│                          ▼ AST 释放点 ▼                       │   │
│                                                               ▼   │
│                                                    ┌────────────┐ │
│                                                    │  CODEGEN   │ │
│                                                    └────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

---

### 方案 A：CHIR Lowering 后显式释放 AST（推荐方案）

#### 核心思路

在 `PerformCHIRCompilation()` 完成后，插入一个显式的 AST 释放阶段。在释放前，确保所有跨阶段引用已被解耦或序列化。

#### 架构图

```
SAVE_CJO ──► CHIR Lowering ──► ReleaseAST() ──► CODEGEN ──► SAVE_RESULTS
                  │                   │
                  │                   ├─ 清除 CHIRTypeCache::typeMap
                  │                   ├─ 清除 AST2CHIRNodeMap（已随 AST2CHIR 析构）
                  │                   ├─ 序列化 CompilationCache 中的 AST 引用
                  │                   ├─ 替换 astPkg2chirPkgMap 为 string-keyed map
                  │                   ├─ srcPkgs.clear()（释放 AST 节点）
                  │                   └─ pkgCtxMap.clear()（释放 ASTContext）
                  │
                  └─ AST2CHIR 是栈上临时对象，lowering 结束自动析构
```

#### 详细设计

**1. 新增 `CompilerInstance::ReleaseASTMemory()` 方法**

```cpp
// CompilerInstance.h
class CompilerInstance {
public:
    // ...
    void ReleaseASTMemory();
    bool IsASTReleased() const { return astReleased; }

private:
    bool astReleased = false;
};
```

```cpp
// CompilerInstance.cpp
void CompilerInstance::ReleaseASTMemory()
{
    if (astReleased) {
        return;
    }

    // 1. 序列化 CompilationCache 中的 AST 引用为字符串键
    MaterializeCachedASTReferences();

    // 2. 清除 CHIRTypeCache 中的 AST::Ty* 映射
    //    CHIR::Type* 由 CHIRContext 管理，不受影响
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    chirData.ClearASTReferences();
#endif

    // 3. 替换 astPkg2chirPkgMap 为 string-keyed 版本
    for (auto& [astPkg, chirPkg] : astPkg2chirPkgMap) {
        chirPkgByName[astPkg->fullPackageName] = chirPkg;
    }
    astPkg2chirPkgMap.clear();

    // 4. 清除非所有权引用
    pkgs.clear();

    // 5. 按正确顺序释放 AST（先 AST 节点，后 ASTContext）
    srcPkgs.clear();
    pkgCtxMap.clear();

    // 6. 释放不再需要的管理器
    delete typeChecker;
    typeChecker = nullptr;
    delete gim;
    gim = nullptr;

    astReleased = true;
}
```

**2. 在编译流水线中插入释放点**

```cpp
// CompilerInstance.cpp - InitCompilerInstance()
performMap.insert_or_assign(CompileStage::CHIR, &CompilerInstance::PerformCHIRCompilation);
// 新增：CHIR 后释放 AST
performMap.insert_or_assign(CompileStage::RELEASE_AST, &CompilerInstance::PerformASTRelease);
performMap.insert_or_assign(CompileStage::CODEGEN, &CompilerInstance::PerformCodeGen);
```

**3. CompilationCache AST 引用物化**

```cpp
void CompilerInstance::MaterializeCachedASTReferences()
{
    // 将 varAndFuncDep 中的 Ptr<AST::Decl> 转换为 RawMangledName
    MaterializedVarFuncDep matDep;
    for (auto& [decl, deps] : cachedInfo.varAndFuncDep) {
        std::vector<RawMangledName> depNames;
        for (auto& dep : deps) {
            depNames.push_back(dep->rawMangledName);
        }
        matDep.emplace_back(decl->rawMangledName, std::move(depNames));
    }
    cachedInfo.varAndFuncDep.clear();
    cachedInfo.materializedVarFuncDep = std::move(matDep);

    // 将 semaInfo.usages 中的 Ptr<AST::Decl> key 转换为 RawMangledName
    std::unordered_map<RawMangledName, SemaUsage> matUsages;
    for (auto& [decl, usage] : cachedInfo.semaInfo.usages) {
        matUsages[decl->rawMangledName] = std::move(usage);
    }
    cachedInfo.semaInfo.usages.clear();
    cachedInfo.semaInfo.materializedUsages = std::move(matUsages);
}
```

**4. CHIRData 清除 AST 引用**

```cpp
// CHIRData 新增方法
void CHIRData::ClearASTReferences()
{
    // typeMap 中 key 是 AST::Ty*，value 是 CHIR::Type*
    // CHIR::Type* 由 CHIRContext 的 constAllocatedTys/dynamicAllocatedTys 管理
    // 清除 map 不影响 CHIR::Type 的生命周期
    // 注意：需要在 AST2CHIR 的 CHIRType 实例析构后调用
}
```

#### 优势

- **内存收益最大化**：AST 节点树、ASTContext 符号表、InvertedIndex、TypeChecker、GIM 全部释放
- **改动集中**：主要修改 `CompilerInstance`，不侵入 CHIR/CodeGen 内部
- **安全性高**：CHIR IR 节点不持有 AST 引用，CodeGen 仅依赖 CHIR，释放 AST 不影响后续阶段
- **可验证**：通过 `IsASTReleased()` 守卫，后续阶段误访问 AST 会在开发期被捕获

#### 劣势

- **CompilationCache 需要重构**：`varAndFuncDep` 和 `semaInfo.usages` 的 key 从 `Ptr<AST::Decl>` 改为 `RawMangledName`，影响增量编译的序列化/反序列化逻辑
- **SAVE_CJO 阶段约束**：CJO 序列化必须在 AST 释放前完成（当前流水线已满足：SAVE_CJO 在 CHIR 之前）
- **SAVE_RESULTS 特殊处理**：CHIR 输出模式下 `PerformResultsSaving` 需要保存 CJO，需将此逻辑提前到 AST 释放前
- **调试信息**：AST 释放后，CHIR/CodeGen 阶段的错误诊断无法回溯到 AST 节点（但 CHIR 有独立的 DebugLocation）

#### 适用场景

- 标准编译流（`DefaultCompilerInstance`）
- 大型项目编译，内存压力大
- 非 IDE/LSP 场景

---

### 方案 B：AST 节点选择性释放 + 骨架保留

#### 核心思路

不完全释放 AST，而是在 CHIR lowering 后对 AST 节点进行"瘦身"：释放函数体、表达式树等大体积子树，仅保留声明骨架（名称、类型签名、mangledName）。这样 `CompilationCache` 中的 `Ptr<AST::Decl>` 仍然有效，同时释放了大部分内存。

#### 架构图

```
CHIR Lowering 完成后：

AST::Package                    AST::Package (瘦身后)
├── File                        ├── File
│   ├── ClassDecl               │   ├── ClassDecl (保留签名)
│   │   ├── FuncDecl            │   │   ├── FuncDecl (保留签名)
│   │   │   ├── body ────────►  │   │   │   ├── body = nullptr  ← 释放
│   │   │   ├── params          │   │   │   ├── params (保留)
│   │   │   └── returnType      │   │   │   └── returnType (保留)
│   │   └── VarDecl             │   │   └── VarDecl (保留签名)
│   │       └── initExpr ────►  │   │       └── initExpr = nullptr ← 释放
│   └── FuncDecl                │   └── FuncDecl (保留签名)
│       └── body ────────────►  │       └── body = nullptr ← 释放
└── genericInstantiatedDecls    └── genericInstantiatedDecls (保留签名)
```

#### 详细设计

**1. AST 节点瘦身 Walker**

```cpp
// ASTSlimmer.h
class ASTSlimmer {
public:
    explicit ASTSlimmer(AST::Package& pkg) : pkg(pkg) {}

    void Slim()
    {
        for (auto& file : pkg.files) {
            for (auto& decl : file->decls) {
                SlimDecl(*decl);
            }
        }
        for (auto& decl : pkg.genericInstantiatedDecls) {
            SlimDecl(*decl);
        }
    }

private:
    void SlimDecl(AST::Decl& decl);
    void SlimFuncDecl(AST::FuncDecl& func);
    void SlimClassDecl(AST::ClassDecl& cls);
    // ... 其他声明类型

    AST::Package& pkg;
};
```

```cpp
void ASTSlimmer::SlimFuncDecl(AST::FuncDecl& func)
{
    // 释放函数体（最大的内存消费者）
    func.body.reset();  // OwnedPtr::reset() 释放子树

    // 释放默认参数表达式
    for (auto& param : func.params) {
        param->defaultValue.reset();
    }

    // 保留：name, mangledName, ty, rawMangledName, attrs, modifiers
}
```

**2. ASTContext 部分清理**

```cpp
void ASTContext::SlimDown()
{
    // 清除类型检查缓存（大量 AST::Node* → Ty* 映射）
    typeCheckCache.clear();

    // 清除倒排索引（大量字符串 → Symbol* 映射）
    invertedIndex.Reset();

    // 保留 declMap（声明查找仍可用）
    // 保留 enumConstructors（签名级信息）
}
```

**3. 在编译流水线中插入瘦身点**

```cpp
bool CompilerInstance::PerformASTSlimming()
{
    for (auto& srcPkg : srcPkgs) {
        ASTSlimmer slimmer(*srcPkg);
        slimmer.Slim();
    }
    for (auto& [pkg, ctx] : pkgCtxMap) {
        ctx->SlimDown();
    }
    // TypeChecker 和 GIM 可以释放
    delete typeChecker;
    typeChecker = nullptr;
    delete gim;
    gim = nullptr;
    return true;
}
```

#### 优势

- **向后兼容性最好**：`Ptr<AST::Decl>` 指针仍然有效，`CompilationCache` 无需重构
- **渐进式实施**：可以逐步增加瘦身的节点类型，每次释放更多内存
- **安全性高**：不会产生悬空指针，声明骨架始终可访问
- **调试友好**：保留的声明骨架可用于错误诊断中的符号名称解析
- **增量编译兼容**：`cachedInfo.varAndFuncDep` 等引用不受影响

#### 劣势

- **内存收益有限**：声明骨架（名称、类型签名、属性）仍占用内存，预估仅释放 50-70% 的 AST 内存
- **实现复杂度高**：需要为每种 AST 节点类型实现瘦身逻辑，容易遗漏
- **不变量维护困难**：瘦身后的 AST 处于"半有效"状态，后续代码如果意外访问已释放的子树会导致空指针异常
- **测试负担重**：需要验证每种节点类型的瘦身不会破坏后续阶段

#### 适用场景

- 需要保持 `CompilationCache` 完全兼容的场景
- 渐进式优化，先释放最大的子树（函数体），后续迭代再释放更多
- IDE/LSP 场景下可能需要保留部分 AST 信息

---

### 方案 C：Arena 分配器重构 + 阶段性批量释放

#### 核心思路

将 AST 节点的内存分配从当前的 `new/delete`（通过 `OwnedPtr`）迁移到 Arena 分配器。每个编译阶段使用独立的 Arena，阶段结束时整个 Arena 一次性释放，无需逐节点析构。

#### 架构图

```
┌─────────────────────────────────────────────────────────┐
│                   CompilerInstance                        │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────┐ │
│  │  AST Arena    │  │  CHIR Arena   │  │ CodeGen Arena  │ │
│  │              │  │              │  │               │ │
│  │ Package      │  │ CHIR::Package│  │ llvm::Module  │ │
│  │ File         │  │ Func         │  │               │ │
│  │ Decl         │  │ Block        │  │               │ │
│  │ Expr         │  │ Expression   │  │               │ │
│  │ ASTContext   │  │ Value        │  │               │ │
│  │ Symbol       │  │              │  │               │ │
│  └──────┬───────┘  └──────────────┘  └────────────────┘ │
│         │                                                 │
│         ▼                                                 │
│   CHIR lowering 后                                        │
│   astArena.Reset()  ← 一次性释放所有 AST 内存              │
└─────────────────────────────────────────────────────────┘
```

#### 详细设计

**1. Arena 分配器**

```cpp
// ASTArena.h
class ASTArena {
public:
    ASTArena(size_t blockSize = 4 * 1024 * 1024)  // 4MB blocks
        : blockSize(blockSize) {}

    ~ASTArena() { Reset(); }

    // 分配内存，不调用析构函数
    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t))
    {
        size_t aligned = (currentOffset + alignment - 1) & ~(alignment - 1);
        if (aligned + size > blockSize || blocks.empty()) {
            AllocateNewBlock(std::max(size, blockSize));
            aligned = 0;
        }
        void* ptr = static_cast<char*>(blocks.back()) + aligned;
        currentOffset = aligned + size;
        totalAllocated += size;
        return ptr;
    }

    // 一次性释放所有内存
    void Reset()
    {
        for (auto* block : blocks) {
            ::operator delete(block);
        }
        blocks.clear();
        currentOffset = 0;
        totalAllocated = 0;
    }

    size_t GetTotalAllocated() const { return totalAllocated; }

private:
    void AllocateNewBlock(size_t minSize);

    std::vector<void*> blocks;
    size_t blockSize;
    size_t currentOffset = 0;
    size_t totalAllocated = 0;
};
```

**2. AST 节点使用 Arena 分配**

```cpp
// Node.h 修改
struct Node {
    // 重载 operator new 使用 Arena
    void* operator new(size_t size, ASTArena& arena)
    {
        return arena.Allocate(size);
    }

    // 禁止普通 delete（Arena 统一管理）
    void operator delete(void* ptr) noexcept
    {
        // no-op: Arena 统一释放
    }

    // placement delete for exception safety
    void operator delete(void* ptr, ASTArena& arena) noexcept
    {
        // no-op
    }

    // ... 现有成员
};
```

**3. OwnedPtr 适配 Arena 模式**

```cpp
// SafePointer.h 修改
template <typename T>
class OwnedPtr {
public:
    // 新增：Arena 模式标记
    enum class OwnershipMode { HEAP, ARENA };

    ~OwnedPtr()
    {
        if (mode == OwnershipMode::HEAP) {
            delete origin;
        }
        // ARENA 模式不 delete，由 Arena 统一释放
    }

private:
    T* origin = nullptr;
    OwnershipMode mode = OwnershipMode::HEAP;
};
```

**4. 编译流水线集成**

```cpp
class CompilerInstance {
    ASTArena astArena;  // AST 专用 Arena

    bool PerformASTRelease()
    {
        // Arena 模式：一次性释放，无需逐节点析构
        // 但需要先清除所有指向 Arena 内存的外部引用
        MaterializeCachedASTReferences();
        pkgs.clear();
        srcPkgs.clear();  // OwnedPtr 在 ARENA 模式下不 delete
        pkgCtxMap.clear();
        astArena.Reset();  // 一次性释放所有 AST 内存
        return true;
    }
};
```

#### 优势

- **释放性能最优**：O(N) 块释放 vs O(M) 节点析构（M >> N），无逐节点析构开销
- **内存碎片最少**：Arena 连续分配，内存局部性好，编译期间 cache 友好
- **内存收益 100%**：Arena Reset 释放所有 AST 内存，无残留
- **符合编译器最佳实践**：LLVM、Clang、Swift 编译器均使用 Arena/BumpPtrAllocator

#### 劣势

- **重构工作量巨大**：需要修改所有 AST 节点的创建方式（`new Node(...)` → `new(arena) Node(...)`）
- **OwnedPtr 语义变化**：Arena 模式下 `OwnedPtr` 不再负责释放，需要修改或替换整个智能指针体系
- **std 容器兼容性**：AST 节点内部的 `std::vector<OwnedPtr<T>>` 等容器需要自定义分配器或改用 Arena 感知的容器
- **析构函数问题**：Arena 不调用析构函数，AST 节点中持有的 `std::string`、`std::vector` 等 RAII 资源会泄漏，需要额外的析构注册机制
- **向后兼容性差**：几乎所有创建 AST 节点的代码都需要修改
- **风险极高**：涉及编译器核心数据结构的根本性重构

#### 适用场景

- 长期架构演进目标
- 新编译器项目从零开始设计
- 对编译性能有极致要求的场景

---

## 3. 方案对比

| 维度 | 方案 A：显式释放 | 方案 B：选择性瘦身 | 方案 C：Arena 重构 |
|------|----------------|------------------|------------------|
| **内存收益** | ~90-95% AST 内存 | ~50-70% AST 内存 | ~100% AST 内存 |
| **实现复杂度** | 中等 | 中等 | 极高 |
| **改动范围** | CompilerInstance + CompilationCache | AST 节点 + ASTContext | 全局（所有 AST 创建点） |
| **向后兼容性** | 中（需重构 Cache 引用） | 高（指针仍有效） | 低（根本性重构） |
| **风险等级** | 中 | 低 | 极高 |
| **增量编译兼容** | 需适配 | 天然兼容 | 需适配 |
| **IDE/LSP 兼容** | 需条件守卫 | 天然兼容 | 需条件守卫 |
| **释放性能** | O(M) 节点析构 | O(K) 子树析构 | O(N) 块释放 |
| **实施周期** | 2-3 周 | 1-2 周 | 2-3 月 |
| **可测试性** | 高（明确的释放点） | 中（半有效状态） | 高（Arena 统计） |
| **编译器最佳实践** | 常见 | 少见 | 业界标准 |

---

## 4. 推荐方案及详细设计

### 推荐：方案 A（CHIR Lowering 后显式释放 AST）

**理由**：
- 内存收益接近最优（90-95%），性价比最高
- 改动范围可控，不侵入 CHIR/CodeGen 核心逻辑
- 基于代码分析的关键发现：CHIR IR 节点不持有 AST 引用，CodeGen 仅依赖 CHIR
- 可在 2-3 周内完成实施和验证

### 4.1 模块划分与接口设计

#### 新增/修改的模块

```
CompilerInstance (修改)
├── ReleaseASTMemory()          // 新增：AST 释放入口
├── MaterializeCachedASTRefs()  // 新增：Cache 引用物化
├── PerformASTRelease()         // 新增：流水线阶段函数
├── IsASTReleased()             // 新增：状态查询
│
CompileStage (修改)
├── RELEASE_AST                 // 新增：AST 释放阶段枚举
│
CHIRData (修改)
├── ClearASTTypeCache()         // 新增：清除 AST→CHIR 类型映射
│
CompilationCache (修改)
├── MaterializedVarFuncDep      // 新增：物化后的依赖关系
├── MaterializedSemaUsages      // 新增：物化后的语义使用信息
│
ASTReleaseGuard (新增)
└── 开发期断言守卫，防止释放后误访问
```

#### 接口契约

```cpp
/**
 * @brief 释放 AST 阶段使用的内存。
 *
 * @pre CHIR lowering 已完成（PerformCHIRCompilation 返回 true）
 * @pre SAVE_CJO 已完成（AST 序列化已写入磁盘）
 * @post srcPkgs 为空，pkgCtxMap 为空
 * @post typeChecker 和 gim 已释放
 * @post CompilationCache 中的 AST 引用已物化为字符串键
 * @post IsASTReleased() 返回 true
 *
 * @invariant CHIR IR 节点不受影响
 * @invariant CHIRData 中的 CHIR::Package 和 CHIR::Type 不受影响
 * @invariant DiagnosticEngine 不受影响（使用 SourceManager，非 AST）
 */
void CompilerInstance::ReleaseASTMemory();
```

### 4.2 数据流与状态转换

```
                    ┌─────────────────────────────────────┐
                    │       CompilerInstance 状态机         │
                    │                                       │
                    │  INIT ──► AST_ALIVE ──► AST_RELEASED │
                    │            │                │         │
                    │            │ 可访问:         │ 可访问:  │
                    │            │ · srcPkgs      │ · chirData│
                    │            │ · pkgCtxMap    │ · cachedInfo│
                    │            │ · typeChecker  │   (物化版) │
                    │            │ · gim          │ · diag    │
                    │            │ · chirData     │ · sm      │
                    │            │ · cachedInfo   │          │
                    │            │                │ 不可访问:  │
                    │            │                │ · srcPkgs │
                    │            │                │ · pkgCtxMap│
                    │            │                │ · typeChecker│
                    └─────────────────────────────────────┘
```

### 4.3 CompilationCache 物化策略

当前 `CompilationCache` 中持有 AST 指针的字段及物化方案：

| 字段 | 当前类型 | 物化后类型 | 物化时机 |
|------|---------|-----------|---------|
| `varAndFuncDep` | `vector<pair<Ptr<Decl>, vector<Ptr<Decl>>>>` | `vector<pair<string, vector<string>>>` | ReleaseAST 前 |
| `semaInfo.usages` | `unordered_map<Ptr<Decl>, SemaUsage>` | `unordered_map<string, SemaUsage>` | ReleaseAST 前 |

物化使用 `Decl::rawMangledName` 作为字符串键，该字段在 Mangling 阶段已填充，且在增量编译的序列化/反序列化中已作为标识符使用。

### 4.4 SAVE_RESULTS 特殊处理

在 CHIR 输出模式（`OutputMode::CHIR`）下，`PerformResultsSaving` 需要保存 CJO（读 AST）。解决方案：

```cpp
bool CompilerInstance::PerformASTRelease()
{
    // CHIR 输出模式下，先执行 CJO 保存，再释放 AST
    if (invocation.globalOptions.outputMode == GlobalOptions::OutputMode::CHIR) {
        PerformResultsSaving();  // 提前保存
        resultsSaved = true;
    }
    ReleaseASTMemory();
    return true;
}

bool CompilerInstance::PerformResultsSaving()
{
    if (resultsSaved) {
        return true;  // 已在 AST 释放前保存
    }
    // ... 原有逻辑
}
```

### 4.5 DefaultCompilerInstance 适配

```cpp
// DefaultCompilerInstance 需要 override PerformASTRelease
bool DefaultCompilerInstance::PerformASTRelease()
{
    // CHIR 输出模式下提前保存 CJO
    if (invocation.globalOptions.outputMode == GlobalOptions::OutputMode::CHIR) {
        impl->PerformResultsSaving();
        resultsSaved = true;
    }
    ReleaseASTMemory();
    return true;
}
```

### 4.6 开发期安全守卫

```cpp
// ASTReleaseGuard.h - 仅在 Debug 构建中生效
#ifndef NDEBUG
#define AST_ACCESS_GUARD(ci) \
    CJC_ASSERT(!ci.IsASTReleased() && "Accessing AST after release!")
#else
#define AST_ACCESS_GUARD(ci) ((void)0)
#endif

// 在所有访问 AST 的公共方法中添加守卫
ASTContext* CompilerInstance::GetASTContextByPackage(Ptr<AST::Package> pkg) const
{
    AST_ACCESS_GUARD(*this);
    // ... 原有逻辑
}
```

---

## 5. 非功能需求分析

### 5.1 编译性能影响

| 指标 | 影响评估 | 说明 |
|------|---------|------|
| 编译时间 | 微增（<1%） | `ReleaseASTMemory()` 的开销主要是逐节点析构和 Cache 物化，相比整体编译时间可忽略 |
| 峰值内存 | 显著降低（预估 30-50%） | AST 通常占编译器总内存的 40-60%，释放后 CHIR+CodeGen 阶段峰值大幅下降 |
| 内存碎片 | 可能增加 | 大量小对象释放后可能产生碎片，但后续 CHIR/CodeGen 分配可复用 |

### 5.2 诊断信息质量

- **AST 释放前的阶段**：诊断信息不受影响
- **CHIR 阶段**：CHIR IR 使用独立的 `DebugLocation`（包含 fileID、行列号、文件路径），不依赖 AST 节点，诊断信息完整
- **CodeGen 阶段**：通过 CHIR 的 `DebugLocation` 生成 LLVM debug info，不受影响
- **ICE（Internal Compiler Error）**：AST 释放后如果发生 ICE，crash dump 中无法打印 AST 节点信息。缓解措施：在 `ReleaseASTMemory()` 前记录关键 AST 统计信息（节点数、包名等）

### 5.3 增量编译支持

- `CompilationCache` 的物化是关键适配点
- 物化后的 `materializedVarFuncDep` 和 `materializedSemaUsages` 使用 `RawMangledName` 作为键，与现有的序列化格式（`WriteCache`）兼容
- `ASTCache`（`curPkgASTCache`, `importedASTCache`）已经使用 `RawMangledName` 作为键，不受影响
- `cachedInfo.semaInfo.relations` 和 `builtInTypeRelations` 已经使用 `RawMangledName`，不受影响

### 5.4 可测试性

- **单元测试**：新增 `ReleaseASTMemory()` 的单元测试，验证释放后状态正确
- **集成测试**：在现有编译测试中启用 AST 释放，验证编译结果不变
- **内存测试**：使用 ASan/MSan 验证无 use-after-free
- **性能测试**：对比释放前后的峰值内存和编译时间
- **回归测试**：确保所有现有测试在启用 AST 释放后仍通过

### 5.5 可调试性

- 新增编译选项 `--no-release-ast` 禁用 AST 释放，方便调试
- `ProfileRecorder` 记录 AST 释放的耗时和释放的内存量
- Debug 构建中的 `AST_ACCESS_GUARD` 宏帮助定位误访问

---

## 6. 风险评估与缓解策略

| 风险 | 概率 | 影响 | 缓解策略 |
|------|------|------|---------|
| CHIR/CodeGen 中存在未发现的 AST 引用 | 中 | 高（crash） | 1. 全量 grep 验证 2. ASan 测试 3. Debug 守卫宏 4. `--no-release-ast` 回退开关 |
| CompilationCache 物化遗漏字段 | 低 | 中（增量编译错误） | 1. 静态分析 Cache 结构中所有 AST 指针 2. 增量编译专项测试 |
| SAVE_RESULTS 时序问题 | 低 | 中（CHIR 模式下 CJO 丢失） | 1. 在 AST 释放前显式检查并提前保存 2. 单元测试覆盖 |
| IDE/LSP 子类依赖 AST | 中 | 中（LSP 功能异常） | 1. `ReleaseASTMemory()` 设为 virtual 2. LSP 子类可 override 为 no-op 3. 通过 `IsASTReleased()` 守卫 |
| 析构顺序错误导致 crash | 低 | 高（crash） | 1. 严格遵循 srcPkgs → pkgCtxMap 顺序 2. 复用现有析构函数的顺序逻辑 |
| 第三方插件（MetaTransform）依赖 AST | 低 | 中 | 1. 插件在 CHIR 前执行，不受影响 2. 文档说明 AST 生命周期变化 |

---

## 7. 实施路线图

### Phase 1：基础设施（第 1 周）

1. 在 `CompileStage` 枚举中新增 `RELEASE_AST`（位于 `CHIR` 和 `CODEGEN` 之间）
2. 在 `CompilerInstance` 中新增 `astReleased` 状态标记和 `IsASTReleased()` 查询方法
3. 新增 `--no-release-ast` 编译选项
4. 实现 `AST_ACCESS_GUARD` 宏并在关键 AST 访问点添加守卫
5. 实现空的 `PerformASTRelease()` 框架（默认 no-op），验证流水线集成

### Phase 2：核心实现（第 2 周）

1. 实现 `CompilationCache` 物化逻辑（`MaterializeCachedASTReferences()`）
2. 实现 `CHIRData::ClearASTTypeCache()`
3. 实现 `astPkg2chirPkgMap` → `chirPkgByName` 替换
4. 实现完整的 `ReleaseASTMemory()` 方法
5. 处理 `SAVE_RESULTS` 在 CHIR 输出模式下的时序问题
6. 适配 `DefaultCompilerInstance`

### Phase 3：测试与验证（第 3 周）

1. 全量 grep 验证 CHIR/CodeGen 中无遗漏的 AST 引用
2. ASan/MSan 全量测试
3. 增量编译专项测试
4. 峰值内存对比测试
5. 编译时间回归测试
6. 现有测试套件全量回归

### Phase 4：长期演进（可选）

1. 评估方案 C（Arena 分配器）的可行性，作为下一代内存管理架构
2. 将方案 B（选择性瘦身）作为 IDE/LSP 场景的补充策略
3. 探索 AST 节点的 COW（Copy-on-Write）机制，支持增量编译场景下的 AST 共享

---

## 8. 关键修改文件清单

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `include/cangjie/Frontend/CompilerInstance.h` | 修改 | 新增 `ReleaseASTMemory()`, `astReleased`, `chirPkgByName`, `RELEASE_AST` 枚举 |
| `src/Frontend/CompilerInstance.cpp` | 修改 | 实现释放逻辑、物化逻辑、流水线注册 |
| `include/cangjie/IncrementalCompilation/CompilationCache.h` | 修改 | 新增物化后的数据结构 |
| `include/cangjie/FrontendTool/DefaultCompilerInstance.h` | 修改 | Override `PerformASTRelease()` |
| `src/FrontendTool/DefaultCompilerInstance.cpp` | 修改 | 实现 CHIR 模式下的提前保存 |
| `include/cangjie/Frontend/CompilerInvocation.h` | 修改 | 新增 `--no-release-ast` 选项 |
| `include/cangjie/CHIR/IR/Type/CHIRType.h` | 修改 | 新增 `ClearASTTypeCache()` |
