# 07 ArrayList 闭区间切片越过逻辑长度

补丁编号：`patch 07`。

## 分类

主分类：`stdlib`。修复 std.collection ArrayList 对逻辑长度的闭区间检查。

相关模块：std.collection。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2745`](../../../j_p75yFw7h.txt#L2745)：`LLT/API/std/collection/local_mode/test_arraylist_owned_slice_logical_bounds.cj`

去重后涉及 1 个用例。日志中的同一用例重试仅计算一次。

## 当前处理状态

源码仓中的本地修复提交仍保持撤销。导出的 `.patch` 文件已恢复作为分析附件，未重新应用到源码。

上次验证代码来自 `cangjie_runtime` 的远端 `origin/feature_local_mode`，核对版本 `59b84268718a52946f0c965fb7cc56a45947cfda`。

**上游已包含相应实现修复，且本次验证通过。** 恢复的补丁仅供对照，未重复应用到当前源码。

## 根因分析

普通 rangeSlice 用 range.end > size 检查闭区间，错误接受 end == size；有预留容量时 backing array 允许继续读取未属于列表的元素。local 重载已使用 >=。

例如逻辑长度为 3、容量为 16 的列表，闭区间 0..=3 必须失败，而开区间 0..3 合法。比较的是逻辑 size；容量较大不能使第 4 个元素成为合法列表元素。

## 修改方案

闭区间要求 range.end < size，使用 >= 拒绝逻辑末尾，保持开区间 end == size 合法。

修改文件：

- [cangjie_runtime/stdlib/libs/std/collection/array_list.cj](../../../../cangjie_runtime/stdlib/libs/std/collection/array_list.cj)

## 验证结果

已从远端版本完整重建并安装标准库（203 个构建步骤）及 stdx（135 个构建步骤）。本组 **1/1 通过**。

原框架结果：`/tmp/manifest-upstream-validation-results.json`；日志：`/tmp/manifest-upstream-validation.log`。

## 当前提交

- 验证基线：`cangjie_runtime` `59b84268718a52946f0c965fb7cc56a45947cfda`。

## 参考补丁

[cangjie_runtime.patch](cangjie_runtime.patch) 已恢复，文件内容与删除前完全一致。

| 仓库 | 历史导出提交 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_runtime` | `e24a2af641d099317ca9065384b7388bb77d7189` | `4914f156062674e19d57a4d3c58799d744991a15` |

这里只恢复导出文件，未重新应用到源码；当前仍保留上游代码及上次实跑结果。已包含上游修复的问题无需重复应用此历史补丁。
