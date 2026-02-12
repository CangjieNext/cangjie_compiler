# Testing Workflow - 测试策略

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Objective

仓颉编译器的测试策略，包括单元测试、集成测试和回归测试，确保编译器的正确性和稳定性。

## Prerequisites

- 编译器已成功构建
- 了解测试框架和测试方法
- 熟悉编译器各模块的功能
- 掌握基本的测试编写技巧

## Steps

### 1. 单元测试

Run unit tests for individual compiler modules:

**Command:**
```bash
cd build/build
ninja check-cangjie
```

This runs all unit tests in the `unittests/` directory.

**Test structure:**
- Each module has its own test directory
- Tests use Google Test framework
- Tests are organized by functionality

**Test directories:**
- `unittests/AST/` - Abstract syntax tree tests
- `unittests/Basic/` - Basic utilities and data structures
- `unittests/CHIR/` - CHIR intermediate representation tests
- `unittests/CodeGen/` - Code generation tests
- `unittests/ConditionalCompilation/` - Conditional compilation tests
- `unittests/Demangle/` - Name demangling tests
- `unittests/Driver/` - Compiler driver tests
- `unittests/Frontend/` - Frontend integration tests
- `unittests/IncrCompile/` - Incremental compilation tests
- `unittests/Lex/` - Lexer tests
- `unittests/Macro/` - Macro expansion tests
- `unittests/Modules/` - Module system tests
- `unittests/Option/` - Command-line option tests
- `unittests/Parse/` - Parser tests
- `unittests/Sema/` - Semantic analysis tests
- `unittests/Utils/` - Utility function tests

**Running specific tests:**
```bash
# Run tests for a specific module
./unittests/Lex/LexTests
./unittests/Parse/ParseTests
./unittests/Sema/SemaTests

# Run with filter
./unittests/Lex/LexTests --gtest_filter=TokenTest.*
```

**Involved Modules:** All compiler modules

### 2. 集成测试

Run end-to-end integration tests:

**Purpose:**
- Test complete compilation pipeline
- Verify interaction between modules
- Test real-world code scenarios
- Validate generated code correctness

**Test approach:**
```bash
# Compile test programs
cjc test_program.cj -o test_program

# Run and verify output
./test_program
echo $?  # Check exit code

# Test with different options
cjc -O2 test_program.cj -o test_program_opt
cjc -g test_program.cj -o test_program_debug
```

**Test categories:**
- Basic language features
- Standard library usage
- Error handling
- Edge cases and corner cases
- Performance benchmarks

**Involved Modules:** Complete toolchain

### 3. 回归测试

Ensure modifications don't break existing functionality:

**Purpose:**
- Catch regressions early
- Maintain code quality
- Verify bug fixes don't introduce new bugs
- Ensure backward compatibility

**Regression test strategy:**
```bash
# Run full test suite before changes
ninja check-cangjie > before.log

# Make changes to compiler

# Run full test suite after changes
ninja check-cangjie > after.log

# Compare results
diff before.log after.log
```

**Best practices:**
- Run tests before committing changes
- Add tests for bug fixes
- Update tests when behavior changes intentionally
- Maintain test coverage metrics
- Use continuous integration (CI) for automated testing

