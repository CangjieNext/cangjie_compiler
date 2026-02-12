# Conditional Compilation

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

条件编译（Conditional Compilation）是一种编译时机制，允许根据编译条件（如目标平台、编译配置、特性标志等）选择性地编译代码。在仓颉编译器中，条件编译通过条件指令（如 `#if`, `#ifdef`, `#else`, `#endif`）或属性（如 `@Platform`, `@Feature`）来标记代码块，编译器在编译时评估条件并决定是否包含相应的代码。

## Purpose

条件编译在仓颉编译器中的主要作用包括：

1. **平台适配**：根据目标平台（如 Windows、Linux、macOS）编译不同的代码
2. **特性控制**：根据特性标志启用或禁用特定功能
3. **调试支持**：在调试版本中包含额外的调试代码
4. **版本兼容**：支持不同版本的 API 和功能
5. **代码优化**：根据编译配置选择不同的优化策略

## Implementation in Cangjie Compiler

### Key Data Structures

- **ConditionalCompilation**: 条件编译管理器，负责条件评估和代码选择
- **CompilationCondition**: 编译条件，表示一个条件表达式
- **ConditionEvaluator**: 条件求值器，评估条件表达式的真假
- **PlatformInfo**: 平台信息，包含目标平台的各种属性
- **FeatureFlags**: 特性标志集合，表示启用的特性

### Key Algorithms

- **Condition Evaluation**: 条件评估算法，计算条件表达式的布尔值
- **Code Block Selection**: 代码块选择算法，根据条件选择要编译的代码块
- **Preprocessor Integration**: 预处理器集成，处理条件编译指令
- **Attribute Processing**: 属性处理，解析和评估条件编译属性
- **Dead Code Elimination**: 死代码消除，移除未选中的代码块

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/ConditionalCompilation/ConditionalCompilation.h` - 条件编译核心定义
- `/usr1/ace/cangjie_compiler/src/ConditionalCompilation/` - 条件编译实现代码
- `/usr1/ace/cangjie_compiler/include/cangjie/ConditionalCompilation/PlatformInfo.h` - 平台信息定义
- `/usr1/ace/cangjie_compiler/include/cangjie/ConditionalCompilation/FeatureFlags.h` - 特性标志定义

## Related Modules

- **ConditionalCompilation**: 条件编译的核心实现模块
- **parse**: 解析条件编译指令和属性
- **sema**: 在条件编译后进行语义检查
- **ast**: 条件编译相关的 AST 节点
- **driver**: 设置编译条件和特性标志

## Examples

```cpp
// 条件编译指令示例
#if PLATFORM_WINDOWS
func getPlatformName(): String {
  return "Windows"
}
#elif PLATFORM_LINUX
func getPlatformName(): String {
  return "Linux"
}
#else
func getPlatformName(): String {
  return "Unknown"
}
#endif

// 条件编译属性示例
@Platform(windows)
func windowsSpecificFunction() {
  // Windows 特定代码
}

@Feature(experimental)
func experimentalFeature() {
  // 实验性特性代码
}

// 条件编译管理器实现示例
class ConditionalCompilation {
  PlatformInfo Platform;
  FeatureFlags Features;

public:
  bool evaluateCondition(CompilationCondition *Cond) {
    switch (Cond->getKind()) {
    case CK_Platform:
      return Platform.matches(Cond->getPlatformName());
    case CK_Feature:
      return Features.isEnabled(Cond->getFeatureName());
    case CK_Debug:
      return Platform.isDebugBuild();
    case CK_And:
      return evaluateCondition(Cond->getLHS()) &&
             evaluateCondition(Cond->getRHS());
    case CK_Or:
      return evaluateCondition(Cond->getLHS()) ||
             evaluateCondition(Cond->getRHS());
    case CK_Not:
      return !evaluateCondition(Cond->getOperand());
    }
  }

  Stmt *selectCodeBlock(IfConfigStmt *ICS) {
    for (auto *Clause : ICS->getClauses()) {
      if (evaluateCondition(Clause->getCondition())) {
        return Clause->getBody();
      }
    }
    return ICS->getElseClause();
  }
};

// 平台信息示例
class PlatformInfo {
  enum Platform { Windows, Linux, macOS, Android, iOS };
  Platform CurrentPlatform;
  bool IsDebug;

public:
  bool matches(const std::string &Name) const {
    if (Name == "windows") return CurrentPlatform == Windows;
    if (Name == "linux") return CurrentPlatform == Linux;
    if (Name == "macos") return CurrentPlatform == macOS;
    return false;
  }

  bool isDebugBuild() const { return IsDebug; }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/ConditionalCompilation/` - 条件编译头文件目录
- `/usr1/ace/cangjie_compiler/src/ConditionalCompilation/` - 条件编译实现目录
- `/usr1/ace/cangjie_compiler/docs/ConditionalCompilation.md` - 条件编译设计文档（如果存在）
