# 09 local JSON 浮点序列化遗漏非有限值检查

补丁编号：`patch 09`。

## 分类

主分类：`stdlib`。修复 stdx.encoding.json local 序列化路径的非有限浮点值检查。

相关模块：stdx.encoding.json。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:4819`](../../../j_p75yFw7h.txt#L4819)：`LLT/API/stdx/encoding/json/local_mode/test_json_scalar_local.cj`

去重后涉及 1 个用例。日志中的同一用例重试仅计算一次。

## 当前处理状态

源码仓中的本地修复提交仍保持撤销。导出的 `.patch` 文件已恢复作为分析附件，未重新应用到源码。

上次验证代码来自 `cangjie_stdx` 的远端 `origin/feature_local_mode`，核对版本 `e39f677e2e09de00557e3f528b5e9e2eb7c3f6b8`。

**上游已包含相应实现修复，且本次验证通过。** 恢复的补丁仅供对照，未重复应用到当前源码。

## 根因分析

普通 JsonFloat 和 WriteBuffer 拒绝 NaN/Infinity，local 路径直接调用 Float64.toString 或 C 格式化函数，产生与普通路径不同的行为。

覆盖 NaN、正无穷和负无穷；JsonFloat.toString、toStringWithoutEscaping 以及 WriteBuffer 的 local 路径均复用普通路径的检查与异常文案。有限值仍按原逻辑序列化。

## 修改方案

在 local JsonFloat 字符串方法及 WriteBuffer.append(Float64) 中同步拒绝非有限值的 JsonException。

修改文件：

- [cangjie_stdx/src/stdx/encoding/json/json_object.cj](../../../../cangjie_stdx/src/stdx/encoding/json/json_object.cj)
- [cangjie_stdx/src/stdx/encoding/json/write_buffer.cj](../../../../cangjie_stdx/src/stdx/encoding/json/write_buffer.cj)

## 验证结果

已从远端版本完整重建并安装标准库（203 个构建步骤）及 stdx（135 个构建步骤）。本组 **1/1 通过**。

原框架结果：`/tmp/manifest-upstream-validation-results.json`；日志：`/tmp/manifest-upstream-validation.log`。

## 当前提交

- 验证基线：`cangjie_stdx` `e39f677e2e09de00557e3f528b5e9e2eb7c3f6b8`。

## 参考补丁

[cangjie_stdx.patch](cangjie_stdx.patch) 已恢复，文件内容与删除前完全一致。

| 仓库 | 历史导出提交 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_stdx` | `7dc0859df3b0378ba17e3fe9e97fd34203f24077` | `c1a524aeaab8ef680a7840eac0bf7b2d0bcca783` |

这里只恢复导出文件，未重新应用到源码；当前仍保留上游代码及上次实跑结果。已包含上游修复的问题无需重复应用此历史补丁。
