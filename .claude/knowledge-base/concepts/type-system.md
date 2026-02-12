# Type System

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

仓颉语言的类型系统（Type System）是编译器中负责类型定义、类型检查、类型推断和类型转换的核心组件。类型系统确保程序的类型安全性，支持静态类型检查、泛型编程、类型推断等特性。仓颉的类型系统包括基本类型、复合类型、泛型类型、接口类型等多种类型构造。

## Purpose

类型系统在仓颉编译器中的主要作用包括：

1. **类型安全**：在编译期检查类型错误，防止类型不匹配导致的运行时错误
2. **类型推断**：自动推导表达式和变量的类型，减少显式类型标注
3. **泛型支持**：支持泛型类型和泛型函数，实现代码复用
4. **类型兼容性检查**：检查类型之间的赋值兼容性、子类型关系等
5. **接口实现验证**：验证类型是否正确实现了接口要求的方法

## Implementation in Cangjie Compiler

### Key Data Structures

- **Type Hierarchy**: 类型层次结构，包括 Type 基类和各种派生类型（IntType, FloatType, ClassType, InterfaceType 等）
- **TypeManager**: 类型管理器，负责类型的创建、缓存和查询，确保相同类型只有一个实例
- **TypeChecker**: 类型检查器，执行类型检查和类型推断
- **GenericContext**: 泛型上下文，管理泛型参数和泛型实例化
- **TypeConstraints**: 类型约束系统，用于泛型类型参数的约束

### Key Algorithms

- **Type Inference**: 类型推断算法，基于 Hindley-Milner 类型推断或约束求解
- **Type Unification**: 类型统一算法，用于泛型类型推断
- **Subtype Checking**: 子类型检查算法，判断类型之间的继承和实现关系
- **Generic Instantiation**: 泛型实例化算法，将泛型类型具体化为特定类型
- **Type Compatibility**: 类型兼容性检查，包括赋值兼容性、转换兼容性等

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/Sema/TypeChecker.h` - 类型检查器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Sema/TypeManager.h` - 类型管理器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Sema/Type.h` - 类型系统核心定义
- `/usr1/ace/cangjie_compiler/src/Sema/TypeChecker.cpp` - 类型检查实现
- `/usr1/ace/cangjie_compiler/src/Sema/TypeInference.cpp` - 类型推断实现

## Related Modules

- **sema**: 类型系统的主要实现模块，包含 TypeChecker 和 TypeManager
- **ast**: 类型信息附加到 AST 节点
- **chir**: 类型信息传递到 CHIR 层
- **parse**: 解析类型声明和类型注解
- **codegen**: 根据类型信息生成代码

## Examples

```cpp
// 类型定义示例
class Type {
public:
  enum TypeKind {
    TK_Int, TK_Float, TK_Bool, TK_Class, TK_Interface, TK_Generic
  };
  virtual TypeKind getKind() const = 0;
  virtual bool isCompatibleWith(Type *Other) const = 0;
};

// 类型检查示例
class TypeChecker {
public:
  Type *checkExpr(Expr *E) {
    if (auto *BinExpr = dyn_cast<BinaryExpr>(E)) {
      Type *LHSType = checkExpr(BinExpr->getLHS());
      Type *RHSType = checkExpr(BinExpr->getRHS());
      if (!LHSType->isCompatibleWith(RHSType)) {
        reportError("Type mismatch in binary expression");
      }
      return LHSType;
    }
    // ... 其他表达式类型检查
  }
};

// 泛型实例化示例
class GenericInstantiator {
public:
  Type *instantiate(GenericType *GT, ArrayRef<Type*> TypeArgs) {
    // 将泛型类型参数替换为具体类型
    TypeSubstitutionMap SubstMap;
    for (size_t i = 0; i < GT->getNumParams(); ++i) {
      SubstMap[GT->getParam(i)] = TypeArgs[i];
    }
    return substituteType(GT->getBody(), SubstMap);
  }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/Sema/` - 语义分析和类型系统头文件
- `/usr1/ace/cangjie_compiler/src/Sema/` - 类型系统实现代码
- `/usr1/ace/cangjie_compiler/docs/TypeSystem.md` - 类型系统设计文档（如果存在）
