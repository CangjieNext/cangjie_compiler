# LLVM Integration

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

LLVM 集成（LLVM Integration）是仓颉编译器与 LLVM 编译器基础设施的集成层，负责将仓颉的高层中间表示（CHIR）转换为 LLVM IR，并利用 LLVM 的优化和代码生成能力生成目标机器码。集成层包括 LLVM IR 生成、自定义优化遍、调试信息生成、以及对 LLVM 的定制化修改（通过补丁）。

## Purpose

LLVM 集成在仓颉编译器中的主要作用包括：

1. **代码生成**：将 CHIR 转换为 LLVM IR，利用 LLVM 生成高质量的机器码
2. **优化支持**：利用 LLVM 的优化框架进行代码优化
3. **多平台支持**：通过 LLVM 支持多种目标平台和架构
4. **调试信息**：生成 DWARF 调试信息，支持调试器
5. **定制化扩展**：通过补丁对 LLVM 进行定制化修改，支持仓颉特有的特性

## Implementation in Cangjie Compiler

### Key Data Structures

- **CGModule**: 代码生成模块，管理 LLVM 模块的生成
- **IRBuilder**: LLVM IR 构建器，用于生成 LLVM IR 指令
- **TypeConverter**: 类型转换器，将仓颉类型转换为 LLVM 类型
- **DebugInfoBuilder**: 调试信息构建器，生成 DWARF 调试信息
- **PassManager**: 优化遍管理器，管理 LLVM 优化遍的执行

### Key Algorithms

- **LLVM IR Generation**: LLVM IR 生成算法，从 CHIR 生成 LLVM IR
- **Type Lowering**: 类型降级算法，将高层类型映射到 LLVM 类型
- **Function Lowering**: 函数降级算法，生成函数的 LLVM IR
- **Debug Info Generation**: 调试信息生成算法，生成 DWARF 格式的调试信息
- **Custom Passes**: 自定义优化遍，针对仓颉语言特性的优化

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/CodeGen/IRBuilder.h` - IR 构建器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/CodeGen/CGModule.h` - 代码生成模块定义
- `/usr1/ace/cangjie_compiler/src/CodeGen/` - 代码生成实现目录
- `/usr1/ace/cangjie_compiler/third_party/` - LLVM 源代码目录
- `/usr1/ace/cangjie_compiler/llvmPatch.diff` - LLVM 补丁文件

## Related Modules

- **codegen**: 代码生成模块，包含 CGModule 和 IRBuilder
- **chir**: CHIR 是 LLVM IR 生成的输入
- **driver**: 驱动 LLVM 后端工具链（汇编器、链接器）
- **sema**: 提供类型信息用于 LLVM IR 生成
- **modules**: 管理多模块的 LLVM IR 生成

## Examples

```cpp
// LLVM IR 生成示例
class IRGenerator {
  llvm::LLVMContext &LLVMCtx;
  llvm::Module *LLVMModule;
  llvm::IRBuilder<> Builder;

public:
  llvm::Value *generateExpr(CHIRExpr *Expr) {
    if (auto *BinOp = dyn_cast<CHIRBinaryOp>(Expr)) {
      llvm::Value *LHS = generateExpr(BinOp->getLHS());
      llvm::Value *RHS = generateExpr(BinOp->getRHS());

      switch (BinOp->getOp()) {
      case BO_Add:
        return Builder.CreateAdd(LHS, RHS, "add");
      case BO_Sub:
        return Builder.CreateSub(LHS, RHS, "sub");
      case BO_Mul:
        return Builder.CreateMul(LHS, RHS, "mul");
      // ... 其他操作符
      }
    } else if (auto *Const = dyn_cast<CHIRConstant>(Expr)) {
      return llvm::ConstantInt::get(
        convertType(Const->getType()),
        Const->getValue()
      );
    }
    // ... 其他表达式类型
  }

  llvm::Function *generateFunction(CHIRFunction *Func) {
    // 创建函数类型
    llvm::FunctionType *FT = convertFunctionType(Func->getType());

    // 创建函数
    llvm::Function *LLVMFunc = llvm::Function::Create(
      FT,
      llvm::Function::ExternalLinkage,
      Func->getName(),
      LLVMModule
    );

    // 生成函数体
    for (auto *BB : Func->getBasicBlocks()) {
      llvm::BasicBlock *LLVMBB = llvm::BasicBlock::Create(
        LLVMCtx,
        BB->getName(),
        LLVMFunc
      );
      Builder.SetInsertPoint(LLVMBB);

      for (auto *Inst : BB->getInstructions()) {
        generateInstruction(Inst);
      }
    }

    return LLVMFunc;
  }
};

// 类型转换示例
class TypeConverter {
  llvm::LLVMContext &LLVMCtx;

public:
  llvm::Type *convertType(CHIRType *Type) {
    if (auto *IntTy = dyn_cast<CHIRIntType>(Type)) {
      return llvm::Type::getIntNTy(LLVMCtx, IntTy->getBitWidth());
    } else if (auto *FloatTy = dyn_cast<CHIRFloatType>(Type)) {
      if (FloatTy->getBitWidth() == 32)
        return llvm::Type::getFloatTy(LLVMCtx);
      else
        return llvm::Type::getDoubleTy(LLVMCtx);
    } else if (auto *PtrTy = dyn_cast<CHIRPointerType>(Type)) {
      return llvm::PointerType::get(
        convertType(PtrTy->getPointeeType()),
        0
      );
    }
    // ... 其他类型
  }
};

// 调试信息生成示例
class DebugInfoBuilder {
  llvm::DIBuilder DIBuilder;

public:
  llvm::DISubprogram *createFunction(
    CHIRFunction *Func,
    llvm::Function *LLVMFunc
  ) {
    llvm::DIFile *File = DIBuilder.createFile(
      Func->getSourceFile(),
      Func->getSourceDirectory()
    );

    llvm::DISubroutineType *FuncType = DIBuilder.createSubroutineType(
      convertDebugType(Func->getType())
    );

    llvm::DISubprogram *SP = DIBuilder.createFunction(
      File,
      Func->getName(),
      LLVMFunc->getName(),
      File,
      Func->getLine(),
      FuncType,
      Func->getLine(),
      llvm::DINode::FlagPrototyped,
      llvm::DISubprogram::SPFlagDefinition
    );

    LLVMFunc->setSubprogram(SP);
    return SP;
  }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/CodeGen/` - 代码生成头文件目录
- `/usr1/ace/cangjie_compiler/src/CodeGen/` - 代码生成实现目录
- `/usr1/ace/cangjie_compiler/third_party/` - LLVM 源代码
- `/usr1/ace/cangjie_compiler/llvmPatch.diff` - LLVM 定制化补丁
- LLVM Documentation - https://llvm.org/docs/
- LLVM Language Reference - https://llvm.org/docs/LangRef.html
