# Cangjie Compiler Knowledge Base

**Last Updated**: 2026-02-12
**Version**: 1.0.0

## Quick Navigation

- [Architecture Documentation](./architecture/index.md) - 编译器架构和设计
- [Module Documentation](./modules/index.md) - 各模块详细说明
- [Concept Documentation](./concepts/index.md) - 关键概念解释
- [Workflow Documentation](./workflows/index.md) - 常见工作流指南
- [Code Navigation](./code-navigation/index.md) - 代码导航指引

## How to Use This Knowledge Base

参见 [Knowledge Base Retrieval Skill](../skills/use-knowledge-base.md)

## Architecture Overview

仓颉编译器（Cangjie Compiler）是一个完整的编译器实现，将仓颉语言源码编译为可执行文件。编译流程：

```
Source Code → Lex → Parse → Sema → CHIR → CodeGen → LLVM → Executable
```

## Key Directories

- `src/` - 编译器源码实现
- `include/cangjie/` - 公共头文件
- `schema/` - FlatBuffers 序列化 schema
- `unittests/` - 单元测试
- `figures/` - 架构图
