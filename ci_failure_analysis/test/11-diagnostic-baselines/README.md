# 11 负例 SCAN-IN 诊断基线与库源码行号更新

补丁编号：`patch 11`。

## 分类

主分类：`test`。相关模块：sema、stdlib。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2484`](../../../j_p75yFw7h.txt#L2484)：`LLT/compiler/CHIROpt/Devirtualization/inherit_same_interface_sema_error.cj`
- [`j_p75yFw7h.txt:3238`](../../../j_p75yFw7h.txt#L3238)：`LLT/API/std/collection/local_mode/test_arraylist_local_zip_neg_q_other.cj`
- [`j_p75yFw7h.txt:3671`](../../../j_p75yFw7h.txt#L3671)：`LLT/API/stdx/encoding/json/local_mode/test_json_format_local_neg_indent.cj`

## 根因分析

三个用例均已按预期编译失败，差异来自过期的方法签名及库源码位置。恢复并统一使用完整 SCAN-IN，保留错误、候选说明、源码行号和错误总数，未使用 ASSERT。

## 修改方案

ArrayList 的 zip 候选定位更新到 array_list.cj:1925:25；JSON toJsonString 的两个候选定位更新到 json_object.cj:829:25 和 833:17；继承负例完整匹配 f1(a: Int64): Unit 的诊断及第 27 行源码位置。

修改文件：

- [testsuites/LLT/API/std/collection/local_mode/test_arraylist_local_zip_neg_q_other.cj](../../../../cangjie_test/testsuites/LLT/API/std/collection/local_mode/test_arraylist_local_zip_neg_q_other.cj)
- [testsuites/LLT/API/stdx/encoding/json/local_mode/test_json_format_local_neg_indent.cj](../../../../cangjie_test/testsuites/LLT/API/stdx/encoding/json/local_mode/test_json_format_local_neg_indent.cj)
- [testsuites/LLT/compiler/CHIROpt/Devirtualization/inherit_same_interface_sema_error.cj](../../../../cangjie_test/testsuites/LLT/compiler/CHIROpt/Devirtualization/inherit_same_interface_sema_error.cj)

## 验证结果

原 LLT 框架定向运行 patch 11 的三个用例与 patch 12 的一个用例：4/4 通过。记录：`/tmp/manifest-scan-baselines-check.log`、`/tmp/manifest-scan-baselines-results.json`。完整重建远端标准库和 stdx 后，四个用例再次全部通过（4/4），结果为 `/tmp/manifest-upstream-validation-results.json`。

本机平台为 Linux x86_64，未执行 macOS 构建。

## 提交

- `cangjie_test`：`c79c9c1bc406284b9fb05c5164b6fee8d7999475`；提交标题：`[patch 11] test: update diagnostic SCAN-IN baselines`

## 修复补丁

基于父提交 `1c8cc4fa9242d8d2f619256ebec3bde2f37d97e6`。补丁直接覆盖原修复提交，没有追加修订提交。

[cangjie_test.patch](cangjie_test.patch)

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/test/11-diagnostic-baselines/cangjie_test.patch
```

当前工作区已包含修复，无需重复应用。
