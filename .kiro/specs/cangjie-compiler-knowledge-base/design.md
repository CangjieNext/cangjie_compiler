# Design Document: Cangjie Compiler Knowledge Base System

## Overview

本设计文档描述了仓颉编译器知识库系统的架构和实现方案。该系统旨在为 AI 助手（如 Claude）提供一个结构化、易于检索的知识库，使其能够快速理解仓颉编译器的架构、模块、关键概念和代码组织。

知识库采用分层文档结构，包含架构文档、模块文档、概念文档和工作流文档。通过多层次索引系统，AI 助手可以快速定位到相关信息。同时，配备专门的检索技能文档，指导 AI 如何高效使用知识库。

### Design Goals

1. **易于检索**: 通过清晰的索引和分类，使 AI 能在 1-2 步内找到所需信息
2. **结构化组织**: 采用一致的文档结构和命名规范，降低认知负担
3. **完整覆盖**: 覆盖编译器的所有主要模块、概念和工作流
4. **易于维护**: 使用 Markdown 格式，便于人工更新和版本控制
5. **渐进式构建**: 支持逐步添加文档，不要求一次性完成

## Architecture

### Directory Structure

知识库将采用以下目录结构：

```
cangjie_compiler/
├── .claude/
│   ├── knowledge-base/
│   │   ├── index.md                    # 根索引（人类可读）
│   │   ├── index.json                  # 根索引（机器可读）
│   │   ├── cross-references.json       # 全局交叉引用索引
│   │   ├── search-index.json           # 全局搜索索引
│   │   ├── architecture/
│   │   │   ├── index.md                # 架构文档索引
│   │   │   ├── index.json              # 架构文档元数据索引
│   │   │   ├── overview.md             # 编译器整体架构
│   │   │   ├── overview.json           # 架构概览元数据
│   │   │   ├── pipeline.md             # 编译流程管线
│   │   │   ├── pipeline.json           # 管线元数据
│   │   │   ├── frontend.md             # 前端架构
│   │   │   ├── frontend.json           # 前端元数据
│   │   │   ├── backend.md              # 后端架构（LLVM）
│   │   │   ├── backend.json            # 后端元数据
│   │   │   ├── data-flow.md            # 数据流和模块交互
│   │   │   └── data-flow.json          # 数据流元数据
│   │   ├── modules/
│   │   │   ├── index.md                # 模块文档索引
│   │   │   ├── index.json              # 模块元数据索引
│   │   │   ├── ast.md                  # AST 模块
│   │   │   ├── ast.json                # AST 模块元数据
│   │   │   ├── ast-components.json     # AST 组件详细索引
│   │   │   ├── lex.md                  # 词法分析模块
│   │   │   ├── lex.json                # Lex 模块元数据
│   │   │   ├── lex-components.json     # Lex 组件详细索引
│   │   │   ├── parse.md                # 语法分析模块
│   │   │   ├── parse.json              # Parse 模块元数据
│   │   │   ├── parse-components.json   # Parse 组件详细索引
│   │   │   ├── sema.md                 # 语义分析模块
│   │   │   ├── sema.json               # Sema 模块元数据
│   │   │   ├── sema-components.json    # Sema 组件详细索引
│   │   │   ├── chir.md                 # CHIR 中间表示模块
│   │   │   ├── chir.json               # CHIR 模块元数据
│   │   │   ├── chir-components.json    # CHIR 组件详细索引
│   │   │   ├── codegen.md              # 代码生成模块
│   │   │   ├── codegen.json            # CodeGen 模块元数据
│   │   │   ├── codegen-components.json # CodeGen 组件详细索引
│   │   │   ├── macro.md                # 宏展开模块
│   │   │   ├── macro.json              # Macro 模块元数据
│   │   │   ├── macro-components.json   # Macro 组件详细索引
│   │   │   ├── modules.md              # 包管理模块
│   │   │   ├── modules.json            # Modules 模块元数据
│   │   │   ├── modules-components.json # Modules 组件详细索引
│   │   │   ├── mangle.md               # 名称修饰模块
│   │   │   ├── mangle.json             # Mangle 模块元数据
│   │   │   ├── mangle-components.json  # Mangle 组件详细索引
│   │   │   ├── driver.md               # 编译驱动模块
│   │   │   ├── driver.json             # Driver 模块元数据
│   │   │   ├── driver-components.json  # Driver 组件详细索引
│   │   │   ├── utils.md                # 工具模块
│   │   │   ├── utils.json              # Utils 模块元数据
│   │   │   └── utils-components.json   # Utils 组件详细索引
│   │   ├── concepts/
│   │   │   ├── index.md                # 概念文档索引
│   │   │   ├── index.json              # 概念元数据索引
│   │   │   ├── ast.md                  # AST 概念
│   │   │   ├── ast.json                # AST 概念元数据
│   │   │   ├── chir.md                 # CHIR 概念
│   │   │   ├── chir.json               # CHIR 概念元数据
│   │   │   ├── name-mangling.md        # 名称修饰
│   │   │   ├── name-mangling.json      # 名称修饰元数据
│   │   │   ├── module-system.md        # 模块系统
│   │   │   ├── module-system.json      # 模块系统元数据
│   │   │   ├── macro-expansion.md      # 宏展开
│   │   │   ├── macro-expansion.json    # 宏展开元数据
│   │   │   ├── conditional-compilation.md  # 条件编译
│   │   │   ├── conditional-compilation.json # 条件编译元数据
│   │   │   ├── serialization.md        # 序列化格式
│   │   │   ├── serialization.json      # 序列化元数据
│   │   │   ├── type-system.md          # 类型系统
│   │   │   ├── type-system.json        # 类型系统元数据
│   │   │   ├── llvm-integration.md     # LLVM 集成
│   │   │   └── llvm-integration.json   # LLVM 集成元数据
│   │   ├── workflows/
│   │   │   ├── index.md                # 工作流文档索引
│   │   │   ├── index.json              # 工作流元数据索引
│   │   │   ├── building.md             # 构建编译器
│   │   │   ├── building.json           # 构建工作流元数据
│   │   │   ├── adding-feature.md       # 添加新语言特性
│   │   │   ├── adding-feature.json     # 添加特性工作流元数据
│   │   │   ├── debugging.md            # 调试编译问题
│   │   │   ├── debugging.json          # 调试工作流元数据
│   │   │   ├── testing.md              # 测试策略
│   │   │   └── testing.json            # 测试工作流元数据
│   │   └── code-navigation/
│   │       ├── index.md                # 代码导航索引
│   │       ├── index.json              # 代码导航元数据索引
│   │       ├── directory-structure.md  # 目录结构映射
│   │       ├── directory-structure.json # 目录结构元数据
│   │       ├── naming-conventions.md   # 命名规范
│   │       ├── naming-conventions.json # 命名规范元数据
│   │       ├── key-files.md            # 关键文件清单
│   │       └── key-files.json          # 关键文件元数据
│   └── skills/
│       ├── build-cangjie.md            # 已存在的构建技能
│       └── use-knowledge-base.md       # 新增：知识库检索技能
```

### Document Categories

#### 1. Architecture Documentation (架构文档)

描述编译器的整体架构和设计决策：

- **overview.md**: 编译器整体架构概览，包含架构图引用
- **pipeline.md**: 从源码到可执行文件的完整编译流程
- **frontend.md**: 前端各阶段（Lex → Parse → Sema → CHIR）的详细说明
- **backend.md**: LLVM 后端集成、优化和代码生成
- **data-flow.md**: 模块间的数据流和交互模式

#### 2. Module Documentation (模块文档)

每个主要模块的详细文档，统一结构：

- **Purpose**: 模块的职责和在编译流程中的位置
- **Key Components**: 主要类、函数和数据结构
- **Public APIs**: 对外暴露的接口
- **Dependencies**: 依赖的其他模块
- **Source Locations**: 源码和头文件位置
- **Usage Examples**: 典型使用场景

#### 3. Concept Documentation (概念文档)

解释编译器中的关键概念和术语：

- **Definition**: 概念的定义和背景
- **Purpose**: 为什么需要这个概念
- **Implementation**: 在编译器中如何实现
- **Related Modules**: 相关的模块
- **Examples**: 具体示例

#### 4. Workflow Documentation (工作流文档)

常见开发任务的步骤指南：

- **Objective**: 工作流的目标
- **Prerequisites**: 前置条件
- **Steps**: 详细步骤
- **Involved Modules**: 涉及的模块
- **Troubleshooting**: 常见问题和解决方案

#### 5. Code Navigation (代码导航)

帮助快速定位源码：

- **Directory Structure**: 逻辑模块到物理目录的映射
- **Naming Conventions**: 代码命名规范
- **Key Files**: 每个模块的关键文件列表

### Index System

