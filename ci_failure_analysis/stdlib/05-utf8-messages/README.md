# 05 普通 UTF-8 校验遗漏新诊断

补丁编号：`patch 05`。

## 分类

主分类：`stdlib`。修复 std.core 普通 UTF-8 校验重载的错误信息。

相关模块：std.core。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2290`](../../../j_p75yFw7h.txt#L2290)：`LLT/API/std/core/String/test_string_no_simd.cj`
- [`j_p75yFw7h.txt:3130`](../../../j_p75yFw7h.txt#L3130)：`LLT/API/std/core/test_string.cj`
- [`j_p75yFw7h.txt:3438`](../../../j_p75yFw7h.txt#L3438)：`LLT/API/std/core/StringBuilder/test_append.cj`
- [`j_p75yFw7h.txt:4944`](../../../j_p75yFw7h.txt#L4944)：`LLT/API/std/core/String/test_string_all.cj`

去重后涉及 4 个用例。日志中的同一用例重试仅计算一次。

## 当前处理状态

源码仓中的本地修复提交仍保持撤销。导出的 `.patch` 文件已恢复作为分析附件，未重新应用到源码。

上次验证代码来自 `cangjie_runtime` 的远端 `origin/feature_local_mode`，核对版本 `59b84268718a52946f0c965fb7cc56a45947cfda`。

**上游已包含相应实现修复，且本次验证通过。** 恢复的补丁仅供对照，未重复应用到当前源码。

## 根因分析

checkInvalid 的普通重载仍使用旧版笼统错误信息，而 @local? 重载已经使用包含字节和索引的 Invalid UTF-8 诊断，破坏普通字符串和 StringBuilder 的异常信息契约。

两个分支分别是“缺失 leading byte 的 continuation byte”和“非法 leading byte”。普通重载恢复 Invalid UTF-8、具体字节值及索引，与已有 local 重载保持一致；合法输入的解析流程不变。

## 修改方案

上游同步普通重载的两条错误信息，并使用普通 arrayGetUnchecked 读取，与对应参数模式一致。

修改文件：

- [cangjie_runtime/stdlib/libs/std/core/string_common.cj](../../../../cangjie_runtime/stdlib/libs/std/core/string_common.cj)

## 验证结果

已从远端版本完整重建并安装标准库（203 个构建步骤）及 stdx（135 个构建步骤）。本组 **4/4 通过**。

原框架结果：`/tmp/manifest-upstream-validation-results.json`；日志：`/tmp/manifest-upstream-validation.log`。

## 当前提交

- 验证基线：`cangjie_runtime` `59b84268718a52946f0c965fb7cc56a45947cfda`。

## 参考补丁

[cangjie_runtime.patch](cangjie_runtime.patch) 已恢复，文件内容与删除前完全一致。

| 仓库 | 历史导出提交 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_runtime` | `304252ec3b73c4ff9c6e62f7dc3bbbd13cbd2cd0` | `019406d5d5dbb0ed20d27aeb3ad2e603653aeb7d` |

这里只恢复导出文件，未重新应用到源码；当前仍保留上游代码及上次实跑结果。已包含上游修复的问题无需重复应用此历史补丁。
