# Module: CodeGen

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

将 CHIR 转换为 LLVM IR

## Key Components

- CGModule: Module-level code generation (模块级代码生成)
- CGFunction: Function-level code generation (函数级代码生成)
- CGContext: Code generation context (代码生成上下文)
- IRBuilder: LLVM IR builder (LLVM IR 构建器)
- DIBuilder: Debug information builder (调试信息构建器)
- EmitPackageIR: Package IR emission
- EmitFunctionIR: Function IR emission
- EmitBasicBlockIR: Basic block IR emission
- EmitExpressionIR: Expression IR emission
- EmitGlobalVariableIR: Global variable IR emission

## Public APIs

- `CGModule::emitModule()`: Emits LLVM module from CHIR
- `CGModule::emitFunction()`: Emits LLVM function from CHIR function
- `CGFunction`: Function-level code generation interface
- `CGContext`: Code generation context management
- `IRBuilder::createAlloca()`: Creates alloca instruction
- `IRBuilder::createStore()`: Creates store instruction
- `IRBuilder::createLoad()`: Creates load instruction
- `IRBuilder::createCall()`: Creates call instruction
- `DIBuilder`: Debug information generation interface

## Dependencies

- chir: Cangjie high-level IR module

## Source Locations

- Headers: `include/cangjie/CodeGen/`
  - `include/cangjie/CodeGen/CGModule.h`: Module code generation
  - `include/cangjie/CodeGen/CGContext.h`: Code generation context
  - `include/cangjie/CodeGen/IRBuilder.h`: IR builder interface
  - `include/cangjie/CodeGen/DIBuilder.h`: Debug info builder
  - `include/cangjie/CodeGen/IRAttribute.h`: IR attributes
  - `include/cangjie/CodeGen/EmitPackageIR.h`: Package emission
- Implementation: `src/CodeGen/`
  - `src/CodeGen/Base/`: Base code generation components
  - `src/CodeGen/CJNative/`: Cangjie native code generation
  - `src/CodeGen/IncrementalGen/`: Incremental code generation
  - `src/CodeGen/Utils/`: Utility functions
- Tests: `unittests/CodeGen/`

## Usage Examples

```cpp
// Create code generation context
cangjie::CodeGen::CGContext context;

// Create module code generator
cangjie::CodeGen::CGModule cgModule(context);

// Emit LLVM IR from CHIR
cgModule.emitModule(chirModule);

// Emit function
cgModule.emitFunction(chirFunction);
```

## Related Concepts

- [LLVM Integration](../concepts/llvm-integration.md): Integration with LLVM infrastructure
- [CHIR](../concepts/chir.md): Cangjie high-level IR

## Related Workflows

- CHIR to LLVM IR conversion workflow
- Code generation pipeline
- Debug information generation