知识库采用多层索引结构（5层以上），结合 Markdown 索引和 JSON 元数据文件，提供精确的深层检索能力：

#### Level 1: Root Index (根索引)

位于 `.claude/knowledge-base/index.md` 和 `.claude/knowledge-base/index.json`

**index.md** - 人类可读的总览：

```markdown
# Cangjie Compiler Knowledge Base

## Quick Navigation

- [Architecture Documentation](./architecture/index.md) - 编译器架构和设计
- [Module Documentation](./modules/index.md) - 各模块详细说明
- [Concept Documentation](./concepts/index.md) - 关键概念解释
- [Workflow Documentation](./workflows/index.md) - 常见工作流指南
- [Code Navigation](./code-navigation/index.md) - 代码导航指引

## How to Use This Knowledge Base

参见 [Knowledge Base Retrieval Skill](../skills/use-knowledge-base.md)
```

**index.json** - 机器可读的结构化索引：

```json
{
  "version": "1.0.0",
  "lastUpdated": "2024-01-01",
  "categories": [
    {
      "id": "architecture",
      "name": "Architecture Documentation",
      "description": "编译器架构和设计",
      "path": "./architecture/index.md",
      "indexFile": "./architecture/index.json",
      "documentCount": 5
    },
    {
      "id": "modules",
      "name": "Module Documentation",
      "description": "各模块详细说明",
      "path": "./modules/index.md",
      "indexFile": "./modules/index.json",
      "documentCount": 11
    }
  ],
  "tags": ["compiler", "cangjie", "documentation"],
  "searchHints": {
    "moduleQuery": "查找模块信息请访问 modules/index.json",
    "conceptQuery": "查找概念解释请访问 concepts/index.json",
    "architectureQuery": "查找架构信息请访问 architecture/index.json"
  }
}
```

#### Level 2: Category Indexes (分类索引)

每个分类目录下的 `index.md` 和 `index.json`

**modules/index.md** - 人类可读的模块列表：

```markdown
# Module Documentation Index

## Frontend Modules

- [Lex](./lex.md) - 词法分析，将源码转换为 token 流
- [Parse](./parse.md) - 语法分析，构建 AST
- [Sema](./sema.md) - 语义分析，类型检查和推断
- [AST](./ast.md) - 抽象语法树数据结构

## Intermediate Representation

- [CHIR](./chir.md) - 高层中间表示和优化

## Backend Modules

- [CodeGen](./codegen.md) - LLVM IR 代码生成

...
```

**modules/index.json** - 机器可读的模块索引：

```json
{
  "category": "modules",
  "lastUpdated": "2024-01-01",
  "subcategories": [
    {
      "id": "frontend",
      "name": "Frontend Modules",
      "description": "编译器前端模块",
      "modules": [
        {
          "id": "lex",
          "name": "Lex",
          "description": "词法分析，将源码转换为 token 流",
          "path": "./lex.md",
          "metadataFile": "./lex.json",
          "tags": ["frontend", "lexical-analysis", "tokenization"],
          "dependencies": [],
          "relatedConcepts": ["tokenization", "lexical-grammar"],
          "sourceLocations": ["src/Lex/", "include/cangjie/Lex/"]
        },
        {
          "id": "parse",
          "name": "Parse",
          "description": "语法分析，构建 AST",
          "path": "./parse.md",
          "metadataFile": "./parse.json",
          "tags": ["frontend", "parsing", "ast"],
          "dependencies": ["lex", "ast"],
          "relatedConcepts": ["ast", "grammar", "syntax-tree"],
          "sourceLocations": ["src/Parse/", "include/cangjie/Parse/"]
        }
      ]
    },
    {
      "id": "intermediate",
      "name": "Intermediate Representation",
      "description": "中间表示和优化",
      "modules": [
        {
          "id": "chir",
          "name": "CHIR",
          "description": "高层中间表示和优化",
          "path": "./chir.md",
          "metadataFile": "./chir.json",
          "tags": ["ir", "optimization", "chir"],
          "dependencies": ["sema", "ast"],
          "relatedConcepts": ["chir", "ir-optimization"],
          "sourceLocations": ["src/CHIR/", "include/cangjie/CHIR/"]
        }
      ]
    }
  ]
}
```

#### Level 3: Document Metadata (文档元数据)

每个文档配套一个 JSON 元数据文件，提供深层索引信息

**modules/lex.json** - Lex 模块的详细元数据：

```json
{
  "documentId": "module-lex",
  "documentPath": "./lex.md",
  "lastUpdated": "2024-01-01",
  "version": "1.0.0",
  "module": {
    "name": "Lex",
    "fullName": "Lexical Analysis Module",
    "category": "frontend",
    "purpose": "将源代码文本转换为 token 流",
    "pipelineStage": "lexical-analysis",
    "dependencies": [],
    "dependents": ["parse", "macro"]
  },
  "components": [
    {
      "name": "Lexer",
      "type": "class",
      "location": "include/cangjie/Lex/Lexer.h",
      "description": "主词法分析器类",
      "publicAPIs": ["tokenize", "getNextToken", "peekToken"]
    },
    {
      "name": "Token",
      "type": "class",
      "location": "include/cangjie/Lex/Token.h",
      "description": "Token 数据结构",
      "publicAPIs": ["getKind", "getLocation", "getValue"]
    },
    {
      "name": "TokenKind",
      "type": "enum",
      "location": "include/cangjie/Lex/TokenKind.h",
      "description": "Token 类型枚举"
    }
  ],
  "concepts": [
    {
      "name": "tokenization",
      "conceptPath": "../concepts/tokenization.md",
      "relevance": "core"
    },
    {
      "name": "lexical-grammar",
      "conceptPath": "../concepts/lexical-grammar.md",
      "relevance": "core"
    }
  ],
  "sourceFiles": {
    "headers": [
      "include/cangjie/Lex/Lexer.h",
      "include/cangjie/Lex/Token.h",
      "include/cangjie/Lex/TokenKind.h"
    ],
    "implementations": [
      "src/Lex/Lexer.cpp",
      "src/Lex/Token.cpp"
    ],
    "tests": [
      "unittests/Lex/LexerTest.cpp",
      "unittests/Lex/TokenTest.cpp"
    ]
  },
  "tags": ["frontend", "lexical-analysis", "tokenization", "scanner"],
  "searchKeywords": ["词法分析", "token", "lexer", "scanner", "tokenization"]
}
```

#### Level 4: Component Index (组件索引)

为每个模块的关键组件建立索引

**modules/lex-components.json** - Lex 模块组件的详细索引：

```json
{
  "moduleId": "lex",
  "components": {
    "classes": [
      {
        "name": "Lexer",
        "fullName": "cangjie::Lex::Lexer",
        "location": "include/cangjie/Lex/Lexer.h",
        "lineRange": [15, 120],
        "description": "主词法分析器类，负责将源代码转换为 token 流",
        "publicMethods": [
          {
            "name": "tokenize",
            "signature": "std::vector<Token> tokenize(const std::string& source)",
            "description": "将源代码字符串转换为 token 列表",
            "parameters": [
              {"name": "source", "type": "const std::string&", "description": "源代码文本"}
            ],
            "returnType": "std::vector<Token>",
            "throws": ["LexicalError"]
          },
          {
            "name": "getNextToken",
            "signature": "Token getNextToken()",
            "description": "获取下一个 token",
            "returnType": "Token"
          }
        ],
        "relatedClasses": ["Token", "TokenKind", "SourceLocation"],
        "usageExamples": [
          {
            "title": "基本词法分析",
            "code": "Lexer lexer(sourceCode);\nauto tokens = lexer.tokenize();",
            "location": "examples/lex_basic.cpp"
          }
        ]
      },
      {
        "name": "Token",
        "fullName": "cangjie::Lex::Token",
        "location": "include/cangjie/Lex/Token.h",
        "lineRange": [10, 50],
        "description": "表示一个词法单元",
        "publicMethods": [
          {
            "name": "getKind",
            "signature": "TokenKind getKind() const",
            "description": "获取 token 类型",
            "returnType": "TokenKind"
          }
        ]
      }
    ],
    "enums": [
      {
        "name": "TokenKind",
        "location": "include/cangjie/Lex/TokenKind.h",
        "values": [
          {"name": "Identifier", "description": "标识符"},
          {"name": "Keyword", "description": "关键字"},
          {"name": "Literal", "description": "字面量"}
        ]
      }
    ],
    "functions": []
  }
}
```

#### Level 5: Cross-Reference Index (交叉引用索引)

建立文档、模块、概念之间的交叉引用网络

**cross-references.json** - 全局交叉引用索引：

