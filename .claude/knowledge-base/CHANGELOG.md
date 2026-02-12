# Changelog

知识库版本变更记录。

## [1.0.0] - 2026-02-12

### 初始版本

知识库系统的首次发布，包含以下内容：

#### 目录结构
- 创建 5 个分类目录：architecture、modules、concepts、workflows、code-navigation
- 创建根索引文件（index.md、index.json）
- 创建全局索引文件（cross-references.json、search-index.json）

#### 架构文档（5 篇）
- overview.md - 编译器整体架构概览
- pipeline.md - 编译流程管线
- frontend.md - 前端架构
- backend.md - 后端架构（LLVM）
- data-flow.md - 数据流和模块交互

#### 模块文档（11 个模块，每个含 .md、.json、-components.json）
- Lex - 词法分析模块
- Parse - 语法分析模块
- AST - 抽象语法树模块
- Sema - 语义分析模块
- CHIR - 仓颉高层中间表示模块
- CodeGen - 代码生成模块
- Macro - 宏展开模块
- Modules - 包管理模块
- Mangle - 名称修饰模块
- Driver - 编译驱动模块
- Utils - 工具模块

#### 概念文档（9 篇）
- ast - 抽象语法树
- chir - 仓颉高层中间表示
- type-system - 类型系统
- name-mangling - 名称修饰
- module-system - 模块系统
- macro-expansion - 宏展开
- conditional-compilation - 条件编译
- serialization - 序列化格式
- llvm-integration - LLVM 集成

#### 工作流文档（4 篇）
- building - 构建编译器
- adding-feature - 添加新语言特性
- debugging - 调试编译问题
- testing - 测试策略

#### 代码导航文档（3 篇）
- directory-structure - 目录结构映射
- naming-conventions - 命名规范
- key-files - 关键文件清单

#### 工具和技能
- 检索技能文档（.claude/skills/use-knowledge-base.md）
- JSON 验证脚本（tools/validate_json.py）
- 链接检查脚本（tools/check_links.py）
- 文档结构验证脚本（tools/validate_structure.py）
