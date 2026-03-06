# Serialization Formats

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

序列化格式（Serialization Formats）是仓颉编译器用于持久化和传输数据结构的二进制格式规范。编译器主要使用 FlatBuffers 作为序列化框架，定义了多种格式用于不同的数据结构，包括 AST 序列化、CHIR 序列化、模块序列化、包序列化和宏消息序列化等。这些格式支持高效的序列化和反序列化，用于编译缓存、模块间通信和宏展开等场景。

## Purpose

序列化格式在仓颉编译器中的主要作用包括：

1. **编译缓存**：将编译中间结果序列化到磁盘，支持增量编译
2. **模块通信**：在模块间传递 AST 和类型信息
3. **宏展开**：在编译器和宏求值服务器间传递 AST 和 Token
4. **持久化存储**：将编译产物（CJO 文件）持久化到磁盘
5. **跨进程通信**：在编译器的不同进程间传递数据

## Implementation in Cangjie Compiler

### Key Data Structures

- **FlatBuffers Schema**: FlatBuffers 模式定义，描述数据结构的序列化格式
- **Serializer**: 序列化器，将内存中的数据结构转换为二进制格式
- **Deserializer**: 反序列化器，从二进制格式恢复数据结构
- **Buffer**: 序列化缓冲区，存储序列化后的二进制数据
- **Verifier**: 验证器，验证序列化数据的完整性和正确性

### Key Algorithms

- **FlatBuffers Serialization**: FlatBuffers 序列化算法，零拷贝序列化
- **AST Serialization**: AST 序列化算法，递归序列化 AST 节点树
- **CHIR Serialization**: CHIR 序列化算法，序列化 CHIR 模块和指令
- **Type Serialization**: 类型序列化算法，序列化类型信息
- **Incremental Serialization**: 增量序列化算法，只序列化变化的部分

### Key Files

- `/usr1/ace/cangjie_compiler/schema/BCHIRFormat.fbs` - CHIR 二进制格式定义
- `/usr1/ace/cangjie_compiler/schema/CachedASTFormat.fbs` - 缓存 AST 格式定义
- `/usr1/ace/cangjie_compiler/schema/MacroMsgFormat.fbs` - 宏消息格式定义
- `/usr1/ace/cangjie_compiler/schema/ModuleFormat.fbs` - 模块格式定义
- `/usr1/ace/cangjie_compiler/schema/NodeFormat.fbs` - AST 节点格式定义
- `/usr1/ace/cangjie_compiler/schema/PackageFormat.fbs` - 包格式定义

## Related Modules

- **modules**: 使用序列化格式实现 AST 和模块的序列化
- **macro**: 使用序列化格式传递宏消息
- **chir**: 使用序列化格式缓存 CHIR
- **ast**: AST 节点的序列化和反序列化
- **codegen**: 使用序列化格式存储编译产物

## Examples

```cpp
// FlatBuffers Schema 定义示例 (NodeFormat.fbs)
namespace cangjie.serialization;

table IntegerLiteral {
  value: int64;
  type: Type;
  location: SourceLocation;
}

table BinaryExpr {
  lhs: Expr;
  rhs: Expr;
  op: BinaryOperator;
  type: Type;
  location: SourceLocation;
}

union Expr {
  IntegerLiteral,
  BinaryExpr,
  // ... 其他表达式类型
}

// AST 序列化示例
class ASTSerializer {
  flatbuffers::FlatBufferBuilder Builder;

public:
  flatbuffers::Offset<serialization::Expr> serializeExpr(Expr *E) {
    if (auto *IL = dyn_cast<IntegerLiteral>(E)) {
      auto ILOffset = serialization::CreateIntegerLiteral(
        Builder,
        IL->getValue(),
        serializeType(IL->getType()),
        serializeLocation(IL->getLocation())
      );
      return serialization::CreateExpr(Builder, serialization::Expr_IntegerLiteral, ILOffset.Union());
    } else if (auto *BE = dyn_cast<BinaryExpr>(E)) {
      auto BEOffset = serialization::CreateBinaryExpr(
        Builder,
        serializeExpr(BE->getLHS()),
        serializeExpr(BE->getRHS()),
        static_cast<serialization::BinaryOperator>(BE->getOp()),
        serializeType(BE->getType()),
        serializeLocation(BE->getLocation())
      );
      return serialization::CreateExpr(Builder, serialization::Expr_BinaryExpr, BEOffset.Union());
    }
    // ... 其他表达式类型
  }

  std::vector<uint8_t> serialize(Expr *E) {
    auto ExprOffset = serializeExpr(E);
    Builder.Finish(ExprOffset);
    return std::vector<uint8_t>(
      Builder.GetBufferPointer(),
      Builder.GetBufferPointer() + Builder.GetSize()
    );
  }
};

// AST 反序列化示例
class ASTDeserializer {
  ASTContext &Context;

public:
  Expr *deserializeExpr(const serialization::Expr *FBExpr) {
    switch (FBExpr->expr_type()) {
    case serialization::Expr_IntegerLiteral: {
      auto *FBIL = FBExpr->expr_as_IntegerLiteral();
      return new (Context) IntegerLiteral(
        FBIL->value(),
        deserializeType(FBIL->type()),
        deserializeLocation(FBIL->location())
      );
    }
    case serialization::Expr_BinaryExpr: {
      auto *FBBE = FBExpr->expr_as_BinaryExpr();
      return new (Context) BinaryExpr(
        deserializeExpr(FBBE->lhs()),
        deserializeExpr(FBBE->rhs()),
        static_cast<BinaryOperatorKind>(FBBE->op()),
        deserializeType(FBBE->type()),
        deserializeLocation(FBBE->location())
      );
    }
    // ... 其他表达式类型
    }
  }

  Expr *deserialize(const std::vector<uint8_t> &Data) {
    auto *FBExpr = serialization::GetExpr(Data.data());
    return deserializeExpr(FBExpr);
  }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/schema/` - 所有序列化格式定义目录
- `/usr1/ace/cangjie_compiler/src/Serialization/` - 序列化实现代码（如果存在）
- FlatBuffers Documentation - https://google.github.io/flatbuffers/
- `/usr1/ace/cangjie_compiler/docs/Serialization.md` - 序列化设计文档（如果存在）