```json
{
  "version": "1.0.0",
  "lastUpdated": "2024-01-01",
  "references": {
    "modules": {
      "lex": {
        "referencedBy": {
          "modules": ["parse", "macro", "driver"],
          "concepts": ["tokenization", "lexical-grammar"],
          "workflows": ["adding-feature", "debugging"]
        },
        "references": {
          "modules": [],
          "concepts": ["tokenization", "lexical-grammar"],
          "architecture": ["frontend", "pipeline"]
        }
      },
      "parse": {
        "referencedBy": {
          "modules": ["sema", "macro", "driver"],
          "concepts": ["ast", "grammar"],
          "workflows": ["adding-feature"]
        },
        "references": {
          "modules": ["lex", "ast"],
          "concepts": ["ast", "grammar", "syntax-tree"],
          "architecture": ["frontend", "pipeline"]
        }
      }
    },
    "concepts": {
      "ast": {
        "referencedBy": {
          "modules": ["parse", "sema", "chir"],
          "concepts": ["syntax-tree", "type-system"],
          "workflows": ["adding-feature"]
        },
        "references": {
          "modules": ["ast", "parse"],
          "concepts": ["syntax-tree"],
          "architecture": ["frontend"]
        }
      }
    }
  },
  "dependencyGraph": {
    "modules": {
      "lex": {"dependencies": [], "dependents": ["parse", "macro"]},
      "parse": {"dependencies": ["lex", "ast"], "dependents": ["sema", "macro"]},
      "sema": {"dependencies": ["parse", "ast"], "dependents": ["chir"]},
      "chir": {"dependencies": ["sema", "ast"], "dependents": ["codegen"]},
      "codegen": {"dependencies": ["chir"], "dependents": []}
    }
  }
}
```

#### Level 6: Search Index (搜索索引)

提供快速全文搜索和关键词查找

**search-index.json** - 全局搜索索引：

```json
{
  "version": "1.0.0",
  "lastUpdated": "2024-01-01",
  "index": {
    "keywords": {
      "词法分析": {
        "documents": [
          {"type": "module", "id": "lex", "path": "modules/lex.md", "relevance": 1.0},
          {"type": "concept", "id": "tokenization", "path": "concepts/tokenization.md", "relevance": 0.9},
          {"type": "architecture", "id": "frontend", "path": "architecture/frontend.md", "relevance": 0.7}
        ]
      },
      "AST": {
        "documents": [
          {"type": "module", "id": "ast", "path": "modules/ast.md", "relevance": 1.0},
          {"type": "module", "id": "parse", "path": "modules/parse.md", "relevance": 0.9},
          {"type": "concept", "id": "ast", "path": "concepts/ast.md", "relevance": 1.0}
        ]
      },
      "CHIR": {
        "documents": [
          {"type": "module", "id": "chir", "path": "modules/chir.md", "relevance": 1.0},
          {"type": "concept", "id": "chir", "path": "concepts/chir.md", "relevance": 1.0},
          {"type": "architecture", "id": "pipeline", "path": "architecture/pipeline.md", "relevance": 0.8}
        ]
      }
    },
    "sourceFiles": {
      "include/cangjie/Lex/Lexer.h": {
        "module": "lex",
        "component": "Lexer",
        "documentation": "modules/lex.md#lexer",
        "metadata": "modules/lex.json"
      },
      "src/Parse/Parser.cpp": {
        "module": "parse",
        "component": "Parser",
        "documentation": "modules/parse.md#parser",
        "metadata": "modules/parse.json"
      }
    },
    "concepts": {
      "tokenization": {"path": "concepts/tokenization.md", "relatedModules": ["lex"]},
      "ast": {"path": "concepts/ast.md", "relatedModules": ["ast", "parse", "sema"]},
      "chir": {"path": "concepts/chir.md", "relatedModules": ["chir", "codegen"]}
    }
  },
  "aliases": {
    "lexer": "lex",
    "parser": "parse",
    "semantic analysis": "sema",
    "code generation": "codegen",
    "抽象语法树": "ast",
    "中间表示": "chir"
  }
}
```

#### Index Hierarchy Summary

```
Level 1: Root Index (index.md + index.json)
  ├─ 提供知识库总览和分类导航
  └─ 包含搜索提示和使用指南

Level 2: Category Indexes (category/index.md + category/index.json)
  ├─ 列出该分类下的所有文档
  ├─ 提供子分类组织
  └─ 包含文档的基本元数据

Level 3: Document Metadata (document.json)
  ├─ 每个文档的详细元数据
  ├─ 包含组件列表、依赖关系、相关概念
  └─ 提供源码位置和标签

Level 4: Component Index (module-components.json)
  ├─ 模块内部组件的详细索引
  ├─ 包含类、函数、枚举的详细信息
  └─ 提供 API 签名和使用示例

Level 5: Cross-Reference Index (cross-references.json)
  ├─ 全局交叉引用网络
  ├─ 依赖关系图
  └─ 双向引用关系

Level 6: Search Index (search-index.json)
  ├─ 关键词到文档的映射
  ├─ 源文件到文档的映射
  └─ 概念和别名索引
```

## Components and Interfaces

### Knowledge Base Manager

虽然知识库主要是静态文档，但我们定义一个概念性的"管理器"来描述其组织原则：

```typescript
interface KnowledgeBaseManager {
  // 文档组织原则
  categories: {
    architecture: ArchitectureDoc[]
    modules: ModuleDoc[]
    concepts: ConceptDoc[]
    workflows: WorkflowDoc[]
    codeNavigation: CodeNavigationDoc[]
  }
  
  // 索引系统
  indexes: {
    root: RootIndex
    categoryIndexes: Map<Category, CategoryIndex>
  }
  
  // 文档模板
  templates: {
    moduleTemplate: ModuleDocTemplate
    conceptTemplate: ConceptDocTemplate
    workflowTemplate: WorkflowDocTemplate
  }
}
```

### Document Templates

#### Module Document Template

```markdown
# Module: {ModuleName}

**Last Updated**: {Date}  
**Version**: {CompilerVersion}

## Purpose

{模块的职责和在编译流程中的位置}

## Key Components

### {ComponentName}

- **Type**: Class/Function/Data Structure
- **Location**: `{source_file_path}`
- **Description**: {简要说明}

## Public APIs

### {APIName}

```cpp
{API signature}
```

{API 说明和使用场景}

## Dependencies

- **{ModuleName}**: {依赖原因}

## Source Locations

- **Headers**: `include/cangjie/{Module}/`
- **Implementation**: `src/{Module}/`
- **Tests**: `unittests/{Module}/`

## Usage Examples

{典型使用场景和代码示例}

## Related Concepts

- [{ConceptName}](../concepts/{concept}.md)

## Related Workflows

- [{WorkflowName}](../workflows/{workflow}.md)
```

#### Concept Document Template

```markdown
# Concept: {ConceptName}

**Last Updated**: {Date}

## Definition

{概念的定义和背景}

## Purpose

{为什么需要这个概念，解决什么问题}

## Implementation in Cangjie Compiler

{在编译器中如何实现这个概念}

### Key Data Structures

{相关的数据结构}

### Key Algorithms

{相关的算法}

## Related Modules

- [{ModuleName}](../modules/{module}.md): {关系说明}

## Examples

{具体示例，可以是代码或图示}

## Further Reading

{相关资源链接}
```

#### Workflow Document Template

```markdown
# Workflow: {WorkflowName}

**Last Updated**: {Date}

## Objective

{工作流的目标}

## Prerequisites

- {前置条件1}
- {前置条件2}

## Steps

### Step 1: {StepName}

{详细说明}

**Involved Modules**: [{Module1}](../modules/{module1}.md), [{Module2}](../modules/{module2}.md)

**Commands**:
```bash
{相关命令}
```

### Step 2: {StepName}

...

## Troubleshooting

### Issue: {IssueDescription}

**Symptoms**: {症状}

**Cause**: {原因}

**Solution**: {解决方案}

## Related Documentation

- [{DocName}](../path/to/doc.md)
```

### Retrieval Skill Document

位于 `.claude/skills/use-knowledge-base.md`，指导 AI 如何使用知识库：

