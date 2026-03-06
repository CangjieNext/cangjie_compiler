# Module: macro

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

编译时宏展开和求值。该模块负责处理仓颉语言中的宏定义、宏调用、宏展开和宏求值，实现编译时的代码生成和转换功能。

## Key Components

- **MacroEvaluation**: 宏求值引擎，负责在编译时执行宏代码并计算结果
- **MacroExpansion**: 宏展开器，将宏调用展开为实际的 AST 节点
- **MacroCall**: 宏调用解析和处理，管理宏的调用过程
- **NodeSerialization**: AST 节点序列化，用于宏展开过程中的节点传递
- **TokenSerialization**: Token 序列化，支持宏展开中的词法单元处理

## Public APIs

- `MacroEvaluation::evaluate()` - 求值宏表达式
- `MacroEvaluation::expandMacro()` - 展开宏定义
- `MacroExpansion::expand()` - 执行宏展开
- `MacroCall::resolve()` - 解析宏调用
- `MacroCall::invoke()` - 调用宏函数
- `NodeSerialization::serialize()` - 序列化 AST 节点
- `NodeSerialization::deserialize()` - 反序列化 AST 节点
- `TokenSerialization::serializeTokens()` - 序列化 Token 流

## Dependencies

- **lex**: 词法分析，提供 Token 流
- **parse**: 语法分析，提供 AST 结构
- **ast**: 抽象语法树定义和操作

## Source Locations

- **Implementation**: `src/Macro/`
  - `MacroEvaluation.cpp` - 宏求值实现
  - `MacroExpansion.cpp` - 宏展开实现
  - `MacroProcess.cpp` - 宏处理流程
  - `MacroCall.cpp` - 宏调用处理
  - `NodeSerialization.cpp` - 节点序列化
  - `ExprSerialization.cpp` - 表达式序列化
  - `TokenSerialization.cpp` - Token 序列化
  - `MacroEvalMsgSerializer.cpp` - 宏求值消息序列化
  - `MacroCommon.cpp` - 宏通用功能
  - `MacroCallResolve.cpp` - 宏调用解析
  - `InvokeUtil.cpp` - 调用工具函数

- **Headers**: `include/cangjie/Macro/`

- **Tests**: `unittests/Macro/`

## Usage Examples

```cangjie
// 定义一个简单的宏
macro add(a: Int64, b: Int64): Int64 {
    return a + b
}

// 使用宏
let result = add(10, 20)  // 编译时展开为 30
```

## Related Concepts

- [Macro Expansion](../concepts/macro-expansion.md)

## Related Workflows

- 编译流程中的宏展开阶段
- 编译时代码生成
- 元编程支持
