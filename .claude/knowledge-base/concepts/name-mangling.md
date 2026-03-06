# Name Mangling

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

名称修饰（Name Mangling）是将程序中的符号名称（如函数名、变量名、类型名等）编码为唯一字符串的过程。在仓颉编译器中，名称修饰用于解决符号重载、泛型实例化、命名空间等导致的符号名称冲突问题，确保链接器能够正确识别和链接符号。修饰后的名称包含了符号的完整信息，如所属模块、类型签名、泛型参数等。

## Purpose

名称修饰在仓颉编译器中的主要作用包括：

1. **符号唯一性**：确保每个符号在链接时有唯一的标识，避免名称冲突
2. **重载支持**：支持函数重载，通过参数类型区分同名函数
3. **泛型实例化**：为不同的泛型实例生成不同的符号名称
4. **模块隔离**：通过模块名编码实现符号的命名空间隔离
5. **ABI 兼容性**：提供稳定的二进制接口，支持跨版本链接

## Implementation in Cangjie Compiler

### Key Data Structures

- **BaseMangler**: 名称修饰的基类，提供通用的修饰算法和压缩机制
- **ASTMangler**: 基于 AST 的名称修饰器，在语义分析阶段使用
- **CHIRMangler**: 基于 CHIR 的名称修饰器，在代码生成阶段使用
- **ManglingContext**: 修饰上下文，维护修饰过程中的状态信息
- **CompressionTable**: 压缩表，用于减少修饰名称的长度

### Key Algorithms

- **Compression Algorithm**: 压缩算法，通过替换重复的子串减少修饰名称长度
- **Type Encoding**: 类型编码算法，将类型信息编码为字符串
- **Signature Mangling**: 函数签名修饰，包括参数类型、返回类型等
- **Generic Mangling**: 泛型修饰，编码泛型参数和约束
- **Module Path Encoding**: 模块路径编码，包含包名和模块名

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/Mangle/BaseMangler.h` - 基础修饰器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Mangle/ASTMangler.h` - AST 修饰器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Mangle/CHIRMangler.h` - CHIR 修饰器定义
- `/usr1/ace/cangjie_compiler/src/Mangle/` - 名称修饰实现代码

## Related Modules

- **mangle**: 名称修饰的核心实现，包含 BaseMangler、ASTMangler、CHIRMangler
- **sema**: 在语义分析阶段使用 ASTMangler 生成符号名称
- **codegen**: 在代码生成阶段使用 CHIRMangler 生成 LLVM 符号
- **modules**: 模块系统使用修饰名称进行符号导出和导入
- **linker**: 链接器使用修饰名称进行符号解析

## Examples

```cpp
// 名称修饰示例
class ASTMangler {
public:
  std::string mangleFunction(FunctionDecl *FD) {
    std::string Result = "_CJ"; // 仓颉前缀

    // 编码模块路径
    mangleModulePath(FD->getModule());

    // 编码函数名
    Result += std::to_string(FD->getName().size());
    Result += FD->getName();

    // 编码参数类型
    for (auto *Param : FD->getParameters()) {
      mangleType(Param->getType());
    }

    // 应用压缩
    return compress(Result);
  }

private:
  void mangleType(Type *T) {
    if (auto *IT = dyn_cast<IntType>(T)) {
      Result += "i" + std::to_string(IT->getBitWidth());
    } else if (auto *CT = dyn_cast<ClassType>(T)) {
      Result += "C" + std::to_string(CT->getName().size());
      Result += CT->getName();
    }
    // ... 其他类型编码
  }

  std::string compress(const std::string &Input) {
    // 压缩算法：替换重复子串为引用
    CompressionTable Table;
    std::string Output;
    for (size_t i = 0; i < Input.size(); ) {
      if (auto Ref = Table.find(Input, i)) {
        Output += "S" + std::to_string(Ref->Index) + "_";
        i += Ref->Length;
      } else {
        Output += Input[i];
        Table.insert(Input, i);
        ++i;
      }
    }
    return Output;
  }
};

// 使用示例
ASTMangler Mangler;
std::string MangledName = Mangler.mangleFunction(MyFunction);
// 结果可能类似: _CJ4main3add2i32i32
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/Mangle/` - 名称修饰头文件目录
- `/usr1/ace/cangjie_compiler/src/Mangle/` - 名称修饰实现目录
- `/usr1/ace/cangjie_compiler/docs/Mangling.md` - 名称修饰规范文档（如果存在）
- Itanium C++ ABI - 参考的名称修饰标准
