# 13 std.ast 词法选项迁移遗漏 effect handler 开关

补丁编号：`patch 13`。

## 分类

主分类：`stdlib`。修复 std.ast 库桥接层传递 enableEH 的逻辑。

相关模块：std.ast、effect handler。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:5141`](../../../j_p75yFw7h.txt#L5141)：`LLT/API/std/libast/tokens/cangjieLex/test_cangjieLex_eh.cj`

去重后涉及 1 个原失败用例。日志中的同一用例重试仅计算一次。

## 当前处理状态

已按用户要求把第 13 项重新合入当前 runtime，基于 `origin/feature_local_mode` 的 `59b84268718a52946f0c965fb7cc56a45947cfda` 新建一个修复提交。

原失败用例已通过。05—09 继续使用已有的上游修复。

## 根因分析

`SetLexerOptions` 迁移时只传递了 `enableLocalMode`，遗漏 `enableEH`。即使宿主编译已传入 `--experimental --enable-eh`，std.ast 创建的 Lexer 仍会把 `handle`、`perform`、`resume` 降为普通标识符。

修复前，完整重建标准库和 stdx 后仍报 `cangjieLex demoted the effect-handler keywords to IDENTIFIER`，并提示预期 `HANDLE/PERFORM/RESUME`。SDK 中 ast CJO、静态库和 astFFI 已与构建产物核对哈希，因此这是选项传递遗漏。

`SetLexerOptions` 同时被正常词法分析和错误重解析调用，应在此统一转发开关。

## 修改方案

在 [ast_api.cpp](../../../../cangjie_runtime/stdlib/libs/std/ast/native/ast_api.cpp) 的 `SetLexerOptions` 中增加一行：

```cpp
lex.SetEHEnabled(opts && opts->enableEH);
```

有宏编译上下文时沿用其 EH 开关；没有上下文时保持 EH 关闭。

## 验证结果

Linux x86_64 上增量构建标准库成功（154 个构建步骤），安装新 SDK，并核对 SDK 中 `libcangjie-std-astFFI.a` 与 `libstdFFI.so` 的哈希和本次构建一致。

使用原 LLT 框架与 cjnative 配置运行 **27 例，27/27 通过**：

- 上次运行的 24 个回归用例全部通过，其中六项 stdlib 问题的原失败用例 **19/19** 通过；patch 11/12 的四个 SCAN 用例和 patch 15 原 finalizer 用例也通过。
- `test_cangjieLex_eh.cj` 在启用 EH 时检查关键字 token 及 parseExpr/parseDecl/parseProgram。
- 新增检查已有的 `test_cangjieLex_eh_neg.cj`，确认关闭 EH 时仍符合负例预期；`test_cangjieLex_exception.cj` 和 `test_cangjieLex_exception_01.cj` 也通过。没有修改测试代码。

本组原失败用例 **1/1** 通过；同目录四个有效用例 **4/4** 通过。

构建日志：`/tmp/manifest-patch13-applied-build.log`；安装日志：`/tmp/manifest-patch13-applied-install.log`。

框架结果：`/tmp/manifest-patch13-applied-results.json`；运行日志：`/tmp/manifest-patch13-applied-tests.log`；用例列表：`/tmp/manifest-patch13-applied-testlist`。

修复前的失败记录保留在 `/tmp/manifest-upstream-validation-results.json` 和 `/tmp/manifest-upstream-validation.log`。macOS 尚未验证。

## 当前提交与补丁

| 仓库 | 修复提交 | 父提交 |
| --- | --- | --- |
| `cangjie_runtime` | `2ddd31937eb4823e6d0cc6cec0bcb564cbb4c0a5` | `59b84268718a52946f0c965fb7cc56a45947cfda` |

[cangjie_runtime.patch](cangjie_runtime.patch) 已从本次提交重新导出，当前源码已经包含此修复。

在兼容基线的 runtime 仓库中可通过 `git am /path/to/ci_failure_analysis/stdlib/13-ast-eh-lexer/cangjie_runtime.patch` 应用。当前工作区无需重复应用。尚未推送。
