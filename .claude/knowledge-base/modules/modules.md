# Module: modules

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

包和模块管理，导入解析。该模块负责处理仓颉语言的模块系统，包括包的管理、模块的导入解析、依赖关系处理以及 AST 的序列化和反序列化。

## Key Components

- **ImportManager**: 导入管理器，负责解析和处理 import 语句
- **CjoManager**: CJO 文件管理器，处理编译后的模块文件（.cjo）
- **PackageManager**: 包管理器，管理包的查找和加载
- **DependencyGraph**: 依赖图，管理模块间的依赖关系
- **ASTSerialization**: AST 序列化子系统，支持模块的持久化

## Public APIs

- `ImportManager::resolveImport()` - 解析 import 语句
- `ImportManager::loadModule()` - 加载模块
- `CjoManager::readCjo()` - 读取 CJO 文件
- `CjoManager::writeCjo()` - 写入 CJO 文件
- `PackageManager::findPackage()` - 查找包
- `PackageManager::resolvePackage()` - 解析包路径
- `DependencyGraph::addDependency()` - 添加依赖关系
- `DependencyGraph::topologicalSort()` - 拓扑排序依赖

## Dependencies

- **ast**: 抽象语法树定义
- **sema**: 语义分析，用于模块的类型检查

## Source Locations

- **Implementation**: `src/Modules/`
  - 包含模块管理的核心实现
  - `ASTSerialization/` 子目录包含 AST 序列化相关代码

- **Headers**: `include/cangjie/Modules/`
  - `ImportManager.h` - 导入管理器接口
  - `CjoManager.h` - CJO 管理器接口
  - `PackageManager.h` - 包管理器接口
  - `ModulesUtils.h` - 模块工具函数
  - `ASTSerialization.h` - AST 序列化接口

- **Tests**: `unittests/Modules/`

## Usage Examples

```cangjie
// 导入标准库模块
import std.collection.ArrayList

// 导入自定义模块
import mypackage.mymodule

// 使用导入的类型
let list = ArrayList<Int64>()
```

## Related Concepts

- [Module System](../concepts/module-system.md)
- [Serialization](../concepts/serialization.md)

## Related Workflows

- 模块编译和链接
- 包依赖管理
- 增量编译支持
