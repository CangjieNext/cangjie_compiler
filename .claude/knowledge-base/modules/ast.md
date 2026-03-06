# Module: AST

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

定义和操作抽象语法树数据结构

## Key Components

- **Node**: AST 节点基类，所有 AST 节点的基础类型
- **ASTContext**: AST 上下文和内存管理，负责 AST 节点的分配和生命周期管理
- **Walker**: AST 遍历器，提供遍历 AST 的机制
- **Types**: 类型表示，定义类型系统的数据结构
- **Create**: AST 节点创建工具
- **Clone**: AST 节点克隆工具
- **Query**: AST 查询工具
- **Searcher**: AST 搜索工具

## Public APIs

- `Node::getKind()`: 获取节点类型
- `Node::getChildren()`: 获取子节点
- `ASTContext::create()`: 创建 AST 节点
- `ASTContext::allocate()`: 分配内存
- `Walker::walk()`: 遍历 AST
- `Create::createNode()`: 创建特定类型的节点
- `Clone::clone()`: 克隆 AST 节点
- `Query::query()`: 查询 AST 信息
- `Searcher::search()`: 搜索 AST 节点

## Dependencies

无直接依赖

## Source Locations

**Headers:**
- `include/cangjie/AST/Node.h`
- `include/cangjie/AST/NodeX.h`
- `include/cangjie/AST/Types.h`
- `include/cangjie/AST/Create.h`
- `include/cangjie/AST/Clone.h`
- `include/cangjie/AST/Query.h`
- `include/cangjie/AST/Searcher.h`
- `include/cangjie/AST/ASTContext.h`
- `include/cangjie/AST/Walker.h`
- `include/cangjie/AST/PrintNode.h`

**Implementations:**
- `src/AST/Node.cpp`
- `src/AST/Types.cpp`
- `src/AST/Create.cpp`
- `src/AST/Clone.cpp`
- `src/AST/Utils.cpp`
- `src/AST/Walker.cpp`
- `src/AST/PrintNode.cpp`
- `src/AST/ASTContext.cpp`
- `src/AST/Query.cpp`
- `src/AST/Searcher.cpp`

**Tests:**
- `unittests/AST/`

## Usage Examples

```cpp
// 创建 AST 上下文
ASTContext context;

// 创建 AST 节点
Node* node = context.create<DeclNode>();

// 遍历 AST
Walker walker;
walker.walk(node, [](Node* n) {
    // 处理节点
});

// 克隆 AST
Clone cloner;
Node* clonedNode = cloner.clone(node);

// 查询 AST
Query query;
auto result = query.query(node, "some-query");
```

## Related Concepts

- [AST](../concepts/ast.md)
- [Type System](../concepts/type-system.md)

## Related Workflows

- [Adding Feature](../workflows/adding-feature.md)
