# Requirements Document

## Introduction

本文档定义了仓颉编译器知识库系统的需求。该系统旨在为 AI 助手（如 Claude）提供一个结构化、易于检索的知识库，使其能够快速理解仓颉编译器的架构、模块、关键概念和代码组织。知识库将采用分层索引结构，并配备专门的检索技能，帮助 AI 高效定位相关信息。

## Glossary

- **Knowledge_Base**: 知识库系统，包含编译器架构、模块、概念等结构化信息的文档集合
- **Index_System**: 索引系统，提供多层次的导航和检索入口
- **Retrieval_Skill**: 检索技能，教导 AI 如何使用知识库查找信息的指导文档
- **Module_Documentation**: 模块文档，描述编译器各个模块的功能、接口和实现细节
- **Architecture_Documentation**: 架构文档，描述编译器整体架构和模块间关系
- **Concept_Documentation**: 概念文档，解释编译器中的关键概念和术语
- **AI_Assistant**: AI 助手，指 Claude 等大语言模型
- **Compiler_Frontend**: 编译器前端，包括词法、语法、语义分析等模块
- **Compiler_Backend**: 编译器后端，基于 LLVM 的代码生成和优化
- **CHIR**: Cangjie High Level IR，仓颉编译器的中间表示

## Requirements

### Requirement 1: 知识库结构组织

**User Story:** 作为 AI 助手，我希望知识库有清晰的结构组织，以便我能快速定位到相关的文档和信息。

#### Acceptance Criteria

1. THE Knowledge_Base SHALL organize documentation into hierarchical categories including architecture, modules, concepts, and workflows
2. WHEN a module is documented, THE Knowledge_Base SHALL include its purpose, key components, public interfaces, and relationships with other modules
3. THE Knowledge_Base SHALL maintain a root index file that provides an overview of all available documentation categories
4. WHEN documentation is created, THE Knowledge_Base SHALL use consistent naming conventions following kebab-case format
5. THE Knowledge_Base SHALL store all documentation files in the `.claude/knowledge-base/` directory

### Requirement 2: 架构文档

**User Story:** 作为 AI 助手，我希望有完整的架构文档，以便我能理解编译器的整体设计和各模块之间的关系。

#### Acceptance Criteria

1. THE Architecture_Documentation SHALL describe the overall compiler pipeline from source code to executable
2. WHEN describing the pipeline, THE Architecture_Documentation SHALL include all major phases: lexical analysis, parsing, semantic analysis, CHIR generation, and code generation
3. THE Architecture_Documentation SHALL document the relationships and data flow between Compiler_Frontend and Compiler_Backend
4. THE Architecture_Documentation SHALL reference the existing architecture diagram in `figures/Compiler_Architecture_Diagram_zh.png`
5. WHEN a module interacts with other modules, THE Architecture_Documentation SHALL describe the interaction patterns and interfaces

### Requirement 3: 模块文档

**User Story:** 作为 AI 助手，我希望每个主要模块都有详细的文档，以便我能理解模块的功能和实现细节。

#### Acceptance Criteria

1. THE Module_Documentation SHALL be created for each major compiler module including AST, Lex, Parse, Sema, CHIR, CodeGen, Macro, Modules, Mangle, Driver, and Utils
2. WHEN documenting a module, THE Module_Documentation SHALL include sections for purpose, key components, public APIs, data structures, and usage examples
3. THE Module_Documentation SHALL reference the actual source code locations in the `src/` and `include/` directories
4. WHEN a module has dependencies, THE Module_Documentation SHALL list all dependent modules and explain the dependency relationships
5. THE Module_Documentation SHALL include information about the module's role in the compilation pipeline

### Requirement 4: 概念文档

**User Story:** 作为 AI 助手，我希望有关键概念的解释文档，以便我能理解编译器中使用的专业术语和设计模式。

#### Acceptance Criteria

1. THE Concept_Documentation SHALL explain key compiler concepts including AST, CHIR, name mangling, module system, macro expansion, and conditional compilation
2. WHEN explaining a concept, THE Concept_Documentation SHALL provide definitions, use cases, and examples
3. THE Concept_Documentation SHALL cross-reference related modules and architecture components
4. THE Concept_Documentation SHALL include explanations of serialization formats used in the compiler including FlatBuffers schemas
5. WHEN a concept relates to LLVM, THE Concept_Documentation SHALL explain the integration points and customizations

