# stdlib

[返回总览](../README.md)

本类包含 std 和 stdx 的六项问题。05—09 使用上游修复，保留五个历史参考 patch；13 已重新合入当前 runtime，并更新导出 patch。六项问题的 19 个原失败用例本次 **19/19 通过**。

| 编号 | 分析报告 | 本次验证结果 | 当前源码提交 | 补丁 |
| --- | --- | --- | --- | --- |
| 05 | [普通 UTF-8 校验遗漏新诊断](05-utf8-messages/README.md) | 4/4 通过 | 上游 `59b8426871` | [参考 patch](05-utf8-messages/cangjie_runtime.patch) |
| 06 | [Rune UTF-8 解码遗漏负索引检查](06-rune-negative-index/README.md) | 1/1 通过 | 上游 `59b8426871` | [参考 patch](06-rune-negative-index/cangjie_runtime.patch) |
| 07 | [ArrayList 闭区间切片越过逻辑长度](07-arraylist-closed-range/README.md) | 1/1 通过 | 上游 `59b8426871` | [参考 patch](07-arraylist-closed-range/cangjie_runtime.patch) |
| 08 | [JsonArray 缺少 local 下标重载](08-json-local-index/README.md) | 11/11 通过 | 上游 `fcfc21ffff` | [参考 patch](08-json-local-index/cangjie_stdx.patch) |
| 09 | [local JSON 浮点序列化遗漏非有限值检查](09-json-nonfinite/README.md) | 1/1 通过 | 上游 `e39f677e2e` | [参考 patch](09-json-nonfinite/cangjie_stdx.patch) |
| 13 | [std.ast 词法选项迁移遗漏 effect handler 开关](13-ast-eh-lexer/README.md) | 1/1 通过 | 本地 `2ddd31937e` | [修复 patch](13-ast-eh-lexer/cangjie_runtime.patch) |

本次框架结果：`/tmp/manifest-patch13-applied-results.json`；日志：`/tmp/manifest-patch13-applied-tests.log`。13 同目录的 EH 负例及两个词法异常用例也通过，合计 4/4。
