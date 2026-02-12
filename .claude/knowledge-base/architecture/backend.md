# 后端架构

- Last Updated: 2026-02-12
- Version: 1.0.0

## 概述

仓颉编译器后端负责将前端产出的带类型 AST 转换为可执行的机器代码。后端由三个主要阶段组成：CHIR 生成与优化、CodeGen（LLVM IR 生成）、LLVM 后端处理。

## 模块结构

```
后端核心模块：
  src/CHIR/       — 高级中间表示生成与优化
  src/CodeGen/    — LLVM IR 代码生成

对应头文件：
  include/cangjie/CHIR/
  include/cangjie/CodeGen/
```

## CHIR（高级中间表示）

### 职责

将带类型标注的 AST 转换为 Cangjie High-level IR（CHIR），并在此层级执行高级优化。

### 设计理念

CHIR 是介于 AST 和 LLVM IR 之间的中间表示层。它比 AST 更接近执行语义，但比 LLVM IR 保留了更多的高级语言信息。这使得编译器可以在此层级执行语言特定的优化，而这些优化在 LLVM IR 层级难以实现。

### 处理流程

1. **AST → CHIR 转换：** 遍历 Typed AST，生成对应的 CHIR 节点
2. **高级优化：** 在 CHIR 层级执行优化 Pass
   - 控制流简化
   - 语言特性降级（将高级语言构造转换为更基础的操作）
   - 高级别的内联和特化
3. **输出优化后的 CHIR** 供 CodeGen 消费

### 源码位置

- `src/CHIR/` — CHIR 节点定义、生成逻辑和优化 Pass

## CodeGen（代码生成）

### 职责

将优化后的 CHIR 翻译为 LLVM IR。

### 关键类

- **CGModule** — 代码生成模块级上下文，对应一个 LLVM Module。管理全局变量、函数声明和类型定义的生成。
- **CGFunction** — 代码生成函数级上下文，负责单个函数体的 LLVM IR 生成。管理基本块、局部变量和控制流。
- **IRBuilder** — LLVM IR 指令构建器的封装，提供生成各类 LLVM IR 指令的接口。

### 处理流程

1. **模块初始化：** 创建 CGModule，初始化 LLVM Module 和目标机器信息
2. **类型映射：** 将仓颉类型系统映射到 LLVM 类型
   - 基本类型 → LLVM 基本类型（i8, i16, i32, i64, float, double 等）
   - 类/结构体 → LLVM 结构体类型
   - 数组 → LLVM 数组/指针类型
   - 函数类型 → LLVM 函数类型
3. **全局生成：** 生成全局变量、常量和类型元数据
4. **函数生成：** 对每个函数创建 CGFunction，逐指令生成 LLVM IR
   - 创建函数签名和入口基本块
   - 分配局部变量
   - 遍历 CHIR 指令，使用 IRBuilder 生成对应的 LLVM IR
   - 处理控制流（分支、循环、异常处理）
5. **符号修饰：** 调用 Mangle 模块生成修饰后的符号名称
6. **运行时调用：** 生成对仓颉运行时库的调用（内存分配、GC 交互等）

### 源码位置

- `src/CodeGen/` — 代码生成核心逻辑

## LLVM 集成

### LLVM 后端处理

CodeGen 产出的 LLVM IR 交由 LLVM 基础设施进行后续处理：

1. **LLVM 优化 Pass：** 根据优化级别（-O0/-O1/-O2/-O3）执行不同程度的优化
   - 死代码消除
   - 常量折叠与传播
   - 循环优化
   - 函数内联
   - 向量化
2. **指令选择：** 将 LLVM IR 转换为目标平台的机器指令
3. **寄存器分配：** 将虚拟寄存器映射到物理寄存器
4. **目标代码发射：** 生成目标文件（.o）
5. **链接：** 将目标文件与运行时库链接为最终可执行文件

### LLVM 版本与依赖

编译器依赖特定版本的 LLVM 库，通过 LLVM C++ API 进行交互。主要使用的 LLVM 组件包括：

- `llvm::Module` — 模块级 IR 容器
- `llvm::Function` — 函数级 IR 容器
- `llvm::IRBuilder` — IR 指令构建器
- `llvm::PassManager` — 优化 Pass 管理器
- `llvm::TargetMachine` — 目标平台抽象

## 后端与支撑系统的交互

- **Mangle：** CodeGen 在生成函数和全局变量时调用 Mangle 模块获取修饰后的符号名
- **Driver：** Driver 控制后端的执行流程，包括优化级别选择和输出格式配置
- **IncrementalCompilation：** 增量编译系统可能缓存 LLVM IR 或目标文件以加速重编译
