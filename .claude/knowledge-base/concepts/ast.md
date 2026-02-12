# Abstract Syntax Tree (AST)

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

抽象语法树（Abstract Syntax Tree, AST）是源代码的树状结构表示。它是编译器前端的核心数据结构，将源代码的语法结构以树形式组织，每个节点代表源代码中的一个语法构造（如表达式、语句、声明等）。AST 抽象掉了源代码中的具体语法细节（如括号、分号等），保留了程序的语义结构。

## Purpose

AST 在仓颉编译器中的主要作用包括：

1. **语法结构表示**：将解析后的源代码转换为结构化的树形表示，便于后续编译阶段处理
2. **语义分析基础**：为类型检查、名称解析等语义分析提供数据结构支持
3. **代码转换**：作为从源代码到中间表示（CHIR）转换的桥梁
4. **序列化支持**：支持 AST 的序列化和反序列化，用于增量编译和模块缓存
5. **工具支持**：为代码分析、重构、IDE 功能等工具提供基础数据结构

## Implementation in Cangjie Compiler

### Key Data Structures

- **Node Hierarchy**: 所有 AST 节点的基类和派生类层次结构，包括表达式节点（Expr）、语句节点（Stmt）、声明节点（Decl）等
- **ASTContext**: AST 的内存管理器，负责 AST 节点的分配和生命周期管理，使用 arena 分配策略提高性能
- **SourceLocation**: 源代码位置信息，用于错误报告和调试
- **Type Nodes**: 类型相关的 AST 节点，表示类型声明和类型引用
- **Attribute Nodes**: 属性节点，表示代码中的注解和修饰符

### Key Algorithms

- **AST Construction**: 解析器（Parser）根据语法规则构建 AST，采用递归下降解析方法
- **AST Traversal**: Walker 模式用于遍历 AST，支持前序、后序等多种遍历方式
- **AST Annotation**: 语义分析阶段对 AST 节点进行注解，添加类型信息、符号引用等
- **AST Serialization**: 将 AST 序列化为二进制格式（基于 FlatBuffers），用于模块缓存
- **AST Deserialization**: 从二进制格式恢复 AST 结构，支持增量编译

## Related Modules

- **ast**: AST 数据结构的定义和基本操作
- **parse**: 负责从源代码构建 AST
- **sema**: 对 AST 进行语义分析和注解
- **modules**: AST 的序列化和反序列化，用于模块系统
- **macro**: 宏展开过程中的 AST 操作和序列化
- **codegen**: 将 AST 转换为 CHIR 的起点

## Examples

```cpp
// AST 节点创建示例
class BinaryExpr : public Expr {
  Expr *LHS;
  Expr *RHS;
  BinaryOperatorKind Op;

public:
  BinaryExpr(ASTContext &Ctx, Expr *lhs, Expr *rhs, BinaryOperatorKind op)
    : LHS(lhs), RHS(rhs), Op(op) {}
};

// AST 遍历示例
class ASTWalker {
public:
  virtual void visitExpr(Expr *E) = 0;
  virtual void visitStmt(Stmt *S) = 0;
  virtual void visitDecl(Decl *D) = 0;
};

// AST 使用示例
ASTContext Context;
Expr *left = new (Context) IntegerLiteral(42);
Expr *right = new (Context) IntegerLiteral(10);
Expr *binExpr = new (Context) BinaryExpr(Context, left, right, BO_Add);
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/AST/Node.h` - AST 节点基类定义
- `/usr1/ace/cangjie_compiler/include/cangjie/AST/Types.h` - 类型相关 AST 节点
- `/usr1/ace/cangjie_compiler/include/cangjie/AST/Expr.h` - 表达式 AST 节点
- `/usr1/ace/cangjie_compiler/include/cangjie/AST/Stmt.h` - 语句 AST 节点
- `/usr1/ace/cangjie_compiler/include/cangjie/AST/Decl.h` - 声明 AST 节点
- `/usr1/ace/cangjie_compiler/src/AST/` - AST 实现代码
- `/usr1/ace/cangjie_compiler/schema/NodeFormat.fbs` - AST 序列化格式定义