```markdown
# Skill: Use Cangjie Compiler Knowledge Base

## Overview

本技能指导如何高效使用仓颉编译器知识库查找信息。知识库采用多层索引结构（6层），结合 Markdown 文档和 JSON 元数据，提供精确的深层检索能力。

## Knowledge Base Location

知识库位于 `.claude/knowledge-base/` 目录。

## Index System Overview

知识库采用 6 层索引结构：

1. **Level 1: Root Index** - 知识库总览（index.md + index.json）
2. **Level 2: Category Indexes** - 分类索引（category/index.md + category/index.json）
3. **Level 3: Document Metadata** - 文档元数据（document.json）
4. **Level 4: Component Index** - 组件详细索引（module-components.json）
5. **Level 5: Cross-Reference Index** - 交叉引用网络（cross-references.json）
6. **Level 6: Search Index** - 全局搜索索引（search-index.json）

## Retrieval Strategy

### Step 1: Identify Query Type and Choose Entry Point

根据问题类型选择最佳入口点：

#### 快速查询（使用 JSON 索引）

- **"模块 X 的 API 是什么？"** → 直接查看 `modules/{module}.json` 的 components 字段
- **"源文件 Y 在哪个模块？"** → 查看 `search-index.json` 的 sourceFiles 字段
- **"概念 Z 相关的模块有哪些？"** → 查看 `cross-references.json` 的 concepts 字段
- **"模块 A 依赖哪些模块？"** → 查看 `cross-references.json` 的 dependencyGraph 字段

#### 深入理解（使用 Markdown 文档）

- **"模块 X 是如何工作的？"** → 查看 `modules/index.md` → `modules/{module}.md`
- **"什么是概念 Y？"** → 查看 `concepts/index.md` → `concepts/{concept}.md`
- **"如何完成任务 Z？"** → 查看 `workflows/index.md` → `workflows/{workflow}.md`
- **"整体架构是怎样的？"** → 查看 `architecture/index.md` → `architecture/{aspect}.md`

### Step 2: Use JSON Metadata for Precise Lookup

JSON 文件提供结构化的精确查询能力：

#### 查找模块信息

```javascript
// 1. 查看模块索引获取概览
const moduleIndex = readJSON('.claude/knowledge-base/modules/index.json');
const lexModule = moduleIndex.subcategories
  .find(cat => cat.id === 'frontend')
  .modules.find(m => m.id === 'lex');

// 2. 查看模块元数据获取详细信息
const lexMetadata = readJSON('.claude/knowledge-base/modules/lex.json');
console.log(lexMetadata.components);  // 获取所有组件
console.log(lexMetadata.sourceFiles); // 获取源文件位置

// 3. 查看组件索引获取 API 详情
const lexComponents = readJSON('.claude/knowledge-base/modules/lex-components.json');
const lexerClass = lexComponents.components.classes.find(c => c.name === 'Lexer');
console.log(lexerClass.publicMethods); // 获取所有公共方法
```

#### 查找依赖关系

```javascript
// 查看交叉引用索引
const crossRefs = readJSON('.claude/knowledge-base/cross-references.json');

// 查找模块的依赖
const parseDeps = crossRefs.dependencyGraph.modules.parse;
console.log(parseDeps.dependencies); // ['lex', 'ast']
console.log(parseDeps.dependents);   // ['sema', 'macro']

// 查找模块被哪些文档引用
const lexRefs = crossRefs.references.modules.lex;
console.log(lexRefs.referencedBy.modules);  // 引用 lex 的模块
console.log(lexRefs.referencedBy.concepts); // 引用 lex 的概念
```

#### 搜索关键词

```javascript
// 使用搜索索引
const searchIndex = readJSON('.claude/knowledge-base/search-index.json');

// 搜索关键词
const astDocs = searchIndex.index.keywords['AST'];
console.log(astDocs.documents); // 所有提到 AST 的文档，按相关性排序

// 查找源文件对应的文档
const lexerDoc = searchIndex.index.sourceFiles['include/cangjie/Lex/Lexer.h'];
console.log(lexerDoc.documentation); // 'modules/lex.md#lexer'

// 使用别名
const alias = searchIndex.aliases['抽象语法树']; // 'ast'
```

### Step 3: Navigate to Markdown Documents for Details

使用 JSON 索引定位后，阅读 Markdown 文档获取详细说明：

```markdown
1. 从 JSON 获取文档路径
2. 打开对应的 .md 文件
3. 阅读相关章节
4. 跟随交叉引用链接深入了解
```

### Step 4: Cross-Reference for Related Information

利用交叉引用网络发现相关信息：

```javascript
// 从模块元数据获取相关概念
const lexMetadata = readJSON('.claude/knowledge-base/modules/lex.json');
lexMetadata.concepts.forEach(concept => {
  console.log(`Related concept: ${concept.name} at ${concept.conceptPath}`);
});

// 从交叉引用索引获取引用关系
const crossRefs = readJSON('.claude/knowledge-base/cross-references.json');
const astRefs = crossRefs.references.concepts.ast;
console.log('AST concept is referenced by:', astRefs.referencedBy);
```

## Common Query Patterns

### Pattern 1: Understanding a Module

**Query**: "Parse 模块是如何工作的？"

**Retrieval Path**:
1. 快速查看：`modules/parse.json` → 获取 purpose、components、dependencies
2. 详细了解：`modules/parse.md` → 阅读 Purpose、Key Components、Public APIs
3. 查看组件：`modules/parse-components.json` → 获取 Parser 类的详细 API
4. 相关概念：从 parse.json 的 concepts 字段找到相关概念链接
5. 依赖关系：`cross-references.json` → 查看 parse 的依赖和被依赖关系

### Pattern 2: Understanding a Concept

**Query**: "什么是 CHIR？"

**Retrieval Path**:
1. 快速查看：`concepts/chir.json` → 获取定义和相关模块
2. 详细了解：`concepts/chir.md` → 阅读 Definition、Purpose、Implementation
3. 相关模块：从 chir.json 的 relatedModules 找到 chir 模块
4. 交叉引用：`cross-references.json` → 查看哪些模块使用了 CHIR

### Pattern 3: Finding API Details

**Query**: "Lexer 类有哪些公共方法？"

**Retrieval Path**:
1. 搜索索引：`search-index.json` → 搜索 "Lexer" 找到所属模块
2. 组件索引：`modules/lex-components.json` → 找到 Lexer 类
3. 查看方法：读取 publicMethods 数组，获取所有方法签名和说明
4. 使用示例：查看 usageExamples 字段获取代码示例

### Pattern 4: Finding Source Code

**Query**: "词法分析的代码在哪里？"

**Retrieval Path**:
1. 模块元数据：`modules/lex.json` → 查看 sourceFiles 字段
2. 或搜索索引：`search-index.json` → 查看 sourceFiles 映射
3. 或模块文档：`modules/lex.md` → 查看 Source Locations 章节
4. 组件定位：`modules/lex-components.json` → 查看具体类的 location 和 lineRange

### Pattern 5: Understanding Dependencies

**Query**: "Parse 模块依赖哪些模块？"

**Retrieval Path**:
1. 快速查看：`cross-references.json` → dependencyGraph.modules.parse.dependencies
2. 或模块元数据：`modules/parse.json` → module.dependencies 字段
3. 详细说明：`modules/parse.md` → Dependencies 章节

### Pattern 6: Finding Related Documentation

**Query**: "AST 相关的所有文档有哪些？"

**Retrieval Path**:
1. 搜索索引：`search-index.json` → keywords['AST'].documents
2. 交叉引用：`cross-references.json` → references.concepts.ast.referencedBy
3. 按相关性排序：搜索索引中的 relevance 字段

## Decision Tree

```
用户问题
├─ 需要快速查询？
│  ├─ API 详情？ → {module}-components.json
│  ├─ 依赖关系？ → cross-references.json
│  ├─ 源文件位置？ → search-index.json 或 {module}.json
│  └─ 关键词搜索？ → search-index.json
│
└─ 需要深入理解？
   ├─ 关于模块？ → modules/index.md → modules/{module}.md
   ├─ 关于概念？ → concepts/index.md → concepts/{concept}.md
   ├─ 关于任务？ → workflows/index.md → workflows/{workflow}.md
   ├─ 关于架构？ → architecture/index.md → architecture/{aspect}.md
   └─ 关于代码位置？ → code-navigation/index.md
```

## Retrieval Workflow

### Workflow 1: Quick Fact Lookup

```
1. 确定查询类型（API、依赖、源文件等）
2. 选择对应的 JSON 索引文件
3. 解析 JSON 获取精确信息
4. 如需更多细节，跟随路径到 Markdown 文档
```

### Workflow 2: Deep Understanding

```
1. 从根索引或分类索引开始
2. 导航到相关的 Markdown 文档
3. 阅读文档内容
4. 使用 JSON 元数据获取结构化信息
5. 跟随交叉引用链接探索相关内容
```

### Workflow 3: Exploratory Search

```
1. 使用 search-index.json 搜索关键词
2. 获取相关文档列表（按相关性排序）
3. 查看每个文档的元数据（.json 文件）
4. 选择最相关的文档深入阅读
5. 使用 cross-references.json 发现更多相关内容
```

## Tips

1. **JSON 优先用于精确查询**: 当你需要具体的 API、依赖关系、源文件位置时，直接查看 JSON 文件
2. **Markdown 用于理解和学习**: 当你需要理解概念、工作流程、设计决策时，阅读 Markdown 文档
3. **利用搜索索引**: search-index.json 提供关键词到文档的快速映射
4. **利用交叉引用**: cross-references.json 帮助你发现模块、概念之间的关系
5. **组件索引很详细**: {module}-components.json 包含类、函数的完整签名和示例
6. **注意别名**: search-index.json 中的 aliases 字段支持中英文查询
7. **检查元数据日期**: 注意 lastUpdated 字段，确保信息是最新的
8. **依赖图很有用**: dependencyGraph 可以帮助你理解模块的编译顺序和依赖链

## Examples

### Example 1: 查找 Lexer 类的 tokenize 方法

```javascript
// Step 1: 打开组件索引
const lexComponents = readJSON('.claude/knowledge-base/modules/lex-components.json');

