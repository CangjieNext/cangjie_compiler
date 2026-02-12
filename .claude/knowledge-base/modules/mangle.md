# Module: mangle

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

符号名称修饰，用于链接。该模块负责将仓颉语言中的符号（函数、类型、变量等）转换为唯一的修饰名称，以支持链接器正确识别和链接符号，处理函数重载、泛型实例化等场景。

## Key Components

- **BaseMangler**: 基础修饰器，提供名称修饰的基础功能和通用算法
- **ASTMangler**: AST 级修饰器，在 AST 层面对声明和类型进行名称修饰
- **CHIRMangler**: CHIR 级修饰器，在 CHIR（仓颉中间表示）层面进行名称修饰
- **Compression**: 名称压缩，优化修饰后的符号名称长度
- **ManglingUtils**: 修饰工具函数，提供辅助功能

## Public APIs

- `BaseMangler::mangle()` - 基础名称修饰
- `ASTMangler::mangleDecl()` - 修饰 AST 声明
- `ASTMangler::mangleType()` - 修饰 AST 类型
- `CHIRMangler::mangleCHIRSymbol()` - 修饰 CHIR 符号
- `CHIRManglingUtils::mangleTypeName()` - 修饰类型名称
- `CHIRTypeManglingUtils::mangleFunctionType()` - 修饰函数类型

## Dependencies

- **ast**: 抽象语法树，提供声明和类型信息
- **chir**: 仓颉中间表示，提供 IR 层面的符号信息

## Source Locations

- **Implementation**: `src/Mangle/`
  - `BaseMangler.cpp` - 基础修饰器实现
  - `ASTMangler.cpp` - AST 修饰器实现
  - `Compression.cpp` - 名称压缩实现
  - `CHIRMangler.cpp` - CHIR 修饰器实现
  - `CHIRManglingUtils.cpp` - CHIR 修饰工具
  - `CHIRTypeManglingUtils.cpp` - CHIR 类型修饰工具

- **Headers**: `include/cangjie/Mangle/`
  - `BaseMangler.h` - 基础修饰器接口
  - `ASTMangler.h` - AST 修饰器接口
  - `CHIRMangler.h` - CHIR 修饰器接口
  - `CHIRManglingUtils.h` - CHIR 修饰工具
  - `CHIRTypeManglingUtils.h` - CHIR 类型修饰工具
  - `MangleUtils.h` - 通用修饰工具
  - `StdPkg.inc` - 标准包定义

- **Tests**: `unittests/Demangle/`

## Usage Examples

```cpp
// AST 层面的名称修饰
ASTMangler mangler;
std::string mangledName = mangler.mangleDecl(functionDecl);

// CHIR 层面的名称修饰
CHIRMangler chirMangler;
std::string chirMangledName = chirMangler.mangleCHIRSymbol(chirFunction);
```

修饰后的名称示例：
- 原始函数: `func add(a: Int64, b: Int64): Int64`
- 修饰后: `_CJ3add2i64i64_i64` (示例格式)

## Related Concepts

- [Name Mangling](../concepts/name-mangling.md)

## Related Workflows

- 代码生成阶段的符号处理
- 链接器符号解析
- 函数重载解析
- 泛型实例化
