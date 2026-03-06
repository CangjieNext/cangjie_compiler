# 编译器整体架构概览

- Last Updated: 2026-02-12
- Version: 1.0.0

## 架构图

![Compiler Architecture Diagram](figures/Compiler_Architecture_Diagram_zh.png)

## 概述

仓颉编译器（Cangjie Compiler）是一个多阶段编译系统，将仓颉源代码编译为可执行文件。编译器采用经典的前端-后端分离架构，通过多层中间表示（IR）逐步将高级语言结构降低为机器代码。

## 核心编译管线

源代码经过以下阶段处理：

1. **Lex（词法分析）** — 将源代码文本转换为 Token 流
2. **Parse（语法分析）** — 将 Token 流构建为抽象语法树（AST）
3. **Sema（语义分析）** — 对 AST 进行类型检查与语义验证
4. **CHIR（高级中间表示）** — 生成并优化仓颉高级 IR
5. **CodeGen（代码生成）** — 将 CHIR 转换为 LLVM IR
6. **LLVM Backend** — 由 LLVM 后端生成目标平台的可执行文件

## 模块总览

### 前端模块

| 模块 | 源码目录 | 职责 |
|------|---------|------|
| AST | `src/AST/` | 抽象语法树节点定义与操作 |
| Lex | `src/Lex/` | 词法分析器 |
| Parse | `src/Parse/` | 语法分析器 |
| Sema | `src/Sema/` | 语义分析与类型检查 |
| Frontend | `src/Frontend/` | 前端入口与协调 |
| FrontendTool | `src/FrontendTool/` | 前端工具接口 |

### 后端模块

| 模块 | 源码目录 | 职责 |
|------|---------|------|
| CHIR | `src/CHIR/` | 高级中间表示生成与优化 |
| CodeGen | `src/CodeGen/` | LLVM IR 代码生成 |

### 支撑系统

| 模块 | 源码目录 | 职责 |
|------|---------|------|
| Macro | `src/Macro/` | 编译期宏展开 |
| Modules | `src/Modules/` | 包管理与 import 解析 |
| Mangle | `src/Mangle/` | 符号名称修饰 |
| Driver | `src/Driver/` | 编译流程驱动与调度 |
| IncrementalCompilation | `src/IncrementalCompilation/` | 增量编译支持 |
| ConditionalCompilation | `src/ConditionalCompilation/` | 条件编译处理 |
| MetaTransformation | `src/MetaTransformation/` | 元变换处理 |

### 基础设施

| 模块 | 源码目录 | 职责 |
|------|---------|------|
| Basic | `src/Basic/` | 基础数据结构与工具 |
| Utils | `src/Utils/` | 通用工具函数 |
| Option | `src/Option/` | 编译选项管理 |

## 源码结构

```
src/                          # 源代码实现
  AST/
  Lex/
  Parse/
  Sema/
  CHIR/
  CodeGen/
  Macro/
  Modules/
  Mangle/
  Driver/
  Frontend/
  FrontendTool/
  Basic/
  Utils/
  Option/
  ConditionalCompilation/
  IncrementalCompilation/
  MetaTransformation/

include/cangjie/              # 公共头文件（镜像 src/ 结构）
  AST/
  Lex/
  Parse/
  Sema/
  CHIR/
  CodeGen/
  ...
```

## 模块间关系

- **Driver** 是编译流程的总调度器，协调各阶段的执行顺序
- **Lex → Parse → Sema** 构成前端核心管线
- **Sema** 依赖 **Modules** 进行包和导入解析
- **Macro** 在语法分析阶段介入，进行编译期宏展开
- **ConditionalCompilation** 在词法/语法分析阶段处理条件编译指令
- **Sema** 产出的带类型 AST 传递给 **CHIR** 进行中间表示生成
- **CHIR** 经过优化后传递给 **CodeGen** 生成 LLVM IR
- **Mangle** 被 **CodeGen** 和 **Sema** 使用，生成符号的修饰名称
- **IncrementalCompilation** 跨越多个阶段，管理编译缓存与依赖追踪