// Step 2: 找到 Lexer 类
const lexerClass = lexComponents.components.classes.find(c => c.name === 'Lexer');

// Step 3: 找到 tokenize 方法
const tokenizeMethod = lexerClass.publicMethods.find(m => m.name === 'tokenize');

// Step 4: 获取方法详情
console.log(tokenizeMethod.signature);    // 方法签名
console.log(tokenizeMethod.description);  // 方法说明
console.log(tokenizeMethod.parameters);   // 参数列表
console.log(tokenizeMethod.returnType);   // 返回类型
```

### Example 2: 查找所有依赖 AST 模块的模块

```javascript
// Step 1: 打开交叉引用索引
const crossRefs = readJSON('.claude/knowledge-base/cross-references.json');

// Step 2: 查看 AST 模块的被依赖关系
const astDependents = crossRefs.dependencyGraph.modules.ast.dependents;

console.log('Modules that depend on AST:', astDependents);
// Output: ['parse', 'sema', 'chir', ...]
```

### Example 3: 搜索"类型系统"相关的所有文档

```javascript
// Step 1: 打开搜索索引
const searchIndex = readJSON('.claude/knowledge-base/search-index.json');

// Step 2: 搜索关键词（支持中文）
const typeDocs = searchIndex.index.keywords['类型系统'] || 
                 searchIndex.index.keywords['type system'];

// Step 3: 按相关性排序查看结果
typeDocs.documents
  .sort((a, b) => b.relevance - a.relevance)
  .forEach(doc => {
    console.log(`${doc.type}: ${doc.path} (relevance: ${doc.relevance})`);
  });
```
```

## Data Models

### Root Index Structure

根索引的 JSON 结构：

```typescript
interface RootIndex {
  version: string
  lastUpdated: string
  categories: CategoryReference[]
  tags: string[]
  searchHints: {
    [queryType: string]: string
  }
}

interface CategoryReference {
  id: string
  name: string
  description: string
  path: string           // Markdown 文件路径
  indexFile: string      // JSON 索引文件路径
  documentCount: number
}
```

### Category Index Structure

分类索引的 JSON 结构：

```typescript
interface CategoryIndex {
  category: string
  lastUpdated: string
  subcategories: Subcategory[]
}

interface Subcategory {
  id: string
  name: string
  description: string
  modules?: ModuleReference[]      // 用于 modules 分类
  documents?: DocumentReference[]  // 用于其他分类
}

interface ModuleReference {
  id: string
  name: string
  description: string
  path: string                    // Markdown 文件路径
  metadataFile: string            // JSON 元数据文件路径
  tags: string[]
  dependencies: string[]          // 依赖的模块 ID
  relatedConcepts: string[]       // 相关概念 ID
  sourceLocations: string[]       // 源码目录路径
}

interface DocumentReference {
  id: string
  name: string
  description: string
  path: string
  metadataFile: string
  tags: string[]
}
```

### Document Metadata Structure

每个文档的元数据 JSON 结构：

```typescript
interface DocumentMetadata {
  documentId: string
  documentPath: string
  lastUpdated: string
  version: string
  module?: ModuleMetadata      // 用于模块文档
  concept?: ConceptMetadata    // 用于概念文档
  workflow?: WorkflowMetadata  // 用于工作流文档
  architecture?: ArchitectureMetadata  // 用于架构文档
}

interface ModuleMetadata {
  name: string
  fullName: string
  category: string
  purpose: string
  pipelineStage: string
  dependencies: string[]
  dependents: string[]
  components: ComponentReference[]
  concepts: ConceptReference[]
  sourceFiles: SourceFileLocations
  tags: string[]
  searchKeywords: string[]
}

interface ComponentReference {
  name: string
  type: 'class' | 'function' | 'enum' | 'interface' | 'struct'
  location: string
  description: string
  publicAPIs?: string[]
}

interface ConceptReference {
  name: string
  conceptPath: string
  relevance: 'core' | 'related' | 'tangential'
}

interface SourceFileLocations {
  headers: string[]
  implementations: string[]
  tests: string[]
}

interface ConceptMetadata {
  name: string
  definition: string
  purpose: string
  implementation: {
    dataStructures: string[]
    algorithms: string[]
    keyFiles: string[]
  }
  relatedModules: {
    moduleName: string
    relationship: string
  }[]
  examples: Example[]
  furtherReading: Resource[]
}

interface WorkflowMetadata {
  name: string
  objective: string
  prerequisites: string[]
  steps: StepReference[]
  troubleshooting: TroubleshootingReference[]
  relatedDocs: string[]
}

interface StepReference {
  number: number
  name: string
  involvedModules: string[]
}

interface TroubleshootingReference {
  issue: string
  symptoms: string[]
}

interface ArchitectureMetadata {
  name: string
  description: string
  components: string[]
  relatedModules: string[]
  diagrams: string[]
}
```

### Component Index Structure

模块组件的详细索引结构：

```typescript
interface ComponentIndex {
  moduleId: string
  components: {
    classes: ClassComponent[]
    functions: FunctionComponent[]
    enums: EnumComponent[]
    interfaces: InterfaceComponent[]
  }
}

interface ClassComponent {
  name: string
  fullName: string
  location: string
  lineRange: [number, number]
  description: string
  publicMethods: MethodSignature[]
  publicFields?: FieldSignature[]
  relatedClasses: string[]
  usageExamples: Example[]
}

interface MethodSignature {
  name: string
  signature: string
  description: string
  parameters: Parameter[]
  returnType: string
  throws?: string[]
}

interface Parameter {
  name: string
  type: string
  description: string
  optional?: boolean
  defaultValue?: string
}

interface FieldSignature {
  name: string
  type: string
  description: string
  visibility: 'public' | 'protected' | 'private'
}

interface FunctionComponent {
  name: string
  fullName: string
  location: string
  lineRange: [number, number]
  signature: string
  description: string
  parameters: Parameter[]
  returnType: string
  usageExamples: Example[]
}

interface EnumComponent {
  name: string
  location: string
  values: EnumValue[]
}

interface EnumValue {
  name: string
  value?: string | number
  description: string
}

interface InterfaceComponent {
  name: string
  fullName: string
  location: string
  description: string
  methods: MethodSignature[]
}

interface Example {
  title: string
  description: string
  code?: string
  location?: string
}

interface Resource {
  title: string
  url: string
  type: 'Documentation' | 'Paper' | 'Blog' | 'Code'
}
```

### Cross-Reference Index Structure

交叉引用索引结构：

```typescript
interface CrossReferenceIndex {
  version: string
  lastUpdated: string
  references: {
    modules: {
      [moduleId: string]: ModuleReferences
    }
    concepts: {
      [conceptId: string]: ConceptReferences
    }
    workflows: {
      [workflowId: string]: WorkflowReferences
    }
  }
  dependencyGraph: {
    modules: {
      [moduleId: string]: DependencyInfo
    }
  }
}

interface ModuleReferences {
  referencedBy: {
    modules: string[]
    concepts: string[]
    workflows: string[]
    architecture: string[]
  }
  references: {
    modules: string[]
    concepts: string[]
    architecture: string[]
  }
}

interface ConceptReferences {
  referencedBy: {
    modules: string[]
    concepts: string[]
    workflows: string[]
  }
  references: {
    modules: string[]
    concepts: string[]
  }
}

interface WorkflowReferences {
  referencedBy: {
    workflows: string[]
  }
  references: {
    modules: string[]
    concepts: string[]
    workflows: string[]
  }
}

interface DependencyInfo {
  dependencies: string[]
  dependents: string[]
}
```

### Search Index Structure

搜索索引结构：

```typescript
interface SearchIndex {
  version: string
  lastUpdated: string
  index: {
    keywords: {
      [keyword: string]: KeywordResult
    }
    sourceFiles: {
      [filePath: string]: SourceFileInfo
    }
    concepts: {
      [conceptId: string]: ConceptInfo
    }
  }
  aliases: {
    [alias: string]: string
  }
}

interface KeywordResult {
  documents: SearchResult[]
}

interface SearchResult {
  type: 'module' | 'concept' | 'workflow' | 'architecture'
  id: string
  path: string
  relevance: number  // 0.0 到 1.0
}

interface SourceFileInfo {
  module: string
  component?: string
  documentation: string
  metadata: string
}

interface ConceptInfo {
  path: string
  relatedModules: string[]
}
```