### Requirement 5: 索引系统

**User Story:** 作为 AI 助手，我希望有多层次的索引系统，以便我能通过不同的维度快速找到所需信息。

#### Acceptance Criteria

1. THE Index_System SHALL provide a root index that categorizes all documentation by type: architecture, modules, concepts, and workflows
2. WHEN a user needs module information, THE Index_System SHALL provide a module index listing all documented modules with brief descriptions
3. THE Index_System SHALL provide a concept index listing all documented concepts alphabetically
4. WHEN documentation is added or updated, THE Index_System SHALL be updated to reflect the changes
5. THE Index_System SHALL use relative links to reference documentation files

### Requirement 6: 检索技能文档

**User Story:** 作为 AI 助手，我希望有明确的检索指导，以便我知道如何高效地使用知识库查找信息。

#### Acceptance Criteria

1. THE Retrieval_Skill SHALL be documented in `.claude/skills/use-knowledge-base.md` file
2. WHEN the AI_Assistant needs to find information, THE Retrieval_Skill SHALL provide step-by-step guidance on which index to consult first
3. THE Retrieval_Skill SHALL include decision trees for common query types such as "how does module X work" or "what is concept Y"
4. THE Retrieval_Skill SHALL provide examples of typical queries and the corresponding retrieval paths
5. WHEN multiple documentation sources are relevant, THE Retrieval_Skill SHALL guide the AI_Assistant on how to combine information from different sources

### Requirement 7: 工作流文档

**User Story:** 作为 AI 助手，我希望有常见工作流的文档，以便我能理解如何完成特定的开发任务。

#### Acceptance Criteria

1. THE Knowledge_Base SHALL document common workflows including building the compiler, adding a new language feature, and debugging compilation issues
2. WHEN documenting a workflow, THE Knowledge_Base SHALL provide step-by-step instructions with references to relevant modules and concepts
3. THE Knowledge_Base SHALL reference the existing `build-cangjie.md` skill for build-related workflows
4. WHEN a workflow involves multiple modules, THE Knowledge_Base SHALL explain the interaction sequence
5. THE Knowledge_Base SHALL include troubleshooting guidance for common issues in each workflow

### Requirement 8: 代码导航指引

**User Story:** 作为 AI 助手，我希望有代码导航指引，以便我能快速定位到相关的源代码文件和函数。

#### Acceptance Criteria

1. THE Knowledge_Base SHALL provide a directory structure overview mapping logical modules to physical directories
2. WHEN a module is documented, THE Knowledge_Base SHALL list key source files and header files with their purposes
3. THE Knowledge_Base SHALL document naming conventions used in the codebase for classes, functions, and files
4. WHEN the AI_Assistant needs to find implementation details, THE Knowledge_Base SHALL guide which files to examine first
5. THE Knowledge_Base SHALL document the relationship between `src/` and `include/` directory structures

### Requirement 9: 第三方依赖文档

**User Story:** 作为 AI 助手，我希望了解编译器使用的第三方依赖，以便我能理解外部集成点和约束。

#### Acceptance Criteria

1. THE Knowledge_Base SHALL document all major third-party dependencies including LLVM, FlatBuffers, mingw-w64, and libboundscheck
2. WHEN documenting a dependency, THE Knowledge_Base SHALL explain its purpose, integration points, and version requirements
3. THE Knowledge_Base SHALL reference the patch files in `third_party/` directory and explain their purposes
4. WHEN a module uses a third-party library, THE Module_Documentation SHALL reference the dependency documentation
5. THE Knowledge_Base SHALL document the LLVM customizations and extensions specific to Cangjie compiler

### Requirement 10: 文档维护性

**User Story:** 作为开发者，我希望知识库易于维护和更新，以便文档能与代码保持同步。

#### Acceptance Criteria

1. THE Knowledge_Base SHALL use Markdown format for all documentation files
2. WHEN documentation references source code, THE Knowledge_Base SHALL use relative paths from the repository root
3. THE Knowledge_Base SHALL include metadata in each document indicating last update date and version
4. WHEN the compiler architecture changes, THE Knowledge_Base SHALL provide guidelines for updating affected documentation
5. THE Knowledge_Base SHALL maintain a changelog documenting major updates to the knowledge base structure
