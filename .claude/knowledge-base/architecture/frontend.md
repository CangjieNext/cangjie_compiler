# 前端架构

- Last Updated: 2026-02-12
- Version: 1.0.0

## 概述

仓颉编译器前端负责将源代码文本转换为带类型标注的 AST。前端由三个核心阶段组成：词法分析（Lex）、语法分析（Parse）和语义分析（Sema）。AST 是贯穿前端各阶段的核心数据结构。

## 模块结构

```
前端核心模块：
  src/Lex/          — 词法分析器
  src/Parse/         — 语法分析器
  src/Sema/          — 语义分析器
  src/AST/           — AST 节点定义
  src/Frontend/      — 前端入口协调
  src/FrontendTool/  — 前端工具接口

对应头文件：
  include/cangjie/Lex/
  include/cangjie/Parse/
  include/cangjie/Sema/
  include/cangjie/AST/
  include/cangjie/Frontend/
  include/cangjie/FrontendTool/
```

## Lex（词法分析）

### 职责

将源代码字符流分解为 Token 序列。

### 关键类

- **Lexer** — 词法分析器主类，驱动逐字符扫描并生成 Token
- **Token** — 词法单元，携带类型（TokenKind）和源码位置信息
- **TokenKind** — Token 类型枚举，包括关键字、标识符、字面量、运算符等

### 处理流程

1. 读取源代码字符流
2. 跳过空白符和注释
3. 识别并分类各类 Token
4. 处理条件编译指令（与 ConditionalCompilation 模块协作）
5. 输出 Token 流供 Parser 消费

## Parse（语法分析）

### 职责

将 Token 流按照仓颉语言文法规则构建为 AST。

### 关键类

- **Parser** — 语法分析器主类，实现递归下降解析
- **ASTNode** — AST 节点基类
- **Decl / Stmt / Expr** — 声明、语句、表达式等 AST 节点类型

### 处理流程

1. 从 Lexer 获取 Token 流
2. 按文法规则递归下降解析
3. 识别宏调用并触发宏展开（与 Macro 模块协作）
4. 构建完整的 AST
5. 报告语法错误

### 宏展开

Parser 在解析过程中遇到宏调用时，会调用 Macro 模块进行编译期宏展开。展开后的代码会被重新解析并融入当前 AST。

## Sema（语义分析）

### 职责

对 AST 进行全面的语义检查，产出带类型标注的 AST。

### 关键类

- **TypeChecker** — 类型检查器，执行类型推断和类型验证
- **Scope** — 作用域管理，处理名称查找和可见性
- **Type** — 类型系统表示

### 处理流程

1. 名称解析：将标识符绑定到其声明
2. 模块解析：处理 import 语句（与 Modules 模块协作）
3. 类型推断：推导表达式和变量的类型
4. 类型检查：验证类型一致性和兼容性
5. 重载决议：选择正确的重载函数
6. 泛型实例化：处理泛型类型参数
7. 访问控制：验证成员访问权限
8. 符号修饰：生成修饰后的符号名（与 Mangle 模块协作）

## AST（抽象语法树）

AST 是前端的核心数据结构，定义在 `src/AST/` 中。

### 特点

- 层次化的树形结构，忠实表示源代码的语法结构
- 每个节点携带源码位置信息，用于错误报告
- 在 Sema 阶段被标注类型信息，成为 Typed AST
- 作为前端到后端的主要数据传递接口

### 节点分类

- **Decl（声明）** — 函数声明、类声明、变量声明等
- **Stmt（语句）** — 赋值语句、控制流语句、返回语句等
- **Expr（表达式）** — 字面量、二元运算、函数调用等
- **Type（类型）** — 类型表示节点

## 前端协调

`Frontend` 和 `FrontendTool` 模块负责协调前端各阶段的执行：

1. 初始化编译上下文
2. 按顺序调用 Lex → Parse → Sema
3. 收集和管理诊断信息
4. 将最终的 Typed AST 传递给后端