## 

## Correctness Properties

*属性（Property）是关于系统行为的特征，应该在所有有效执行中保持为真——本质上是关于系统应该做什么的形式化陈述。属性是人类可读规范和机器可验证正确性保证之间的桥梁。*

基于需求文档中的验收标准分析，我们定义以下正确性属性：

### Property 1: 文档结构完整性

*对于任何*类型的文档（模块文档、概念文档、工作流文档），该文档都应包含其类型所要求的所有必需章节。

具体而言：
- 模块文档必须包含：Purpose、Key Components、Public APIs、Dependencies、Source Locations、Usage Examples
- 概念文档必须包含：Definition、Purpose、Implementation、Related Modules、Examples
- 工作流文档必须包含：Objective、Prerequisites、Steps、Troubleshooting

**Validates: Requirements 1.2, 3.2, 4.2, 7.2, 7.5**

### Property 2: 文档格式规范

*对于任何*知识库中的文档文件，该文件应：
- 使用 Markdown 格式（.md 扩展名）
- 文件名遵循 kebab-case 命名规范
- 包含元数据（Last Updated 和 Version 字段）
- 配套一个同名的 JSON 元数据文件（除了索引文件）

**Validates: Requirements 1.4, 10.1, 10.3**

### Property 2.1: JSON 索引完整性

*对于任何*分类目录（architecture、modules、concepts、workflows、code-navigation），该目录应包含：
- index.md（人类可读索引）
- index.json（机器可读索引）
- 每个文档的 .md 文件
- 每个文档对应的 .json 元数据文件

**Validates: Requirements 1.1, 5.1, 5.2**

### Property 2.2: JSON 结构有效性

*对于任何* JSON 文件，该文件应：
- 是有效的 JSON 格式（可被解析）
- 包含必需的顶层字段（根据文件类型）
- 所有引用的路径都是相对路径

**Validates: Requirements 5.5, 10.2**

### Property 3: 相对路径引用

*对于任何*文档中的链接和源码引用，都应使用相对于仓库根目录的相对路径，而不是绝对路径。

**Validates: Requirements 5.5, 10.2**

### Property 4: 模块文档源码引用

*对于任何*模块文档，该文档应包含对实际源码位置的引用，指向 `src/` 或 `include/` 目录中的文件。

**Validates: Requirements 3.3, 8.2**

### Property 5: 概念文档交叉引用

*对于任何*概念文档，该文档应包含至少一个指向相关模块文档的交叉引用链接。

**Validates: Requirements 4.3**

### Property 6: 索引排序一致性

*对于*概念索引文件，其中列出的概念条目应按字母顺序排列。

**Validates: Requirements 5.3**

### Property 7: 架构文档完整性

*对于*架构文档（pipeline.md），该文档应提及所有主要编译阶段：词法分析（lexical analysis/lex）、语法分析（parsing/parse）、语义分析（semantic analysis/sema）、CHIR 生成（CHIR generation）和代码生成（code generation/codegen）。

**Validates: Requirements 2.2**

### Property 8: 依赖文档信息完整性

*对于任何*第三方依赖的文档，该文档应包含依赖的用途（purpose）、集成点（integration points）和版本要求（version requirements）的说明。

**Validates: Requirements 9.2**

### Property 9: 组件索引完整性

*对于任何*模块，如果存在 {module}-components.json 文件，该文件应包含该模块的所有公共类、函数和枚举的详细信息，包括方法签名、参数和返回类型。

**Validates: Requirements 3.2, 3.3**

### Property 10: 交叉引用一致性

*对于任何*在 cross-references.json 中列出的模块依赖关系，该依赖关系应与对应模块的 {module}.json 元数据文件中的 dependencies 字段一致。

**Validates: Requirements 3.4, 5.4**

### Property 11: 搜索索引覆盖性

*对于任何*存在的文档（.md 文件），search-index.json 应包含至少一个指向该文档的关键词条目。

**Validates: Requirements 5.1, 5.2**

### Property 12: JSON 路径有效性

*对于任何* JSON 文件中引用的文档路径（path、documentPath、conceptPath 等字段），该路径指向的文件应实际存在。

**Validates: Requirements 5.5, 10.2**

## Error Handling

知识库系统主要是静态文档集合，错误处理主要关注文档创建和维护过程中的问题：

### 文档缺失处理

**场景**: AI 助手尝试访问不存在的文档

**处理策略**:
- 检索技能文档应指导 AI 从索引开始导航，而不是直接猜测文档路径
- 索引文件应保持最新，准确反映现有文档
- 如果文档缺失，AI 应回退到相关的上层文档或索引

### 文档格式错误

**场景**: 文档不符合模板结构或缺少必需章节

**处理策略**:
- 提供清晰的文档模板和示例
- 在文档创建时使用模板作为起点
- 定期审查文档结构完整性

### 链接失效

**场景**: 文档中的交叉引用链接指向不存在的文件

**处理策略**:
- 使用相对路径，便于重构和移动
- 在添加新文档时更新相关索引
- 定期检查链接有效性（可通过脚本自动化）

### 信息过时

**场景**: 文档内容与代码库当前状态不一致

**处理策略**:
- 在每个文档中包含 "Last Updated" 元数据
- 在代码重大变更时，更新相关文档
- 维护 changelog 记录知识库的重要更新

### 文档冲突

**场景**: 不同文档对同一概念或模块的描述不一致

**处理策略**:
- 建立单一信息源原则：每个概念或模块有一个主要文档
- 其他文档通过交叉引用链接到主要文档，而不是重复信息
- 定期审查文档一致性

## Testing Strategy

知识库系统的测试策略包括两个层面：文档结构验证和内容质量保证。

### 单元测试

单元测试关注具体的文档存在性和结构验证：

#### 文件存在性测试

验证关键文档和索引文件是否存在：

```python
def test_root_index_exists():
    """验证根索引文件存在（Markdown 和 JSON）"""
    assert os.path.exists('.claude/knowledge-base/index.md')
    assert os.path.exists('.claude/knowledge-base/index.json')

def test_global_indexes_exist():
    """验证全局索引文件存在"""
    assert os.path.exists('.claude/knowledge-base/cross-references.json')
    assert os.path.exists('.claude/knowledge-base/search-index.json')

def test_category_indexes_exist():
    """验证所有分类索引存在（Markdown 和 JSON）"""
    categories = ['architecture', 'modules', 'concepts', 'workflows', 'code-navigation']
    for category in categories:
        assert os.path.exists(f'.claude/knowledge-base/{category}/index.md')
        assert os.path.exists(f'.claude/knowledge-base/{category}/index.json')

def test_required_module_docs_exist():
    """验证所有必需的模块文档存在（Markdown 和 JSON）"""
    required_modules = ['ast', 'lex', 'parse', 'sema', 'chir', 'codegen', 
                       'macro', 'modules', 'mangle', 'driver', 'utils']
    for module in required_modules:
        assert os.path.exists(f'.claude/knowledge-base/modules/{module}.md')
        assert os.path.exists(f'.claude/knowledge-base/modules/{module}.json')
        assert os.path.exists(f'.claude/knowledge-base/modules/{module}-components.json')

def test_retrieval_skill_exists():
    """验证检索技能文档存在"""
    assert os.path.exists('.claude/skills/use-knowledge-base.md')
```

#### JSON 格式验证测试

验证 JSON 文件的格式和结构：

```python
import json

def test_json_files_are_valid():
    """验证所有 JSON 文件都是有效的 JSON 格式"""
    json_files = []
    for root, dirs, files in os.walk('.claude/knowledge-base'):
        for file in files:
            if file.endswith('.json'):
                json_files.append(os.path.join(root, file))
    
    for json_file in json_files:
        with open(json_file) as f:
            try:
                json.load(f)
            except json.JSONDecodeError as e:
                pytest.fail(f"Invalid JSON in {json_file}: {e}")

def test_root_index_json_structure():
    """验证根索引 JSON 结构"""
    with open('.claude/knowledge-base/index.json') as f:
        data = json.load(f)
    
    assert 'version' in data
    assert 'lastUpdated' in data
    assert 'categories' in data
    assert isinstance(data['categories'], list)
    
    for category in data['categories']:
        assert 'id' in category
        assert 'name' in category
        assert 'path' in category
        assert 'indexFile' in category

def test_module_index_json_structure():
    """验证模块索引 JSON 结构"""
    with open('.claude/knowledge-base/modules/index.json') as f:
        data = json.load(f)
    
    assert 'category' in data
    assert data['category'] == 'modules'
    assert 'subcategories' in data
    
    for subcat in data['subcategories']:
        assert 'id' in subcat
        assert 'modules' in subcat
        for module in subcat['modules']:
            assert 'id' in module
            assert 'path' in module
            assert 'metadataFile' in module
            assert 'tags' in module
            assert 'dependencies' in module
```

