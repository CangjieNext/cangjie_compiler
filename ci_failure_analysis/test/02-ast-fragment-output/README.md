# 02 AST 片段解析异常的输出契约

补丁编号：`patch 02`。

## 分类

主分类：`test`。std.ast 当前异常契约正确，修复的是过期的输出副作用断言。

相关模块：stdlib（std.ast）。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_0KkvPtXM.txt:18517`](../../../j_0KkvPtXM.txt#L18517)：`HLT/API/std/ast/parse/parseExprFragmentError.cj`
- [`j_0KkvPtXM.txt:18564`](../../../j_0KkvPtXM.txt#L18564)：`HLT/API/std/ast/parse/parseExprFragmentError_01.cj`

去重后涉及 2 个用例。日志中的同一用例重试仅计算一次。

## 根因分析

测试同时要求打印底层诊断和抛出异常；当前 main 的 ffi.cj 将诊断放入异常并在 parseExprFragment 中转换为 ParseASTException，不再将底层诊断打印到进程输出。

对照当前 main 的 std.ast/ffi.cj 和 parse/parse_expr.cj，底层 diagnostic 已通过异常传递。两个用例继续验证 ParseASTException 及失败退出，避免把库内部打印副作用当成接口契约。

## 修改方案

保留失败退出及 ParseASTException 断言，移除已不存在的底层诊断输出副作用要求。

修改文件：

- [cangjie_test/testsuites/HLT/API/std/ast/parse/parseExprFragmentError.cj](../../../../cangjie_test/testsuites/HLT/API/std/ast/parse/parseExprFragmentError.cj)
- [cangjie_test/testsuites/HLT/API/std/ast/parse/parseExprFragmentError_01.cj](../../../../cangjie_test/testsuites/HLT/API/std/ast/parse/parseExprFragmentError_01.cj)

## 验证结果

原 HLT 框架分别运行两个用例，2/2 通过。日志：/tmp/manifest-fragment-tests.log。

所有本机执行结果来自 Linux x86_64；未执行 macOS 构建。`/tmp` 路径是本次本地验证记录，不是仓库依赖。

## 提交

- `cangjie_test`：`0f1522aa18d595bcf0628200dcdaac6910219e5c`；提交标题：`[patch 02] test: assert AST fragment parse exceptions without obsolete diagnostics`

## 修复补丁

补丁由本报告对应的修复提交使用 `git format-patch --full-index --binary` 导出，保留提交说明和作者信息。

| 仓库 | 补丁文件 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `527a291cd346d1bdca8bb4472a9e02e0c47ec4f7` |

在尚未包含修复、且代码版本与补丁基线兼容的工作副本中，从工作区根目录执行：

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/test/02-ast-fragment-output/cangjie_test.patch
```

当前工作区已包含这些修复，无需重复应用。跨仓补丁需要分别应用到表中对应仓库；整体应用顺序见[总览](../../README.md)。
