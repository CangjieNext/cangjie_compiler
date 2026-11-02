# 20 Trace 安全测试中的波浪号被 shell 展开

补丁编号：`patch 20`。分类：`test`。

[返回总览](../../README.md) · [分类目录](../README.md)

## 日志证据

- [j_9LCB8BBR.txt:2435](../../../j_9LCB8BBR.txt#L2435)：`LLT/API/std/runtime/test_trace_security.cj`

去重后共 1 个原失败用例。

## 根因分析

EXEC 中 cjTracePath=~/secret.trace 没有引号，shell 会把 ~ 展开成 HOME。用例本来要验证拒绝字面量 ~，实际却传入有效的绝对路径，并可能启动真实 trace。CI 恰好在该命令超时；本机原例虽能结束，但确实创建了 HOME 下的 trace 输出，证实输入与测试目的不符。

## 修改方案

改为 cjTracePath='~/secret.trace'，让运行时接收到真正包含 ~ 的非法路径。保留原来的四种路径输入。验证过程中产生的本机 trace 文件已按本次运行的创建时间核对后清理。

修改文件：

- [cangjie_test/testsuites/LLT/API/std/runtime/test_trace_security.cj](../../../../cangjie_test/testsuites/LLT/API/std/runtime/test_trace_security.cj)

## 验证结果

原 LLT 框架运行该用例 **1/1** 通过，四条运行命令均成功；另外从结构化结果核对带引号命令输出为 Trace not started with tilde，确认测试到了预期分支。

本机记录：

- `/tmp/manifest-20261105-llt-fixed-results.json`
- `/tmp/manifest-20261105-llt-fixed.log`

## 补丁与提交

各根因独立导出 patch；工作分支按此前要求汇总为每仓一个提交。表中的分解提交保留在备份引用下，用于导出和审阅本项修改。

| 仓库 | 独立补丁 | 导出提交 | 父提交 | 当前汇总提交 |
| --- | --- | --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `c2627e11603be99ef651b96f7b823adaa9f2111d` | `ed606e82311cb14dc00981aa942d73bc4abfdfc6` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` |

当前源码已包含修复。向兼容基线移植时，在相应仓库执行 `git am /path/to/test/20-trace-literal-tilde/仓库名.patch`。跨仓问题 19 分别应用 runtime 和 test 补丁，先重建、安装库后再运行测试。