#### 特定内容验证测试

验证特定文档包含必需的内容：

```python
def test_architecture_diagram_reference():
    """验证架构文档引用了架构图"""
    with open('.claude/knowledge-base/architecture/overview.md') as f:
        content = f.read()
        assert 'Compiler_Architecture_Diagram_zh.png' in content

def test_build_skill_reference():
    """验证工作流文档引用了构建技能"""
    with open('.claude/knowledge-base/workflows/building.md') as f:
        content = f.read()
        assert 'build-cangjie.md' in content

def test_third_party_dependencies_documented():
    """验证第三方依赖被文档化"""
    # 检查架构或概念文档中提到了主要依赖
    dependencies = ['LLVM', 'FlatBuffers', 'mingw-w64', 'libboundscheck']
    # 实现检查逻辑
```

### 属性测试

属性测试验证文档集合的通用规则和约束：

#### 测试配置

使用 Python 的 Hypothesis 库进行属性测试，每个测试运行至少 100 次迭代：

```python
from hypothesis import given, settings, strategies as st
import os
import re

@settings(max_examples=100)
```

#### Property 1: 文档结构完整性测试

```python
@given(st.sampled_from(['modules', 'concepts', 'workflows']))
@settings(max_examples=100)
def test_document_structure_completeness(doc_type):
    """
    Feature: cangjie-compiler-knowledge-base, Property 1: 
    对于任何类型的文档，该文档都应包含其类型所要求的所有必需章节
    """
    # 获取该类型的所有文档
    docs_dir = f'.claude/knowledge-base/{doc_type}'
    if not os.path.exists(docs_dir):
        return
    
    doc_files = [f for f in os.listdir(docs_dir) if f.endswith('.md') and f != 'index.md']
    
    required_sections = {
        'modules': ['Purpose', 'Key Components', 'Public APIs', 'Dependencies', 
                   'Source Locations', 'Usage Examples'],
        'concepts': ['Definition', 'Purpose', 'Implementation', 'Related Modules', 'Examples'],
        'workflows': ['Objective', 'Prerequisites', 'Steps', 'Troubleshooting']
    }
    
    for doc_file in doc_files:
        with open(os.path.join(docs_dir, doc_file)) as f:
            content = f.read()
            for section in required_sections[doc_type]:
                assert f'## {section}' in content or f'### {section}' in content, \
                    f"{doc_file} missing required section: {section}"
```

#### Property 2: 文档格式规范测试

```python
@given(st.sampled_from(['architecture', 'modules', 'concepts', 'workflows', 'code-navigation']))
@settings(max_examples=100)
def test_document_format_compliance(category):
    """
    Feature: cangjie-compiler-knowledge-base, Property 2:
    对于任何知识库中的文档文件，该文件应使用 Markdown 格式、kebab-case 命名、包含元数据
    """
    docs_dir = f'.claude/knowledge-base/{category}'
    if not os.path.exists(docs_dir):
        return
    
    doc_files = [f for f in os.listdir(docs_dir) if f.endswith('.md')]
    
    for doc_file in doc_files:
        # 验证 Markdown 扩展名
        assert doc_file.endswith('.md'), f"{doc_file} is not a Markdown file"
        
        # 验证 kebab-case 命名（允许 index.md 例外）
        if doc_file != 'index.md':
            assert re.match(r'^[a-z0-9]+(-[a-z0-9]+)*\.md$', doc_file), \
                f"{doc_file} does not follow kebab-case naming"
        
        # 验证包含元数据
        with open(os.path.join(docs_dir, doc_file)) as f:
            content = f.read()
            # 检查是否包含 Last Updated 或类似的元数据标记
            assert 'Last Updated' in content or '**Last Updated**' in content, \
                f"{doc_file} missing Last Updated metadata"
```

#### Property 3: 相对路径引用测试

```python
def get_all_markdown_files(base_dir):
    """获取所有 Markdown 文件"""
    md_files = []
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith('.md'):
                md_files.append(os.path.join(root, file))
    return md_files

@given(st.sampled_from(get_all_markdown_files('.claude/knowledge-base')))
@settings(max_examples=100)
def test_relative_path_references(doc_path):
    """
    Feature: cangjie-compiler-knowledge-base, Property 3:
    对于任何文档中的链接和源码引用，都应使用相对路径
    """
    with open(doc_path) as f:
        content = f.read()
    
    # 查找所有链接 [text](path)
    links = re.findall(r'\[([^\]]+)\]\(([^\)]+)\)', content)
    
    for link_text, link_path in links:
        # 跳过外部 URL
        if link_path.startswith('http://') or link_path.startswith('https://'):
            continue
        
        # 验证不是绝对路径
        assert not link_path.startswith('/'), \
            f"Absolute path found in {doc_path}: {link_path}"
        
        # 验证不包含 C:\ 等 Windows 绝对路径
        assert not re.match(r'^[A-Za-z]:\\', link_path), \
            f"Windows absolute path found in {doc_path}: {link_path}"
```

#### Property 4: 模块文档源码引用测试

```python
@settings(max_examples=100)
def test_module_docs_source_references():
    """
    Feature: cangjie-compiler-knowledge-base, Property 4:
    对于任何模块文档，该文档应包含对实际源码位置的引用
    """
    modules_dir = '.claude/knowledge-base/modules'
    if not os.path.exists(modules_dir):
        return
    
    module_files = [f for f in os.listdir(modules_dir) if f.endswith('.md') and f != 'index.md']
    
    for module_file in module_files:
        with open(os.path.join(modules_dir, module_file)) as f:
            content = f.read()
        
        # 验证包含 src/ 或 include/ 的引用
        assert 'src/' in content or 'include/' in content, \
            f"{module_file} does not reference source code locations"
        
        # 验证有 Source Locations 章节
        assert 'Source Locations' in content, \
            f"{module_file} missing Source Locations section"
```

#### Property 5: 概念文档交叉引用测试

```python
@settings(max_examples=100)
def test_concept_docs_cross_references():
    """
    Feature: cangjie-compiler-knowledge-base, Property 5:
    对于任何概念文档，该文档应包含至少一个指向相关模块文档的交叉引用链接
    """
    concepts_dir = '.claude/knowledge-base/concepts'
    if not os.path.exists(concepts_dir):
        return
    
    concept_files = [f for f in os.listdir(concepts_dir) if f.endswith('.md') and f != 'index.md']
    
    for concept_file in concept_files:
        with open(os.path.join(concepts_dir, concept_file)) as f:
            content = f.read()
        
        # 查找指向 modules/ 的链接
        module_links = re.findall(r'\[([^\]]+)\]\(\.\.\/modules\/([^\)]+)\)', content)
        
        assert len(module_links) > 0, \
            f"{concept_file} does not contain cross-references to module documentation"
```

#### Property 6: 索引排序一致性测试

```python
def test_concept_index_alphabetical_order():
    """
    Feature: cangjie-compiler-knowledge-base, Property 6:
    概念索引文件中列出的概念条目应按字母顺序排列
    """
    index_path = '.claude/knowledge-base/concepts/index.md'
    if not os.path.exists(index_path):
        return
    
    with open(index_path) as f:
        content = f.read()
    
    # 提取所有链接的概念名称
    links = re.findall(r'\[([^\]]+)\]\(\.\/([^\)]+)\)', content)
    concept_names = [name for name, _ in links]
    
    # 验证是否按字母顺序排列
    sorted_names = sorted(concept_names, key=str.lower)
    assert concept_names == sorted_names, \
        f"Concept index is not in alphabetical order. Expected: {sorted_names}, Got: {concept_names}"
```

#### Property 7: 架构文档完整性测试

```python
def test_architecture_pipeline_completeness():
    """
    Feature: cangjie-compiler-knowledge-base, Property 7:
    架构文档应提及所有主要编译阶段
    """
    pipeline_path = '.claude/knowledge-base/architecture/pipeline.md'
    if not os.path.exists(pipeline_path):
        return
    
    with open(pipeline_path) as f:
        content = f.read().lower()
    
    required_phases = [
        ('lexical', 'lex'),      # 词法分析
        ('pars',),                # 语法分析 (parsing/parse)
        ('semantic', 'sema'),     # 语义分析
        ('chir',),                # CHIR 生成
        ('codegen', 'code generation')  # 代码生成
    ]
    
    for phase_keywords in required_phases:
        assert any(keyword in content for keyword in phase_keywords), \
            f"Pipeline documentation missing phase: {phase_keywords[0]}"
```

