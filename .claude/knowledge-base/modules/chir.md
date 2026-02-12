# Module: CHIR

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

仓颉高层中间表示，AST 到 CHIR 的转换和优化

## Key Components

- CHIR IR nodes: Core intermediate representation node types
- AST2CHIR converter: Transforms AST into CHIR representation
- Analysis passes: Analyzes CHIR for various properties
- Optimization passes: Optimizes CHIR representation
- Transformation passes: Transforms CHIR structure
- Serializer: Serializes CHIR to persistent format
- Interpreter: Interprets CHIR for execution
- Utils: Utility functions and helpers

## Public APIs

- `CHIR::convert(AST& ast)`: Converts AST to CHIR representation
- `CHIR::optimize()`: Executes CHIR optimization passes
- AST2CHIR converter interface
- Analysis pass interfaces
- Optimization pass interfaces
- Transformation pass interfaces

## Dependencies

- sema: Semantic analysis module
- ast: Abstract syntax tree module

## Source Locations

- Headers: `include/cangjie/CHIR/`
  - `include/cangjie/CHIR/CHIR.h`: Main CHIR entry point
- Implementation: `src/CHIR/`
  - `src/CHIR/CHIR.cpp`: Main implementation
  - `src/CHIR/IR/`: CHIR IR node definitions
  - `src/CHIR/AST2CHIR/`: AST to CHIR conversion
  - `src/CHIR/Analysis/`: Analysis passes
  - `src/CHIR/Checker/`: CHIR validation and checking
  - `src/CHIR/Optimization/`: Optimization passes
  - `src/CHIR/Transformation/`: Transformation passes
  - `src/CHIR/Serializer/`: CHIR serialization
  - `src/CHIR/Interpreter/`: CHIR interpretation
  - `src/CHIR/Utils/`: Utility functions
- Tests: `unittests/CHIR/`

## Usage Examples

```cpp
// Convert AST to CHIR
cangjie::CHIR::CHIR chir;
chir.convert(ast);

// Run optimization passes
chir.optimize();
```

## Related Concepts

- [CHIR](../concepts/chir.md): Core concept of Cangjie high-level IR
- [AST](../concepts/ast.md): Abstract syntax tree representation

## Related Workflows

- AST to CHIR conversion workflow
- CHIR optimization pipeline
- IR generation and transformation
