# Naming Conventions

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Overview

This document describes the naming conventions used throughout the Cangjie compiler codebase. Consistent naming helps developers quickly understand code structure and locate relevant files.

## Code Naming Conventions

### Classes
**Convention:** PascalCase

Classes use PascalCase with descriptive names that indicate their purpose.

**Examples:**
- `Lexer` - Lexical analyzer
- `Parser` - Syntax parser
- `TypeChecker` - Type checking engine
- `CGModule` - Code generation module
- `ASTContext` - AST context manager
- `DiagnosticEngine` - Diagnostic reporting system
- `SourceManager` - Source file manager
- `TokenKind` - Token type enumeration

### Functions and Methods
**Convention:** camelCase

Functions and methods use camelCase starting with a lowercase letter. Names should be verb phrases describing the action.

**Examples:**
- `tokenize()` - Tokenize input
- `getNextToken()` - Retrieve next token
- `parseDecl()` - Parse a declaration
- `checkType()` - Perform type checking
- `emitCode()` - Emit generated code
- `createASTNode()` - Create an AST node
- `reportError()` - Report an error
- `isValidIdentifier()` - Check identifier validity

### Variables
**Convention:** camelCase

Variables use camelCase. Names should be descriptive nouns or noun phrases.

**Examples:**
- `tokenList` - List of tokens
- `currentNode` - Current AST node
- `sourceFile` - Source file reference
- `errorCount` - Number of errors
- `isValid` - Boolean flag

### Constants
**Convention:** PascalCase or UPPER_SNAKE_CASE

Constants may use either PascalCase (for enum values) or UPPER_SNAKE_CASE (for preprocessor defines).

**Examples:**
- `MaxTokenLength` - Maximum token length
- `DEFAULT_BUFFER_SIZE` - Default buffer size
- `TokenKind::Identifier` - Enum value

### Enums
**Convention:** PascalCase for type and values

Enum types and their values both use PascalCase.

**Examples:**
```cpp
enum class TokenKind {
  Identifier,
  Keyword,
  Operator,
  Literal
};

enum class DeclKind {
  Function,
  Class,
  Variable,
  Import
};
```

### Namespaces
**Convention:** lowercase

Namespaces use lowercase, typically single words.

**Examples:**
- `cangjie` - Main compiler namespace
- `detail` - Implementation details
- `internal` - Internal utilities

## File Naming Conventions

### C++ Source Files
**Convention:** PascalCase matching class name

Source files (.cpp) use PascalCase and typically match the primary class they implement.

**Examples:**
- `Lexer.cpp` - Implements Lexer class
- `Parser.cpp` - Implements Parser class
- `TypeChecker.cpp` - Implements TypeChecker class
- `CGModule.cpp` - Implements CGModule class

### C++ Header Files
**Convention:** PascalCase matching class name

Header files (.h) use PascalCase and match the class they declare.

**Examples:**
- `Lexer.h` - Declares Lexer class
- `Parser.h` - Declares Parser class
- `ASTContext.h` - Declares ASTContext class
- `DiagnosticEngine.h` - Declares DiagnosticEngine class

### Include Files
**Convention:** PascalCase with .inc extension

Include files (.inc) contain generated or shared code included by other files.

**Examples:**
- `Tokens.inc` - Token definitions
- `Options.inc` - Compiler options
- `Keywords.inc` - Keyword definitions
- `Diagnostics.inc` - Diagnostic messages

### FlatBuffers Schema Files
**Convention:** PascalCase with .fbs extension

FlatBuffers schema files define serialization formats.

**Examples:**
- `BCHIRFormat.fbs` - Binary CHIR format
- `ModuleFormat.fbs` - Module metadata format
- `IncrementalCompilationFormat.fbs` - Incremental compilation format

### Test Files
**Convention:** PascalCase with Test suffix

Test files use PascalCase with "Test" suffix.

**Examples:**
- `LexerTest.cpp` - Tests for Lexer
- `ParserTest.cpp` - Tests for Parser
- `TypeCheckerTest.cpp` - Tests for TypeChecker
- `ASTTest.cpp` - Tests for AST

## Directory Naming Conventions

### Module Directories
**Convention:** PascalCase

Module directories use PascalCase matching the module name.

**Examples:**
- `AST/` - Abstract Syntax Tree module
- `Lex/` - Lexical analysis module
- `Parse/` - Parsing module
- `Sema/` - Semantic analysis module
- `CHIR/` - Cangjie High-level IR module
- `CodeGen/` - Code generation module

### Special Directories
**Convention:** lowercase or snake_case

Special directories use lowercase or snake_case.

**Examples:**
- `src/` - Source code
- `include/` - Header files
- `unittests/` - Unit tests
- `schema/` - FlatBuffers schemas
- `third_party/` - Third-party dependencies
- `integration_build/` - Integration build scripts

## Naming Patterns

### Factory Functions
Prefix with `create` or `make`:
- `createLexer()`
- `makeASTNode()`
- `createDiagnostic()`

### Getter Functions
Prefix with `get`:
- `getToken()`
- `getType()`
- `getSourceLocation()`

### Boolean Predicates
Prefix with `is`, `has`, or `can`:
- `isValid()`
- `hasError()`
- `canParse()`

### Setter Functions
Prefix with `set`:
- `setToken()`
- `setType()`
- `setLocation()`

## Best Practices

1. **Be descriptive**: Names should clearly indicate purpose
2. **Be consistent**: Follow established patterns in the codebase
3. **Avoid abbreviations**: Use full words unless abbreviation is standard (e.g., AST, IR)
4. **Use domain terminology**: Prefer compiler-specific terms (e.g., "decl" for declaration)
5. **Match file to class**: Primary class name should match filename
