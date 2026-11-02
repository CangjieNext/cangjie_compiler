# 18 ArrayList 闭区间异常诊断预期过期

补丁编号：`patch 18`。分类：`test`。

[返回总览](../../README.md) · [分类目录](../README.md)

## 日志证据

- [j_9LCB8BBR.txt:2533](../../../j_9LCB8BBR.txt#L2533)：`LLT/API/std/collection/test_ArrayList.cj`

去重后共 1 个原失败用例。

## 根因分析

ArrayList.rangeSlice 已按逻辑长度检查闭区间终点 range.end >= size，并抛出包含 greater than or equal to 的消息。综合测试 sliceByBigClosedRangeEmptyArrayList 仍期待旧的 greater than 文案。CI 的 58 个 TestCase 中仅此项失败。

## 修改方案

只把该断言更新为 Closed range end '5' is greater than or equal to size '1'.，继续校验 IndexOutOfBoundsException 的完整消息。现有边界实现正确。

修改文件：

- [cangjie_test/testsuites/LLT/API/std/collection/test_ArrayList.cj](../../../../cangjie_test/testsuites/LLT/API/std/collection/test_ArrayList.cj)

## 验证结果

安装本次重建的标准库后，原 LLT 框架运行 test_ArrayList.cj **1/1** 通过，内部 **58/58** TestCase 通过。

本机记录：

- `/tmp/manifest-20261105-llt-fixed-results.json`
- `/tmp/manifest-20261105-llt-fixed.log`

## 补丁与提交

各根因独立导出 patch；工作分支按此前要求汇总为每仓一个提交。表中的分解提交保留在备份引用下，用于导出和审阅本项修改。

| 仓库 | 独立补丁 | 导出提交 | 父提交 | 当前汇总提交 |
| --- | --- | --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `c85975e76749394452694e982da5a8d8fe7de253` | `e41ec80744413ca473fd69b47ab78f4c3fdcf425` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` |

当前源码已包含修复。向兼容基线移植时，在相应仓库执行 `git am /path/to/test/18-arraylist-closed-range-message/仓库名.patch`。跨仓问题 19 分别应用 runtime 和 test 补丁，先重建、安装库后再运行测试。
