# Module: Parse

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

语法分析，将 token 流构建为 AST

## Key Components

- **Parser**: 主语法分析器，负责将 token 流解析为抽象语法树
- **ASTChecker**: AST 检查器，验证 AST 的正确性
- **ASTHasher**: AST 哈希器，为 AST 节点生成哈希值
- **ParseDecl**: 声明解析器
- **ParseExpr**: 表达式解析器
- **ParseAtom**: 原子表达式解析器
- **ParseType**: 类型解析器
- **ParsePattern**: 模式匹配解析器
- **ParseMacro**: 宏解析器
- **ParseAnnotations**: 注解解析器
- **ParseModifiers**: 修饰符解析器
- **ParseImports**: 导入语句解析器

## Public APIs

- `Parser::parseDecl()`: 解析声明
- `Parser::parseExpr()`: 解析表达式
- `Parser::parseType()`: 解析类型
- `Parser::parsePattern()`: 解析模式
- `ASTChecker::check()`: 检查 AST 节点
- `ASTHasher::hash()`: 计算 AST 哈希值

## Dependencies

- **lex**: 词法分析模块，提供 token 流
- **ast**: AST 数据结构模块

## Source Locations

**Headers:**
- `include/cangjie/Parse/Parser.h`
- `include/cangjie/Parse/ASTChecker.h`
- `include/cangjie/Parse/ASTHasher.h`

**Implementations:**
- `src/Parse/Parser.cpp`
- `src/Parse/ParseDecl.cpp`
- `src/Parse/ParseExpr.cpp`
- `src/Parse/ParseAtom.cpp`
- `src/Parse/ParseType.cpp`
- `src/Parse/ParsePattern.cpp`
- `src/Parse/ParseMacro.cpp`
- `src/Parse/ParseAnnotations.cpp`
- `src/Parse/ParseModifiers.cpp`
- `src/Parse/ParseImports.cpp`
- `src/Parse/ASTChecker.cpp`
- `src/Parse/ASTHasher.cpp`

**Tests:**
- `unittests/Parse/`

## Usage Examples

```cpp
// 创建解析器并解析源代码
Lexer lexer(sourceCode);
Parser parser(lexer);
ASTNode* ast = parser.parseDecl();

// 检查 AST
ASTChecker checker;
bool isValid = checker.check(ast);

// 计算 AST 哈希
ASTHasher hasher;
uint64_t hash = hasher.hash(ast);
```

## Related Concepts

- [AST](../concepts/ast.md)

## Related Workflows

- [Adding Feature](../workflows/adding-feature.md)
