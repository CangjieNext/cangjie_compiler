# 08 JsonArray 缺少 local 下标重载

补丁编号：`patch 08`。

## 分类

主分类：`stdlib`。补全 stdx.encoding.json JsonArray 的 local 下标重载；stdlib 类同时包含 std 与 stdx 库实现。

相关模块：stdx.encoding.json。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2787`](../../../j_p75yFw7h.txt#L2787)：`LLT/API/stdx/encoding/json/local_mode/test_json_array_local.cj`
- [`j_p75yFw7h.txt:2921`](../../../j_p75yFw7h.txt#L2921)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_writer_mutation_paths.cj`
- [`j_p75yFw7h.txt:3064`](../../../j_p75yFw7h.txt#L3064)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_unterminated_unicode.cj`
- [`j_p75yFw7h.txt:3375`](../../../j_p75yFw7h.txt#L3375)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_array_ctor_storage.cj`
- [`j_p75yFw7h.txt:3794`](../../../j_p75yFw7h.txt#L3794)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_fast_slow_boundaries.cj`
- [`j_p75yFw7h.txt:3990`](../../../j_p75yFw7h.txt#L3990)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_float_boundaries.cj`
- [`j_p75yFw7h.txt:4562`](../../../j_p75yFw7h.txt#L4562)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_radix_boundaries.cj`
- [`j_p75yFw7h.txt:4776`](../../../j_p75yFw7h.txt#L4776)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_object_ctor_alias.cj`
- [`j_p75yFw7h.txt:5041`](../../../j_p75yFw7h.txt#L5041)：`LLT/API/stdx/encoding/json/local_mode/test_json_parser_local.cj`
- [`j_p75yFw7h.txt:5198`](../../../j_p75yFw7h.txt#L5198)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_container_growth.cj`
- [`j_p75yFw7h.txt:5237`](../../../j_p75yFw7h.txt#L5237)：`LLT/API/stdx/encoding/json/local_mode/test_json_local_list_ctor_alias.cj`

去重后涉及 11 个用例。日志中的同一用例重试仅计算一次。

## 当前处理状态

源码仓中的本地修复提交仍保持撤销。导出的 `.patch` 文件已恢复作为分析附件，未重新应用到源码。

上次验证代码来自 `cangjie_stdx` 的远端 `origin/feature_local_mode`，核对版本 `fcfc21ffffb51ff350e575bf0d4542f726a3c632`。

**上游已包含相应实现修复，且本次验证通过。** 恢复的补丁仅供对照，未重复应用到当前源码。

## 根因分析

JsonArray 只有普通 operator []，虽然 get 已支持 @local!/@local?，local 数组使用下标仍无法通过类型检查，导致多项 JSON 解析与容器测试停在编译阶段。

@local! 返回 JsonValue @local!，@local? 返回 JsonValue @local?；两者均复用相应 get(index)，None 保持原有 JsonException。11 个看似解析器、容器增长或别名问题的用例都被同一缺失下标重载阻塞，因此合并为一个 commit。

## 修改方案

新增两种 local 下标重载，复用 get，保留 JsonException 边界语义以及返回值的模式。

修改文件：

- [cangjie_stdx/src/stdx/encoding/json/json_object.cj](../../../../cangjie_stdx/src/stdx/encoding/json/json_object.cj)

## 验证结果

已从远端版本完整重建并安装标准库（203 个构建步骤）及 stdx（135 个构建步骤）。本组 **11/11 通过**。

原框架结果：`/tmp/manifest-upstream-validation-results.json`；日志：`/tmp/manifest-upstream-validation.log`。

## 当前提交

- 验证基线：`cangjie_stdx` `fcfc21ffffb51ff350e575bf0d4542f726a3c632`。

## 参考补丁

[cangjie_stdx.patch](cangjie_stdx.patch) 已恢复，文件内容与删除前完全一致。

| 仓库 | 历史导出提交 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_stdx` | `c1a524aeaab8ef680a7840eac0bf7b2d0bcca783` | `a6df7dccdd1858ef276e5626da83c957f1c6db03` |

这里只恢复导出文件，未重新应用到源码；当前仍保留上游代码及上次实跑结果。已包含上游修复的问题无需重复应用此历史补丁。
