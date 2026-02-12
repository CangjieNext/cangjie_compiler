# Debugging Workflow - 调试编译问题

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Objective

调试仓颉编译器的编译问题，包括定位问题阶段、分析诊断信息、使用调试工具等。

## Prerequisites

- 编译器 debug 构建（使用 `-t debug` 选项）
- 熟悉 GDB 或 LLDB 调试器
- 了解编译器各阶段的功能
- 掌握基本的调试技巧
- 了解 LLVM IR 基础知识

## Steps

### 1. 确定问题阶段

Use compiler flags to identify which compilation stage has the issue:

**Available dump options:**
- `-dump-tokens` - Dump lexer tokens
- `-dump-ast` - Dump abstract syntax tree
- `-dump-chir` - Dump Cangjie high-level IR
- `-emit-llvm` - Output LLVM IR
- `-v` - Verbose output showing all stages

**Example:**
```bash
cjc -dump-ast problematic.cj
cjc -dump-chir problematic.cj
cjc -emit-llvm problematic.cj -o output.ll
```

**Involved Modules:** Driver, all compilation stages

### 2. 查看诊断信息

Check DiagnosticEngine output for error messages:

- 分析错误消息的位置和类型
- 检查警告信息
- 查看诊断的严重级别（error, warning, note）
- 追踪错误的源代码位置
- 理解错误的上下文信息

**Diagnostic categories:**
- Lexical errors (token recognition)
- Syntax errors (parsing)
- Semantic errors (type checking, symbol resolution)
- Code generation errors

**Involved Modules:** DiagnosticEngine, all compilation stages

### 3. 使用 GDB/LLDB

Set breakpoints and debug the compiler:

**Starting the debugger:**
```bash
# Using GDB
gdb --args cjc problematic.cj

# Using LLDB
lldb -- cjc problematic.cj
```

**Common breakpoints:**
```gdb
# Break at main
break main

# Break at specific function
break Parser::parseExpression
break Sema::checkType
break CodeGen::emitFunction

# Break on exception
catch throw

# Conditional breakpoint
break Parser.cpp:123 if token.kind == TOK_IDENTIFIER
```

**Useful commands:**
```gdb
# Run the program
run

# Step over
next

# Step into
step

# Continue
continue

# Print variable
print variableName

# Print backtrace
backtrace

# List source code
list
```

**Involved Modules:** All modules, depending on breakpoint location

### 4. 检查 AST

Use PrintNode to print and inspect the AST:

**Methods:**
- Use `-dump-ast` flag to output AST structure
- Implement custom AST visitor for detailed inspection
- Check node types and relationships
- Verify AST structure matches expected grammar
- Inspect node attributes and metadata

**Example AST inspection:**
```cpp
// In debugger or custom code
node->dump();  // Print node structure
node->getKind();  // Get node type
node->getSourceRange();  // Get source location
```

**Common AST issues:**
- Missing or incorrect node types
- Broken parent-child relationships
- Incorrect source location information
- Missing type information

**Involved Modules:** AST, Parse

### 5. 检查 CHIR

Use CHIR dump functionality to inspect intermediate representation:

**Methods:**
- Use `-dump-chir` flag to output CHIR
- Verify CHIR instructions are correct
- Check operand types and values
- Inspect control flow graph
- Verify SSA form properties

**CHIR inspection points:**
- Instruction sequences
- Basic block structure
- Control flow edges
- Type information
- Symbol references

**Common CHIR issues:**
- Incorrect instruction generation
- Type mismatches
- Invalid operands
- Broken control flow

**Involved Modules:** CHIR, CHIRGen

### 6. 检查 LLVM IR

Use `-emit-llvm` to output and validate LLVM IR:

**Methods:**
```bash
# Generate LLVM IR
cjc -emit-llvm problematic.cj -o output.ll

# Validate LLVM IR
llvm-as output.ll -o output.bc

# Disassemble bitcode
llvm-dis output.bc -o output.ll

# Optimize and view
opt -O2 output.ll -S -o optimized.ll

# View CFG
opt -dot-cfg output.ll
```