#### Property 8: 依赖文档信息完整性测试

```python
@settings(max_examples=100)
def test_dependency_documentation_completeness():
    """
    Feature: cangjie-compiler-knowledge-base, Property 8:
    对于任何第三方依赖的文档，该文档应包含用途、集成点和版本要求的说明
    """
    # 假设依赖文档在 concepts/llvm-integration.md 或类似位置
    # 这里需要根据实际文档结构调整
    
    # 检查 LLVM 集成文档
    llvm_doc_path = '.claude/knowledge-base/concepts/llvm-integration.md'
    if os.path.exists(llvm_doc_path):
        with open(llvm_doc_path) as f:
            content = f.read().lower()
        
        # 验证包含必需信息
        assert 'purpose' in content or '用途' in content, \
            "LLVM documentation missing purpose information"
        assert 'integration' in content or '集成' in content, \
            "LLVM documentation missing integration information"
        assert 'version' in content or '版本' in content, \
            "LLVM documentation missing version information"
```

#### Property 9: 组件索引完整性测试

```python
@settings(max_examples=100)
def test_component_index_completeness():
    """
    Feature: cangjie-compiler-knowledge-base, Property 9:
    对于任何模块，其组件索引应包含所有公共类、函数和枚举的详细信息
    """
    modules_dir = '.claude/knowledge-base/modules'
    if not os.path.exists(modules_dir):
        return
    
    # 获取所有模块的组件索引文件
    component_files = [f for f in os.listdir(modules_dir) 
                      if f.endswith('-components.json')]
    
    for comp_file in component_files:
        with open(os.path.join(modules_dir, comp_file)) as f:
            data = json.load(f)
        
        # 验证必需字段
        assert 'moduleId' in data, f"{comp_file} missing moduleId"
        assert 'components' in data, f"{comp_file} missing components"
        
        components = data['components']
        
        # 验证类组件的完整性
        if 'classes' in components:
            for cls in components['classes']:
                assert 'name' in cls
                assert 'location' in cls
                assert 'publicMethods' in cls
                
                for method in cls['publicMethods']:
                    assert 'name' in method
                    assert 'signature' in method
                    assert 'description' in method
                    assert 'parameters' in method
                    assert 'returnType' in method

#### Property 10: 交叉引用一致性测试

```python
def test_cross_reference_consistency():
    """
    Feature: cangjie-compiler-knowledge-base, Property 10:
    交叉引用索引中的依赖关系应与模块元数据一致
    """
    cross_refs_path = '.claude/knowledge-base/cross-references.json'
    if not os.path.exists(cross_refs_path):
        return
    
    with open(cross_refs_path) as f:
        cross_refs = json.load(f)
    
    dep_graph = cross_refs['dependencyGraph']['modules']
    
    # 对每个模块，验证依赖关系一致性
    for module_id, dep_info in dep_graph.items():
        module_json_path = f'.claude/knowledge-base/modules/{module_id}.json'
        if not os.path.exists(module_json_path):
            continue
        
        with open(module_json_path) as f:
            module_data = json.load(f)
        
        # 验证依赖列表一致
        cross_ref_deps = set(dep_info['dependencies'])
        module_deps = set(module_data['module']['dependencies'])
        
        assert cross_ref_deps == module_deps, \
            f"Dependency mismatch for {module_id}: " \
            f"cross-references has {cross_ref_deps}, " \
            f"module metadata has {module_deps}"

#### Property 11: 搜索索引覆盖性测试

```python
def test_search_index_coverage():
    """
    Feature: cangjie-compiler-knowledge-base, Property 11:
    搜索索引应覆盖所有存在的文档
    """
    search_index_path = '.claude/knowledge-base/search-index.json'
    if not os.path.exists(search_index_path):
        return
    
    with open(search_index_path) as f:
        search_index = json.load(f)
    
    # 获取所有文档路径
    all_docs = []
    for root, dirs, files in os.walk('.claude/knowledge-base'):
        for file in files:
            if file.endswith('.md') and file != 'index.md':
                rel_path = os.path.relpath(os.path.join(root, file), 
                                          '.claude/knowledge-base')
                all_docs.append(rel_path)
    
    # 从搜索索引中提取所有被索引的文档
    indexed_docs = set()
    for keyword, result in search_index['index']['keywords'].items():
        for doc in result['documents']:
            indexed_docs.add(doc['path'])
    
    # 验证所有文档都被索引
    for doc in all_docs:
        assert doc in indexed_docs, \
            f"Document {doc} is not indexed in search-index.json"

#### Property 12: JSON 路径有效性测试

```python
@given(st.sampled_from(['modules', 'concepts', 'workflows', 'architecture']))
@settings(max_examples=100)
def test_json_path_validity(category):
    """
    Feature: cangjie-compiler-knowledge-base, Property 12:
    JSON 文件中引用的所有路径应指向实际存在的文件
    """
    category_dir = f'.claude/knowledge-base/{category}'
    if not os.path.exists(category_dir):
        return
    
    json_files = [f for f in os.listdir(category_dir) if f.endswith('.json')]
    
    for json_file in json_files:
        with open(os.path.join(category_dir, json_file)) as f:
            data = json.load(f)
        
        # 递归检查所有路径字段
        def check_paths(obj, base_dir):
            if isinstance(obj, dict):
                for key, value in obj.items():
                    if key in ['path', 'documentPath', 'conceptPath', 
                              'metadataFile', 'indexFile', 'location']:
                        if isinstance(value, str) and not value.startswith('http'):
                            # 相对于 knowledge-base 目录解析路径
                            full_path = os.path.join(base_dir, value)
                            assert os.path.exists(full_path), \
                                f"Path {value} in {json_file} does not exist"
                    else:
                        check_paths(value, base_dir)
            elif isinstance(obj, list):
                for item in obj:
                    check_paths(item, base_dir)
        
        check_paths(data, '.claude/knowledge-base')
```

### 测试执行

测试应该在以下时机执行：

1. **文档创建时**: 在添加新文档后运行相关测试，确保符合规范
2. **文档更新时**: 在修改文档后运行测试，确保没有破坏结构
3. **持续集成**: 在 CI 流程中定期运行所有测试，确保知识库整体质量
4. **重构时**: 在重组知识库结构时运行测试，确保链接和引用仍然有效

### 测试工具

- **单元测试**: pytest
- **属性测试**: Hypothesis
- **Markdown 验证**: markdown-it 或类似工具
- **链接检查**: 自定义脚本或 markdown-link-check

### 测试覆盖目标

- 所有必需文档的存在性: 100%
- 文档结构完整性: 100%
- 链接有效性: 100%
- 命名规范遵循: 100%
- 元数据完整性: 100%

## Implementation Notes

### 渐进式实现策略

知识库不需要一次性完成所有文档。建议采用以下优先级：

**Phase 1: 核心结构**
1. 创建目录结构
2. 创建根索引和分类索引
3. 创建检索技能文档

**Phase 2: 关键文档**
1. 架构概览和编译流程
2. 核心模块文档（AST, Lex, Parse, Sema, CHIR, CodeGen）
3. 关键概念文档（AST, CHIR, 类型系统）

**Phase 3: 扩展文档**
1. 其他模块文档（Macro, Modules, Mangle, Driver, Utils）
2. 其他概念文档（宏展开、模块系统、名称修饰等）
3. 工作流文档

**Phase 4: 完善和维护**
1. 代码导航文档
2. 第三方依赖文档
3. 持续更新和维护

### 文档编写指南

1. **使用模板**: 始终从文档模板开始，确保结构一致
2. **简洁明了**: 每个文档聚焦单一主题，避免过度详细
3. **丰富链接**: 充分利用交叉引用，构建知识网络
4. **包含示例**: 尽可能提供代码示例或图示
5. **保持更新**: 在代码变更时及时更新相关文档

### 维护流程

1. **定期审查**: 每个季度审查文档的准确性和完整性
2. **版本同步**: 在编译器发布新版本时更新文档版本标记
3. **反馈收集**: 收集 AI 助手使用知识库的反馈，持续改进
4. **自动化检查**: 使用脚本定期检查链接有效性和文档结构

## Conclusion

本设计文档描述了仓颉编译器知识库系统的完整架构和实现方案。通过分层的文档结构、清晰的索引系统和专门的检索技能，该知识库将显著提升 AI 助手理解和使用编译器代码库的能力。

知识库采用渐进式构建策略，可以从核心文档开始，逐步扩展到完整覆盖。通过属性测试和单元测试，确保文档质量和结构一致性。通过持续维护和更新，保持知识库与代码库的同步。
