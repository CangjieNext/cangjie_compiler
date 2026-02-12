# Macro Expansion

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Definition

仓颉的宏展开（Macro Expansion）是编译时的元编程机制，允许在编译期执行代码生成和转换。宏系统支持编译时计算、代码生成和 AST 转换，通过宏调用（Macro Call）触发宏展开过程，宏展开器（Macro Expander）执行宏代码并生成新的 AST 节点或 Token 序列。

## Purpose

宏展开在仓颉编译器中的主要作用包括：

1. **代码生成**：在编译期生成重复性代码，减少手工编写
2. **编译时计算**：在编译期执行计算，优化运行时性能
3. **DSL 支持**：支持领域特定语言的嵌入和转换
4. **语法扩展**：通过宏实现语言的语法扩展
5. **元编程**：提供强大的元编程能力，支持反射和代码生成

## Implementation in Cangjie Compiler

### Key Data Structures

- **MacroEvaluation**: 宏求值器，负责执行宏代码
- **MacroExpansion**: 宏展开器，管理宏展开过程
- **MacroCall**: 宏调用节点，表示源代码中的宏调用
- **MacroDefinition**: 宏定义，包含宏的参数、代码体等信息
- **EvaluationServer**: 宏求值服务器，独立进程执行宏代码

### Key Algorithms

- **Macro Call Resolution**: 宏调用解析，查找宏定义并匹配参数
- **AST Serialization**: AST 序列化，将 AST 节点序列化为消息传递给求值服务器
- **Token Serialization**: Token 序列化，将 Token 序列序列化为消息
- **Macro Evaluation**: 宏求值，在独立进程中执行宏代码
- **Result Integration**: 结果集成，将宏展开结果集成回主编译流程

### Key Files

- `/usr1/ace/cangjie_compiler/include/cangjie/Macro/MacroEvaluation.h` - 宏求值器定义
- `/usr1/ace/cangjie_compiler/include/cangjie/Macro/MacroExpansion.h` - 宏展开器定义
- `/usr1/ace/cangjie_compiler/src/Macro/` - 宏系统实现代码
- `/usr1/ace/cangjie_compiler/schema/MacroMsgFormat.fbs` - 宏消息序列化格式
- `/usr1/ace/cangjie_compiler/schema/NodeFormat.fbs` - AST 节点序列化格式

## Related Modules

- **macro**: 宏系统的核心实现，包含 MacroEvaluation 和 MacroExpansion
- **parse**: 解析宏定义和宏调用
- **sema**: 在宏展开前后进行语义检查
- **ast**: 宏相关的 AST 节点和序列化
- **modules**: 宏定义的导出和导入

## Examples

```cpp
// 宏定义示例
@Macro
func generateGetter(fieldName: String, fieldType: Type): Decl {
  return parseDecl("""
    public func get\(fieldName.capitalized)(): \(fieldType) {
      return this.\(fieldName)
    }
  """)
}

// 宏调用示例
class Person {
  private var name: String
  private var age: Int

  @generateGetter("name", String)
  @generateGetter("age", Int)
}

// 宏展开器实现示例
class MacroExpander {
public:
  Decl *expandMacro(MacroCall *Call) {
    // 解析宏定义
    MacroDefinition *Def = resolveMacro(Call->getMacroName());

    // 序列化参数
    flatbuffers::FlatBufferBuilder Builder;
    auto ArgsOffset = serializeArguments(Builder, Call->getArguments());

    // 发送到求值服务器
    MacroRequest Request;
    Request.macroName = Def->getName();
    Request.arguments = ArgsOffset;
    MacroResponse Response = EvalServer.evaluate(Request);

    // 反序列化结果
    return deserializeDecl(Response.result);
  }
};

// 宏求值服务器示例
class EvaluationServer {
public:
  MacroResponse evaluate(const MacroRequest &Request) {
    // 加载宏定义
    MacroDefinition *Def = loadMacro(Request.macroName);

    // 反序列化参数
    std::vector<Value> Args = deserializeArguments(Request.arguments);

    // 执行宏代码
    Value Result = executeMacro(Def, Args);

    // 序列化结果
    MacroResponse Response;
    Response.result = serializeValue(Result);
    return Response;
  }
};
```

## Further Reading

- `/usr1/ace/cangjie_compiler/include/cangjie/Macro/` - 宏系统头文件目录
- `/usr1/ace/cangjie_compiler/src/Macro/` - 宏系统实现目录
- `/usr1/ace/cangjie_compiler/schema/MacroMsgFormat.fbs` - 宏消息格式定义
- `/usr1/ace/cangjie_compiler/docs/MacroSystem.md` - 宏系统设计文档（如果存在）