**Test coverage:**
```bash
# Build with coverage instrumentation
python3 build.py -t debug --coverage

# Run tests
ninja check-cangjie

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

**Involved Modules:** All modified modules and their dependencies

## Involved Modules

### Core Modules
- **AST:** Abstract syntax tree representation and manipulation
- **Basic:** Fundamental data structures and utilities
- **CHIR:** High-level intermediate representation
- **CodeGen:** LLVM IR code generation
- **Lex:** Lexical analysis and tokenization
- **Parse:** Syntax analysis and AST construction
- **Sema:** Semantic analysis and type checking

### Advanced Features
- **ConditionalCompilation:** Conditional compilation support
- **Demangle:** Symbol name demangling
- **Driver:** Compiler driver and orchestration
- **Frontend:** Frontend integration and coordination
- **IncrCompile:** Incremental compilation support
- **Macro:** Macro expansion and processing
- **Modules:** Module system implementation

### Support
- **Option:** Command-line option parsing
- **Utils:** Utility functions and helpers

## Troubleshooting

### Test compilation failures

**Symptoms:** Tests fail to compile

**Solutions:**
- Verify compiler is built successfully
- Check test dependencies are available
- Ensure test framework (Google Test) is properly linked
- Rebuild tests: `ninja clean && ninja check-cangjie`
- Check for missing header files or libraries

### Test execution failures

**Symptoms:** Tests compile but fail at runtime

**Solutions:**
- Run individual tests to isolate failures
- Check test expectations match implementation
- Verify test input data is correct
- Use debugger to step through failing tests
- Check for environment-specific issues
- Review recent code changes that might affect tests

### Flaky tests

**Symptoms:** Tests pass sometimes and fail other times

**Solutions:**
- Check for race conditions in concurrent code
- Verify tests don't depend on timing
- Ensure tests clean up properly
- Check for uninitialized variables
- Verify tests don't depend on execution order
- Use deterministic random seeds for reproducibility

### Coverage gaps

**Symptoms:** Low test coverage in certain areas

**Solutions:**
- Identify untested code paths
- Add tests for edge cases
- Test error handling paths
- Add negative tests (invalid input)
- Test boundary conditions
- Review coverage reports regularly

### Performance test failures

**Symptoms:** Tests timeout or run too slowly

**Solutions:**
- Profile slow tests to find bottlenecks
- Reduce test input size if appropriate
- Check for infinite loops or excessive recursion
- Optimize test setup and teardown
- Use mocking to avoid expensive operations
- Consider splitting large tests into smaller ones

### Integration test failures

**Symptoms:** Unit tests pass but integration tests fail

**Solutions:**
- Check module interactions
- Verify data flow between modules
- Test with realistic input data
- Check for missing initialization
- Verify correct module integration
- Add logging to trace execution flow

### Regression test failures

**Symptoms:** Previously passing tests now fail

**Solutions:**
- Identify which change caused the regression
- Use git bisect to find the problematic commit
- Review the failing test to understand expectations
- Determine if test needs updating or code needs fixing
- Check if behavior change was intentional
- Update tests if new behavior is correct

### Memory leaks in tests

**Symptoms:** Valgrind reports memory leaks

**Solutions:**
- Run valgrind on specific tests: `valgrind ./unittests/Module/ModuleTests`
- Check for missing delete/free calls
- Verify smart pointers are used correctly
- Ensure test cleanup is complete
- Check for circular references
- Use RAII patterns for resource management

### Test environment issues

**Symptoms:** Tests fail in certain environments

**Solutions:**
- Check environment variables are set correctly
- Verify file paths are correct
- Ensure required files and resources are available
- Check for platform-specific issues
- Verify compiler and library versions
- Use consistent test environments (containers/VMs)

### Test data issues

**Symptoms:** Tests fail due to incorrect or missing test data

**Solutions:**
- Verify test data files exist and are readable
- Check test data format is correct
- Ensure test data is version controlled
- Validate test data before running tests
- Use test data generators for complex inputs
- Document test data requirements

## Best Practices

### Writing Tests
- Write tests before fixing bugs (TDD approach)
- Keep tests simple and focused
- Use descriptive test names
- Test one thing per test case
- Avoid test interdependencies
- Use setup and teardown appropriately

### Test Organization
- Group related tests together
- Use test fixtures for common setup
- Organize tests by module and functionality
- Keep test code maintainable
- Document complex test scenarios

### Test Maintenance
- Update tests when requirements change
- Remove obsolete tests
- Refactor tests to reduce duplication
- Keep tests running fast
- Monitor test coverage trends

### Continuous Integration
- Run tests automatically on commits
- Fail builds on test failures
- Track test metrics over time
- Run tests in multiple environments
- Generate and review coverage reports

## Reference

See also:
- `debugging.md` workflow for debugging test failures
- `adding-feature.md` workflow for adding tests with new features
- Google Test documentation for test framework details
