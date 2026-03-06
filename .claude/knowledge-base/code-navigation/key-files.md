# Key Files

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Overview

This document lists the most important files in each module of the Cangjie compiler. These files represent entry points, core functionality, and critical interfaces.

## AST Module

### Headers
- `include/cangjie/AST/ASTContext.h` - Central context for AST management, memory allocation
- `include/cangjie/AST/Decl.h` - Declaration node base classes and hierarchy
- `include/cangjie/AST/Expr.h` - Expression node base classes and hierarchy
- `include/cangjie/AST/Stmt.h` - Statement node base classes and hierarchy
- `include/cangjie/AST/Type.h` - Type representation and type system
- `include/cangjie/AST/DeclVisitor.h` - Visitor pattern for declarations
- `include/cangjie/AST/ExprVisitor.h` - Visitor pattern for expressions
- `include/cangjie/AST/StmtVisitor.h` - Visitor pattern for statements

### Source
- `src/AST/ASTContext.cpp` - AST context implementation
- `src/AST/Decl.cpp` - Declaration node implementations
- `src/AST/Expr.cpp` - Expression node implementations
- `src/AST/Stmt.cpp` - Statement node implementations
- `src/AST/Type.cpp` - Type system implementation

## Basic Module

### Headers
- `include/cangjie/Basic/Diagnostic.h` - Diagnostic message definitions and reporting
- `include/cangjie/Basic/SourceLocation.h` - Source location tracking
- `include/cangjie/Basic/SourceManager.h` - Source file management
- `include/cangjie/Basic/TokenKinds.h` - Token type definitions
- `include/cangjie/Basic/FileManager.h` - File system operations

### Source
- `src/Basic/Diagnostic.cpp` - Diagnostic engine implementation
- `src/Basic/SourceManager.cpp` - Source manager implementation
- `src/Basic/FileManager.cpp` - File manager implementation

### Include Files
- `include/cangjie/Basic/Tokens.inc` - Token definitions (generated)
- `include/cangjie/Basic/Keywords.inc` - Keyword definitions
- `include/cangjie/Basic/Diagnostics.inc` - Diagnostic message templates

## CHIR Module

### Headers
- `include/cangjie/CHIR/CHIRContext.h` - CHIR context and management
- `include/cangjie/CHIR/CHIRBuilder.h` - CHIR construction interface
- `include/cangjie/CHIR/CHIRModule.h` - CHIR module representation
- `include/cangjie/CHIR/CHIRFunction.h` - CHIR function representation
- `include/cangjie/CHIR/CHIRType.h` - CHIR type system
- `include/cangjie/CHIR/CHIRInstruction.h` - CHIR instruction definitions

### Source
- `src/CHIR/CHIRContext.cpp` - CHIR context implementation
- `src/CHIR/CHIRBuilder.cpp` - CHIR builder implementation
- `src/CHIR/CHIRGen.cpp` - AST to CHIR lowering
- `src/CHIR/CHIRVerifier.cpp` - CHIR verification and validation

### Schema
- `schema/BCHIRFormat.fbs` - Binary CHIR serialization format

## CodeGen Module

### Headers
- `include/cangjie/CodeGen/CGModule.h` - Code generation module interface
- `include/cangjie/CodeGen/CodeGenFunction.h` - Function code generation
- `include/cangjie/CodeGen/CodeGenTypes.h` - Type lowering to LLVM
- `include/cangjie/CodeGen/CGBuilder.h` - LLVM IR builder wrapper

### Source
- `src/CodeGen/CGModule.cpp` - Module code generation
- `src/CodeGen/CodeGenFunction.cpp` - Function code generation
- `src/CodeGen/CGDecl.cpp` - Declaration code generation
- `src/CodeGen/CGExpr.cpp` - Expression code generation
- `src/CodeGen/CGStmt.cpp` - Statement code generation
- `src/CodeGen/CodeGenTypes.cpp` - Type conversion to LLVM

## ConditionalCompilation Module

### Headers
- `include/cangjie/ConditionalCompilation/ConditionalCompilation.h` - Conditional compilation interface
- `include/cangjie/ConditionalCompilation/ConditionEvaluator.h` - Condition evaluation

### Source
- `src/ConditionalCompilation/ConditionalCompilation.cpp` - Conditional compilation implementation
- `src/ConditionalCompilation/ConditionEvaluator.cpp` - Condition evaluator

## Driver Module

### Headers
- `include/cangjie/Driver/Driver.h` - Main compiler driver interface
- `include/cangjie/Driver/Compilation.h` - Compilation session management
- `include/cangjie/Driver/Job.h` - Compilation job representation
- `include/cangjie/Driver/Action.h` - Compilation action definitions

### Source
- `src/Driver/Driver.cpp` - Driver implementation
- `src/Driver/Compilation.cpp` - Compilation orchestration
- `src/Driver/ToolChain.cpp` - Toolchain management

## Frontend Module

### Headers
- `include/cangjie/Frontend/CompilerInstance.h` - Compiler instance management
- `include/cangjie/Frontend/CompilerInvocation.h` - Compiler invocation configuration
- `include/cangjie/Frontend/FrontendAction.h` - Frontend action base class
- `include/cangjie/Frontend/FrontendOptions.h` - Frontend options

### Source
- `src/Frontend/CompilerInstance.cpp` - Compiler instance implementation
- `src/Frontend/CompilerInvocation.cpp` - Invocation setup
- `src/Frontend/FrontendAction.cpp` - Frontend actions

