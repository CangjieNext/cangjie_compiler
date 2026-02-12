# Directory Structure

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Overview

This document maps logical modules to their physical directory locations in the Cangjie compiler codebase. The compiler follows a consistent structure where each module has its source code in `src/`, public headers in `include/cangjie/`, and unit tests in `unittests/`.

## Module Directory Mapping

| Module | Source Dir | Header Dir | Test Dir |
|--------|-----------|------------|----------|
| AST | src/AST/ | include/cangjie/AST/ | unittests/AST/ |
| Basic | src/Basic/ | include/cangjie/Basic/ | unittests/Basic/ |
| CHIR | src/CHIR/ | include/cangjie/CHIR/ | unittests/CHIR/ |
| CodeGen | src/CodeGen/ | include/cangjie/CodeGen/ | unittests/CodeGen/ |
| ConditionalCompilation | src/ConditionalCompilation/ | include/cangjie/ConditionalCompilation/ | unittests/ConditionalCompilation/ |
| Driver | src/Driver/ | include/cangjie/Driver/ | unittests/Driver/ |
| Frontend | src/Frontend/ | include/cangjie/Frontend/ | unittests/Frontend/ |
| FrontendTool | src/FrontendTool/ | include/cangjie/FrontendTool/ | - |
| IncrementalCompilation | src/IncrementalCompilation/ | include/cangjie/IncrementalCompilation/ | unittests/IncrCompile/ |
| Lex | src/Lex/ | include/cangjie/Lex/ | unittests/Lex/ |
| Macro | src/Macro/ | include/cangjie/Macro/ | unittests/Macro/ |
| Mangle | src/Mangle/ | include/cangjie/Mangle/ | unittests/Demangle/ |
| MetaTransformation | src/MetaTransformation/ | include/cangjie/MetaTransformation/ | - |
| Modules | src/Modules/ | include/cangjie/Modules/ | unittests/Modules/ |
| Option | src/Option/ | include/cangjie/Option/ | unittests/Option/ |
| Parse | src/Parse/ | include/cangjie/Parse/ | unittests/Parse/ |
| Sema | src/Sema/ | include/cangjie/Sema/ | unittests/Sema/ |
| Utils | src/Utils/ | include/cangjie/Utils/ | unittests/Utils/ |

## Source and Header Relationship

The Cangjie compiler follows a clear separation between interface and implementation:

- **Headers** (`include/cangjie/`): Define public interfaces, class declarations, and API contracts. These headers are included by other modules and external tools.
- **Source** (`src/`): Contain the actual implementation code. Implementation details are hidden from the public interface.

This separation enables:
- Clean module boundaries
- Faster compilation through reduced dependencies
- Clear API contracts
- Better encapsulation

## Additional Directories

### schema/
Contains FlatBuffers schema definitions for serialization formats:
- `BCHIRFormat.fbs`: Binary CHIR serialization format
- `ModuleFormat.fbs`: Module metadata serialization
- `IncrementalCompilationFormat.fbs`: Incremental compilation state

### figures/
Architecture diagrams and documentation images:
- Compiler pipeline diagrams
- Module dependency graphs
- Data flow visualizations

### third_party/
External dependencies and utilities:
- CMake utility modules
- LLVM integration code
- Third-party library configurations

### demangler/
Standalone name demangling tool:
- Converts mangled symbol names back to readable form
- Used for debugging and symbol analysis

### doc/
Documentation files:
- Design documents
- API documentation
- Development guides

### cmake/
CMake build system configuration:
- Module definitions
- Build rules
- Dependency management

### integration_build/
Integration build scripts and configurations:
- CI/CD integration
- Build automation
- Testing infrastructure

### utils/
Utility scripts and tools:
- Code generation scripts
- Build helpers
- Development utilities

## Navigation Tips

1. **Finding a module's code**: Start with `src/<ModuleName>/` for implementation
2. **Finding public APIs**: Look in `include/cangjie/<ModuleName>/`
3. **Finding tests**: Check `unittests/<ModuleName>/` (note: some test dirs have different names)
4. **Finding schemas**: Look in `schema/` for serialization formats
5. **Finding build config**: Check `cmake/` for build system files
