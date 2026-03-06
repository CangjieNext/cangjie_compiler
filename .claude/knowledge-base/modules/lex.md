# Module: Lex

Last Updated: 2026-02-12
Version: 1.0.0

## Purpose

将源代码文本转换为 token 流

## Key Components

- **Lexer**: 主词法分析器类，负责将源代码转换为 token 流
- **Token**: Token 数据结构，表示一个词法单元
- **TokenKind**: Token 类型枚举，定义所有可能的 token 类型

## Public APIs

- `Lexer::tokenize(const std::string& source)`: 将源代码字符串转换为 token 列表
- `Lexer::getNextToken()`: 获取下一个 token
- `Lexer::peekToken()`: 查看下一个 token 但不消费
- `Token::getKind()`: 获取 token 类型
- `Token::getLocation()`: 获取源码位置
- `Token::getValue()`: 获取 token 值

## Dependencies

无直接依赖

## Source Locations

**Headers:**
- `include/cangjie/Lex/Lexer.h`
- `include/cangjie/Lex/Token.h`
- `include/cangjie/Lex/AnnotationToken.h`
- `include/cangjie/Lex/Tokens.inc`
- `include/cangjie/Lex/AnnotationTokens.inc`

**Implementations:**
- `src/Lex/Lexer.cpp`
- `src/Lex/LexerDiag.cpp`
- `src/Lex/LexerImpl.cpp`

**Tests:**
- `unittests/Lex/`

## Usage Examples

```cpp
// 创建词法分析器并分析源代码
Lexer lexer(sourceCode);
std::vector<Token> tokens = lexer.tokenize(sourceCode);

// 逐个获取 token
Token tok = lexer.getNextToken();
TokenKind kind = tok.getKind();
SourceLocation loc = tok.getLocation();
```

## Related Concepts

- [AST](../concepts/ast.md)

## Related Workflows

- [Adding Feature](../workflows/adding-feature.md)
