# Module: Sema

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

语义分析，类型检查和类型推断

## Key Components

- **TypeChecker**: 类型检查器，负责验证类型正确性和执行类型推断
- **TypeManager**: 类型管理器，管理类型系统和类型实例
- **Desugar**: 语法糖消除，将高级语法结构转换为基础形式
- **GenericInstantiationManager**: 泛型实例化管理器，处理泛型类型的实例化
- **Collector**: 收集器，收集语义信息
- **CJMP**: Cangjie 宏处理器
- **FFI**: 外部函数接口处理
- **NativeFFI**: 原生外部函数接口
- **InheritanceChecker**: 继承检查器
- **LegalityOfUsage**: 使用合法性检查
- **Plugin**: 插件系统
- **Test**: 测试管理器
- **TypeCheckExpr**: 表达式类型检查

## Public APIs

- `TypeChecker::check()`: 执行类型检查
- `TypeChecker::infer()`: 执行类型推断
- `TypeManager::getType()`: 获取类型
- `TypeManager::registerType()`: 注册类型
- `Desugar::desugar()`: 消除语法糖
- `GenericInstantiationManager::instantiate()`: 实例化泛型

## Dependencies

- **parse**: 语法分析模块，提供 AST
- **ast**: AST 数据结构模块

## Source Locations

**Headers:**
- `include/cangjie/Sema/TypeChecker.h`
- `include/cangjie/Sema/TypeManager.h`
- `include/cangjie/Sema/Desugar.h`
- `include/cangjie/Sema/GenericInstantiationManager.h`
- `include/cangjie/Sema/CommonTypeAlias.h`
- `include/cangjie/Sema/TestManager.h`
- `include/cangjie/Sema/IncrementalUtils.h`

**Implementations:**
- `src/Sema/` (64 files including subdirectories)
  - `CJMP/`
  - `Desugar/`
  - `FFI/`
  - `NativeFFI/`
  - `GenericInstantiation/`
  - `InheritanceChecker/`
  - `LegalityOfUsage/`
  - `Plugin/`
  - `Test/`
  - `TypeCheckExpr/`

**Tests:**
- `unittests/Sema/`

## Usage Examples

```cpp
// 创建类型检查器
TypeChecker checker;
TypeManager typeManager;

// 执行类型检查
bool isValid = checker.check(astNode, typeManager);

// 类型推断
Type* inferredType = checker.infer(expr, typeManager);

// 消除语法糖
Desugar desugar;
ASTNode* desugared = desugar.desugar(astNode);

// 泛型实例化
GenericInstantiationManager gim;
Type* instantiated = gim.instantiate(genericType, typeArgs);
```

## Related Concepts

- [Type System](../concepts/type-system.md)
- [AST](../concepts/ast.md)

## Related Workflows

- [Adding Feature](../workflows/adding-feature.md)
