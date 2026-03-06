# Adding Feature Workflow - 添加新语言特性

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Objective

在仓颉编译器中添加新的语言特性，涵盖从语法设计到代码生成的完整流程。

## Prerequisites

- 编译器已成功构建（参见 building workflow）
- 了解编译器的基本流程和架构
- 熟悉 C++ 编程
- 理解编译原理（词法分析、语法分析、语义分析、代码生成）
- 了解 LLVM IR 基础知识

## Steps

### 1. 设计语法

Define the syntax rules for the new language feature:

- 确定语法结构和关键字
- 编写语法规范文档
- 考虑与现有语法的兼容性
- 设计示例代码展示新特性用法

**Involved Modules:** Language design, specification

### 2. 修改 Lex

If new tokens are needed, add them to the lexer:

- 在 `Tokens.inc` 中添加新的 token 定义
- 更新 token 类型枚举
- 添加关键字识别逻辑（如果需要）
- 更新词法分析器测试

**Files to modify:**
- `compiler/include/Lex/Tokens.inc`
- `compiler/lib/Lex/Lexer.cpp`

**Involved Modules:** Lex

### 3. 修改 Parse

Add new parsing rules to generate AST nodes:

- 在 Parser 中添加新的解析函数
- 实现语法规则的递归下降解析
- 处理语法错误和恢复
- 生成对应的 AST 节点
- 添加解析器测试

**Files to modify:**
- `compiler/lib/Parse/Parser.cpp`
- `compiler/lib/Parse/ParseExpr.cpp` (for expressions)
- `compiler/lib/Parse/ParseStmt.cpp` (for statements)
- `compiler/lib/Parse/ParseDecl.cpp` (for declarations)

**Involved Modules:** Parse

### 4. 修改 AST

If new node types are needed, define them in the AST:

- 在 `Node.h` 中定义新的 AST 节点类
- 实现节点的构造函数和访问方法
- 添加节点的序列化和反序列化支持
- 实现 AST 打印功能（用于调试）
- 更新 AST visitor 模式

**Files to modify:**
- `compiler/include/AST/Node.h`
- `compiler/lib/AST/Node.cpp`
- `compiler/lib/AST/ASTContext.cpp`

**Involved Modules:** AST

### 5. 修改 Sema

Add semantic analysis and type inference rules:

- 实现语义检查逻辑
- 添加类型推断和类型检查
- 处理作用域和符号解析
- 添加语义错误诊断
- 实现类型转换规则（如果需要）
- 添加语义分析测试

**Files to modify:**
- `compiler/lib/Sema/Sema.cpp`
- `compiler/lib/Sema/SemaExpr.cpp` (for expressions)
- `compiler/lib/Sema/SemaStmt.cpp` (for statements)
- `compiler/lib/Sema/SemaDecl.cpp` (for declarations)
- `compiler/lib/Sema/SemaType.cpp` (for type checking)

**Involved Modules:** Sema

### 6. 修改 CHIR

Add AST to CHIR (Cangjie High-level IR) transformation:

- 实现 AST 节点到 CHIR 的转换
- 添加新的 CHIR 指令或操作（如果需要）
- 实现 CHIR 优化（如果适用）
- 添加 CHIR dump 功能用于调试
- 添加 CHIR 转换测试

**Files to modify:**
- `compiler/lib/CHIR/CHIRGen.cpp`
- `compiler/include/CHIR/CHIR.h` (if new instructions needed)

**Involved Modules:** CHIR

### 7. 修改 CodeGen

Add CHIR to LLVM IR code generation:

- 实现 CHIR 到 LLVM IR 的转换
- 生成正确的 LLVM IR 指令序列
- 处理类型映射和内存布局
- 实现运行时支持（如果需要）
- 添加代码生成测试

**Files to modify:**
- `compiler/lib/CodeGen/CodeGen.cpp`
- `compiler/lib/CodeGen/CGExpr.cpp` (for expressions)
- `compiler/lib/CodeGen/CGStmt.cpp` (for statements)

**Involved Modules:** CodeGen

### 8. 添加测试

Add comprehensive unit tests:

- 在 `unittests/` 对应模块目录中添加单元测试
- 添加词法分析测试（Lex）
- 添加语法分析测试（Parse）
- 添加语义分析测试（Sema）
- 添加代码生成测试（CodeGen）
- 添加端到端集成测试
- 测试边界情况和错误处理

**Test directories:**
- `unittests/Lex/` - Lexer tests
- `unittests/Parse/` - Parser tests
- `unittests/Sema/` - Semantic analysis tests
- `unittests/CodeGen/` - Code generation tests

**Involved Modules:** All modified modules

## Involved Modules

- **Lex:** Lexical analysis, token recognition
- **Parse:** Syntax analysis, AST construction
- **AST:** Abstract syntax tree representation
- **Sema:** Semantic analysis, type checking
- **CHIR:** High-level intermediate representation
- **CodeGen:** LLVM IR code generation
- **Testing:** Unit tests and integration tests

## Troubleshooting

### Token not recognized

**Symptoms:** Lexer fails to recognize new keywords or operators

**Solutions:**
- Verify token is added to `Tokens.inc`
- Check token precedence and associativity
- Ensure lexer logic correctly identifies the token
- Add lexer unit tests to verify token recognition

### Parse errors

**Symptoms:** Parser fails to parse new syntax

**Solutions:**
- Check grammar rules are correctly implemented
- Verify parser follows correct precedence rules
- Add debug output to trace parsing steps
- Test with simplified examples first
- Check for conflicts with existing grammar

### Type checking failures

**Symptoms:** Semantic analysis rejects valid code

**Solutions:**
- Review type inference rules
- Check type compatibility logic
- Verify symbol resolution is correct
- Add detailed diagnostic messages
- Test type checking with various type combinations

### AST construction issues

**Symptoms:** AST nodes are malformed or incomplete

**Solutions:**
- Verify all AST node fields are initialized
- Check parent-child relationships are correct
- Use AST dump to visualize structure
- Ensure memory management is correct

### CHIR generation errors

**Symptoms:** AST to CHIR transformation fails

**Solutions:**
- Check CHIR instruction generation logic
- Verify operand types match instruction requirements
- Use CHIR dump to inspect generated IR
- Test with simpler AST structures first

### Code generation failures

**Symptoms:** LLVM IR generation fails or produces incorrect code

**Solutions:**
- Verify LLVM IR is well-formed (use `llvm-as` to validate)
- Check type mappings between CHIR and LLVM types
- Use `-emit-llvm` flag to inspect generated IR
- Test generated code with simple examples
- Verify runtime support functions are available

### Test failures

**Symptoms:** Unit tests or integration tests fail

**Solutions:**
- Run tests individually to isolate failures
- Add debug output to failing tests
- Verify test expectations match implementation
- Check for uninitialized variables or memory issues
- Use debugger to step through test execution

### Integration issues

**Symptoms:** Feature works in isolation but fails in combination with other features

**Solutions:**
- Test interactions with existing language features
- Check for namespace or symbol conflicts
- Verify feature doesn't break existing functionality
- Run full regression test suite
- Review code for unintended side effects

## Reference

See also:
- `debugging.md` workflow for debugging techniques
- `testing.md` workflow for testing strategies
- Architecture documentation for module details
