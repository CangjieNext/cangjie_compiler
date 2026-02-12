# Cangjie Compiler Modules Index

Last Updated: 2026-02-12
Version: 1.0.0

## Frontend Modules
- [AST](./ast.md) - 抽象语法树数据结构
- [Lex](./lex.md) - 词法分析，将源码转换为 token 流
- [Parse](./parse.md) - 语法分析，构建 AST
- [Sema](./sema.md) - 语义分析，类型检查和推断

## Intermediate Representation
- [CHIR](./chir.md) - 仓颉高层中间表示和优化

## Backend Modules
- [CodeGen](./codegen.md) - LLVM IR 代码生成

## Supporting Modules
- [Macro](./macro.md) - 宏展开和求值
- [Modules](./modules.md) - 包和模块管理
- [Mangle](./mangle.md) - 名称修饰
- [Driver](./driver.md) - 编译驱动和工具链
- [Utils](./utils.md) - 工具函数和辅助模块