**LLVM IR inspection:**
- Check function signatures
- Verify instruction sequences
- Inspect type definitions
- Check metadata and attributes
- Validate control flow

**Common LLVM IR issues:**
- Type mismatches
- Invalid instructions
- Incorrect function calls
- Memory access errors
- Missing or incorrect metadata

**Involved Modules:** CodeGen, LLVM backend

## Involved Modules

- **Driver:** Command-line interface, compilation orchestration
- **DiagnosticEngine:** Error and warning reporting
- **Lex:** Lexical analysis, token generation
- **Parse:** Syntax analysis, AST construction
- **AST:** Abstract syntax tree representation
- **Sema:** Semantic analysis, type checking
- **CHIR:** High-level intermediate representation
- **CodeGen:** LLVM IR code generation
- **LLVM:** Backend code generation and optimization

## Troubleshooting

### Lexer issues

**Symptoms:** Unexpected tokens, tokenization errors

**Solutions:**
- Use `-dump-tokens` to see token stream
- Check input file encoding (UTF-8 expected)
- Verify token definitions in `Tokens.inc`
- Check lexer state machine logic
- Test with minimal input to isolate issue

### Parser issues

**Symptoms:** Syntax errors, unexpected parse failures

**Solutions:**
- Use `-dump-ast` to see partial AST
- Add parser debug output
- Check grammar rules and precedence
- Verify error recovery logic
- Test with simplified input
- Check for ambiguous grammar

### Semantic analysis issues

**Symptoms:** Type errors, symbol resolution failures

**Solutions:**
- Check symbol table contents
- Verify type inference logic
- Inspect scope hierarchy
- Add diagnostic output in Sema
- Test with explicit type annotations
- Check for circular dependencies

### AST issues

**Symptoms:** Malformed AST, missing nodes

**Solutions:**
- Use AST dump to visualize structure
- Check node construction in parser
- Verify parent-child relationships
- Inspect node source locations
- Check memory management (use valgrind)

### CHIR generation issues

**Symptoms:** CHIR generation fails or produces invalid IR

**Solutions:**
- Use `-dump-chir` to inspect output
- Check AST to CHIR transformation logic
- Verify instruction operand types
- Test with simpler AST structures
- Add debug output in CHIRGen

### Code generation issues

**Symptoms:** LLVM IR generation fails, runtime crashes

**Solutions:**
- Use `-emit-llvm` to inspect generated IR
- Validate IR with `llvm-as`
- Check type mappings
- Verify function signatures
- Test generated code with simple inputs
- Use LLVM verification passes
- Check runtime library linkage

### Debugger issues

**Symptoms:** Debugger crashes, cannot set breakpoints

**Solutions:**
- Ensure debug build is used (`-t debug`)
- Check debug symbols are present (`file cjc`)
- Verify source paths are correct
- Use `info sources` to list source files
- Rebuild with `-g` flag if needed

### Performance issues

**Symptoms:** Compilation is very slow

**Solutions:**
- Profile the compiler with `perf` or `gprof`
- Check for infinite loops or recursion
- Identify hot spots in the code
- Use sampling profiler to find bottlenecks
- Check memory usage with `valgrind --tool=massif`

### Memory issues

**Symptoms:** Crashes, segmentation faults, memory leaks

**Solutions:**
- Run with valgrind: `valgrind --leak-check=full cjc input.cj`
- Check for null pointer dereferences
- Verify memory allocation and deallocation
- Use AddressSanitizer: compile with `-fsanitize=address`
- Check for use-after-free errors
- Verify smart pointer usage

### Diagnostic message issues

**Symptoms:** Unclear or missing error messages

**Solutions:**
- Check DiagnosticEngine configuration
- Verify diagnostic IDs are defined
- Add more detailed diagnostic messages
- Include source location information
- Add fix-it hints where appropriate

## Reference

See also:
- `adding-feature.md` workflow for feature development
- `testing.md` workflow for testing strategies
- LLVM documentation for IR debugging
- GDB/LLDB documentation for debugger usage
