# 06 Rune UTF-8 解码遗漏负索引检查

补丁编号：`patch 06`。

## 分类

主分类：`stdlib`。修复 std.core Rune.fromUtf8 的负索引检查。

相关模块：std.core。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:4746`](../../../j_p75yFw7h.txt#L4746)：`LLT/API/std/core/local_mode/test_local_utf8_boundaries.cj`

去重后涉及 1 个用例。日志中的同一用例重试仅计算一次。

## 当前处理状态

源码仓中的本地修复提交仍保持撤销。导出的 `.patch` 文件已恢复作为分析附件，未重新应用到源码。

上次验证代码来自 `cangjie_runtime` 的远端 `origin/feature_local_mode`，核对版本 `59b84268718a52946f0c965fb7cc56a45947cfda`。

**上游已包含相应实现修复，且本次验证通过。** 恢复的补丁仅供对照，未重复应用到当前源码。

## 根因分析

Rune.fromUtf8(Array, index) 只检查 index >= size，负索引被加到 backing array 起点并传入 unchecked 读取。local 重载已有完整检查。

负索引必须在计算 backing array 地址之前拒绝。原检查只覆盖上界，绕过下界后进入 unchecked 读取。复用已有边界异常，不改返回类型或 UTF-8 解码算法。

## 修改方案

上游在普通和 local 重载中统一使用 UInt64(index) >= UInt64(arr.size) 拒绝负索引与上界越界；保留原异常类型。

修改文件：

- [cangjie_runtime/stdlib/libs/std/core/char_extension.cj](../../../../cangjie_runtime/stdlib/libs/std/core/char_extension.cj)

## 验证结果

已从远端版本完整重建并安装标准库（203 个构建步骤）及 stdx（135 个构建步骤）。本组 **1/1 通过**。

原框架结果：`/tmp/manifest-upstream-validation-results.json`；日志：`/tmp/manifest-upstream-validation.log`。

## 当前提交

- 验证基线：`cangjie_runtime` `59b84268718a52946f0c965fb7cc56a45947cfda`。

## 参考补丁

[cangjie_runtime.patch](cangjie_runtime.patch) 已恢复，文件内容与删除前完全一致。

| 仓库 | 历史导出提交 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_runtime` | `4914f156062674e19d57a4d3c58799d744991a15` | `304252ec3b73c4ff9c6e62f7dc3bbbd13cbd2cd0` |

这里只恢复导出文件，未重新应用到源码；当前仍保留上游代码及上次实跑结果。已包含上游修复的问题无需重复应用此历史补丁。
