# 16 栈溢出测试递归帧过小导致扫描超时

补丁编号：`patch 16`。

## 分类

主分类：`test`。极小递归帧使压力用例在到达目标路径前耗尽时间，修复测试构造而非 runtime 实现。

相关模块：runtime、stdlib（std.core）。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:5087`](../../../j_p75yFw7h.txt#L5087)：`LLT/API/std/core/error_handler_stackoverflow_throws_error.cj`

去重后涉及 1 个用例。日志中的同一用例重试仅计算一次。

## 根因分析

原测试在较大默认栈上用极小递归帧逼近栈溢出，GDB 显示运行时在 FixExtendedStack/RecordStackPtrs 中处理 2,097,292 个栈根。超时发生在到达预期错误处理路径前的大量扫描，现有证据不支持死锁结论。

VArray 为每层递归提供约 64 KiB 的活跃数据；递归前后的 blackBox 防止生命周期缩短，禁用内联避免帧结构被改变。验证目标仍是“错误处理器中再次发生 StackOverflowError 后输出诊断”，不依赖递归次数或内部扫描数量。

## 修改方案

关闭函数内联，在递归函数中保留 8192 元素的 VArray<Int64> 并用 std.runtime.blackBox 在递归前后保持其存活，以更少帧触及相同栈上限。保留递归后的操作和原错误处理输出断言，不提高超时。

修改文件：

- [cangjie_test/testsuites/LLT/API/std/core/error_handler_stackoverflow_throws_error.cj](../../../../cangjie_test/testsuites/LLT/API/std/core/error_handler_stackoverflow_throws_error.cj)

## 验证结果

临时变体验证在 25 秒内进入错误处理器中的 StackOverflowError 路径；修改后的原 LLT 框架用例通过。日志：/tmp/manifest-core-tests.log；GDB 证据：/tmp/manifest-ci-debug/stackoverflow-new-gdb.log。

所有本机执行结果来自 Linux x86_64；未执行 macOS 构建。`/tmp` 路径是本次本地验证记录，不是仓库依赖。

## 提交

- `cangjie_test`：`b5c5c05a5e735c65524f95f6ae52c09373385f66`；提交标题：`[patch 16] test: bound recursive frame count in stack overflow handler coverage`

## 修复补丁

补丁由本报告对应的修复提交使用 `git format-patch --full-index --binary` 导出，保留提交说明和作者信息。

| 仓库 | 补丁文件 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `cd1e10565d83c8051b4b417c2d5eb13d33b774ed` |

在尚未包含修复、且代码版本与补丁基线兼容的工作副本中，从工作区根目录执行：

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/test/16-stackoverflow-handler/cangjie_test.patch
```

当前工作区已包含这些修复，无需重复应用。跨仓补丁需要分别应用到表中对应仓库；整体应用顺序见[总览](../../README.md)。
