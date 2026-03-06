# Cangjie High-level IR (CHIR)

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

仓颉高层中间表示（Cangjie High-level IR, CHIR）是介于抽象语法树（AST）和 LLVM IR 之间的中间表示层。CHIR 是一种面向仓颉语言特性的中间表示，保留了高层语言的语义信息（如类型系统、面向对象特性等），同时为后续的优化和代码生成提供了更规范化的表示形式。

## Purpose

CHIR 在仓颉编译器中的主要作用包括：

1. **语言特性抽象**：将仓颉特有的语言特性（如接口、泛型、闭包等）转换为规范化的 IR 表示
2. **优化基础**：提供高层优化的基础，如内联、常量传播、死代码消除等
3. **分析支持**：支持各种程序分析，如数据流分析、控制流分析等
4. **降级桥梁**：作为从高层语言到低层 LLVM IR 的桥梁，逐步降低抽象层次
5. **序列化支持**：支持 CHIR 的序列化，用于增量编译和模块间优化

## Implementation in Cangjie Compiler

### Key Data Structures

- **IR Node System**: CHIR 的节点体系，包括指令（Instruction）、基本块（BasicBlock）、函数（Function）等
- **CHIRModule**: CHIR 模块，包含一个编译单元的所有 CHIR 代码
- **CHIRBuilder**: CHIR 构建器，用于从 AST 生成 CHIR
- **CHIRType**: CHIR 类型系统，映射仓颉类型到 CHIR 表示
- **CHIRValue**: CHIR 值系统，表示计算结果和变量

### Key Algorithms

- **AST2CHIR Conversion**: 将 AST 转换为 CHIR 的算法，处理各种语言构造的降级
- **SSA Construction**: 构建静态单赋值（SSA）形式的 CHIR
- **Optimization Passes**: 各种 CHIR 层面的优化遍，如常量折叠、死代码消除、内联等
- **Analysis Passes**: 分析遍，如活跃变量分析、到达定义分析等
- **CHIR Lowering**: 将 CHIR 进一步降级为 LLVM IR 的算法

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/CHIR/CHIR.h` - CHIR 核心定义
- `/usr1/ace/cangjie_compiler/src/CHIR/IR/` - CHIR IR 实现
- `/usr1/ace/cangjie_compiler/src/CHIR/AST2CHIR/` - AST 到 CHIR 转换实现
- `/usr1/ace/cangjie_compiler/src/CHIR/Optimization/` - CHIR 优化遍实现
- `/usr1/ace/cangjie_compiler/src/CHIR/Analysis/` - CHIR 分析遍实现
- `/usr1/ace/cangjie_compiler/schema/BCHIRFormat.fbs` - CHIR 序列化格式

## Related Modules

- **chir**: CHIR 的定义和基本操作
- **codegen**: CHIR 的消费者，将 CHIR 转换为 LLVM IR
- **sema**: 为 CHIR 生成提供语义信息上下文
- **ast**: CHIR 转换的源数据结构
- **modules**: CHIR 的序列化和模块管理

## Examples

```cpp
// CHIR 函数创建示例
CHIRModule *Module = new CHIRModule("example");
CHIRFunction *Func = Module->createFunction("add", Int32Type);
CHIRBasicBlock *Entry = Func->createBasicBlock("entry");

// CHIR 指令生成示例
CHIRBuilder Builder(Entry);
CHIRValue *Param1 = Func->getParameter(0);
CHIRValue *Param2 = Func->getParameter(1);
CHIRValue *Result = Builder.createAdd(Param1, Param2);
Builder.createReturn(Result);

// CHIR 优化示例
class ConstantFoldingPass : public CHIRPass {
public:
  void runOnFunction(CHIRFunction *F) override {
    for (auto *BB : F->getBasicBlocks()) {
      for (auto *Inst : BB->getInstructions()) {
        if (auto *BinOp = dyn_cast<BinaryOperator>(Inst)) {
          if (isConstant(BinOp->getLHS()) && isConstant(BinOp->getRHS())) {
            // 常量折叠优化
            foldConstant(BinOp);
          }
        }
      }
    }
  }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/CHIR/` - CHIR 头文件目录
- `/usr1/ace/cangjie_compiler/src/CHIR/` - CHIR 实现目录
- `/usr1/ace/cangjie_compiler/docs/CHIR.md` - CHIR 设计文档（如果存在）
- `/usr1/ace/cangjie_compiler/schema/BCHIRFormat.fbs` - CHIR 二进制格式定义