## IncrementalCompilation Module

### Headers
- `include/cangjie/IncrementalCompilation/IncrementalCompilation.h` - Incremental compilation interface
- `include/cangjie/IncrementalCompilation/DependencyGraph.h` - Dependency tracking

### Source
- `src/IncrementalCompilation/IncrementalCompilation.cpp` - Incremental compilation logic
- `src/IncrementalCompilation/DependencyGraph.cpp` - Dependency graph management

### Schema
- `schema/IncrementalCompilationFormat.fbs` - Incremental state serialization

## Lex Module

### Headers
- `include/cangjie/Lex/Lexer.h` - Main lexer interface
- `include/cangjie/Lex/Token.h` - Token representation
- `include/cangjie/Lex/Preprocessor.h` - Preprocessor interface

### Source
- `src/Lex/Lexer.cpp` - Lexer implementation
- `src/Lex/Token.cpp` - Token utilities
- `src/Lex/Preprocessor.cpp` - Preprocessor implementation

## Macro Module

### Headers
- `include/cangjie/Macro/MacroExpander.h` - Macro expansion interface
- `include/cangjie/Macro/MacroDefinition.h` - Macro definition representation

### Source
- `src/Macro/MacroExpander.cpp` - Macro expansion implementation
- `src/Macro/MacroDefinition.cpp` - Macro definition handling

## Mangle Module

### Headers
- `include/cangjie/Mangle/Mangle.h` - Name mangling interface
- `include/cangjie/Mangle/Demangle.h` - Name demangling interface

### Source
- `src/Mangle/Mangle.cpp` - Mangling implementation
- `src/Mangle/Demangle.cpp` - Demangling implementation

## MetaTransformation Module

### Headers
- `include/cangjie/MetaTransformation/MetaTransformation.h` - Meta-transformation interface
- `include/cangjie/MetaTransformation/TransformationContext.h` - Transformation context

### Source
- `src/MetaTransformation/MetaTransformation.cpp` - Meta-transformation implementation

## Modules Module

### Headers
- `include/cangjie/Modules/Module.h` - Module representation
- `include/cangjie/Modules/ModuleBuilder.h` - Module construction
- `include/cangjie/Modules/ModuleLoader.h` - Module loading interface
- `include/cangjie/Modules/ModuleMap.h` - Module dependency mapping

### Source
- `src/Modules/Module.cpp` - Module implementation
- `src/Modules/ModuleBuilder.cpp` - Module builder
- `src/Modules/ModuleLoader.cpp` - Module loader

### Schema
- `schema/ModuleFormat.fbs` - Module metadata serialization

## Option Module

### Headers
- `include/cangjie/Option/Options.h` - Compiler options definitions
- `include/cangjie/Option/OptTable.h` - Option parsing table

### Source
- `src/Option/Options.cpp` - Options implementation

### Include Files
- `include/cangjie/Option/Options.inc` - Option definitions (generated)

## Parse Module

### Headers
- `include/cangjie/Parse/Parser.h` - Main parser interface
- `include/cangjie/Parse/ParseDiagnostic.h` - Parser-specific diagnostics
- `include/cangjie/Parse/Scope.h` - Scope management during parsing

### Source
- `src/Parse/Parser.cpp` - Parser implementation
- `src/Parse/ParseDecl.cpp` - Declaration parsing
- `src/Parse/ParseExpr.cpp` - Expression parsing
- `src/Parse/ParseStmt.cpp` - Statement parsing
- `src/Parse/ParseType.cpp` - Type parsing

## Sema Module

### Headers
- `include/cangjie/Sema/Sema.h` - Main semantic analysis interface
- `include/cangjie/Sema/Scope.h` - Semantic scope management
- `include/cangjie/Sema/TypeChecker.h` - Type checking interface
- `include/cangjie/Sema/NameLookup.h` - Name resolution

### Source
- `src/Sema/Sema.cpp` - Semantic analysis implementation
- `src/Sema/SemaDecl.cpp` - Declaration semantic analysis
- `src/Sema/SemaExpr.cpp` - Expression semantic analysis
- `src/Sema/SemaStmt.cpp` - Statement semantic analysis
- `src/Sema/SemaType.cpp` - Type semantic analysis
- `src/Sema/TypeChecker.cpp` - Type checking implementation

## Utils Module

### Headers
- `include/cangjie/Utils/StringUtils.h` - String utilities
- `include/cangjie/Utils/FileUtils.h` - File utilities
- `include/cangjie/Utils/PathUtils.h` - Path manipulation utilities

### Source
- `src/Utils/StringUtils.cpp` - String utility implementations
- `src/Utils/FileUtils.cpp` - File utility implementations
- `src/Utils/PathUtils.cpp` - Path utility implementations

## Entry Points

### Main Compiler
- `src/FrontendTool/cjc.cpp` - Main compiler executable entry point

### Demangler Tool
- `demangler/main.cpp` - Standalone demangler tool

## Build System

### CMake
- `CMakeLists.txt` - Root CMake configuration
- `cmake/modules/` - CMake utility modules

## Schemas

### FlatBuffers
- `schema/BCHIRFormat.fbs` - Binary CHIR format
- `schema/ModuleFormat.fbs` - Module format
- `schema/IncrementalCompilationFormat.fbs` - Incremental compilation format
