# Module System

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

仓颉的模块系统（Module System）是编译器中负责管理代码组织、依赖关系和符号可见性的核心组件。模块系统支持包（Package）和模块（Module）两级组织结构，提供导入（Import）机制来引用其他模块的符号，并通过 CJO（Cangjie Object）格式实现编译产物的序列化和缓存。

## Purpose

模块系统在仓颉编译器中的主要作用包括：

1. **代码组织**：提供包和模块的层次化组织结构，支持大规模项目开发
2. **依赖管理**：管理模块间的依赖关系，构建依赖图，支持增量编译
3. **符号解析**：解析导入语句，查找和加载被依赖的模块
4. **可见性控制**：控制符号的导出和访问权限，实现封装
5. **编译缓存**：通过 CJO 格式缓存编译结果，加速重复编译

## Implementation in Cangjie Compiler

### Key Data Structures

- **ImportManager**: 导入管理器，负责处理 import 语句，解析模块依赖
- **PackageManager**: 包管理器，管理包的加载、查找和依赖关系
- **CjoManager**: CJO 文件管理器，处理 CJO 格式的读写和缓存
- **ModuleDecl**: 模块声明，表示一个模块的 AST 节点
- **DependencyGraph**: 依赖图，表示模块间的依赖关系

### Key Algorithms

- **Import Resolution**: 导入解析算法，根据导入路径查找模块
- **Dependency Analysis**: 依赖分析算法，构建模块依赖图，检测循环依赖
- **Module Loading**: 模块加载算法，从 CJO 文件或源代码加载模块
- **Symbol Export/Import**: 符号导出导入算法，管理模块间的符号可见性
- **Incremental Compilation**: 增量编译算法，基于依赖图和时间戳判断需要重新编译的模块

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/Modules/ImportManager.h` - 导入管理器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Modules/PackageManager.h` - 包管理器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Modules/CjoManager.h` - CJO 管理器定义
- `/usr1/ace/cangjie_compiler/src/Modules/` - 模块系统实现代码
- `/usr1/ace/cangjie_compiler/schema/ModuleFormat.fbs` - 模块序列化格式
- `/usr1/ace/cangjie_compiler/schema/PackageFormat.fbs` - 包序列化格式

## Related Modules

- **modules**: 模块系统的核心实现，包含 ImportManager、PackageManager、CjoManager
- **parse**: 解析 import 语句和模块声明
- **sema**: 在模块上下文中进行符号解析和类型检查
- **ast**: 模块相关的 AST 节点（ModuleDecl, ImportDecl）
- **driver**: 驱动模块系统的编译流程，处理多模块编译

## Examples

```cpp
// 导入解析示例
class ImportManager {
public:
  ModuleDecl *resolveImport(ImportDecl *Import) {
    // 解析导入路径
    std::string ModulePath = Import->getModulePath();

    // 查找模块
    if (auto *Cached = ModuleCache.find(ModulePath)) {
      return Cached;
    }

    // 从 CJO 文件加载
    if (auto *CjoFile = findCjoFile(ModulePath)) {
      ModuleDecl *Module = CjoManager.loadModule(CjoFile);
      ModuleCache.insert(ModulePath, Module);
      return Module;
    }

    // 从源代码编译
    ModuleDecl *Module = compileModule(ModulePath);
    ModuleCache.insert(ModulePath, Module);
    return Module;
  }
};

// 依赖图构建示例
class DependencyGraph {
  std::map<ModuleDecl*, std::vector<ModuleDecl*>> Edges;

public:
  void addDependency(ModuleDecl *From, ModuleDecl *To) {
    Edges[From].push_back(To);
  }

  bool hasCycle() {
    std::set<ModuleDecl*> Visited;
    std::set<ModuleDecl*> RecStack;
    for (auto &[Module, _] : Edges) {
      if (hasCycleDFS(Module, Visited, RecStack)) {
        return true;
      }
    }
    return false;
  }
};

// CJO 文件管理示例
class CjoManager {
public:
  void saveCjo(ModuleDecl *Module, const std::string &Path) {
    // 序列化 AST
    flatbuffers::FlatBufferBuilder Builder;
    auto ModuleOffset = serializeModule(Builder, Module);
    Builder.Finish(ModuleOffset);

    // 写入文件
    std::ofstream File(Path, std::ios::binary);
    File.write(reinterpret_cast<const char*>(Builder.GetBufferPointer()),
               Builder.GetSize());
  }

  ModuleDecl *loadCjo(const std::string &Path) {
    // 读取文件
    std::ifstream File(Path, std::ios::binary);
    std::vector<uint8_t> Buffer(std::istreambuf_iterator<char>(File), {});

    // 反序列化 AST
    auto *FBModule = GetModule(Buffer.data());
    return deserializeModule(FBModule);
  }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/Modules/` - 模块系统头文件目录
- `/usr1/ace/cangjie_compiler/src/Modules/` - 模块系统实现目录
- `/usr1/ace/cangjie_compiler/schema/ModuleFormat.fbs` - 模块序列化格式定义
- `/usr1/ace/cangjie_compiler/schema/PackageFormat.fbs` - 包序列化格式定义
- `/usr1/ace/cangjie_compiler/docs/ModuleSystem.md` - 模块系统设计文档（如果存在）
