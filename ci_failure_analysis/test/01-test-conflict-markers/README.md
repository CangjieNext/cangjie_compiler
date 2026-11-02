# 01 测试残留三方冲突与内部 fileID 断言

补丁编号：`patch 01`。

## 分类

主分类：`test`。测试源码残留冲突标记以及内部 fileID 断言不稳定，修复仅涉及测试材料。

相关模块：stdlib、宏、runtime。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2052`](../../../j_p75yFw7h.txt#L2052)：`LLT/API/std/libast/nodes/test_annotation_exception_001.cj`
- [`j_p75yFw7h.txt:2085`](../../../j_p75yFw7h.txt#L2085)：`LLT/API/std/libast/decls/classdecl/test_annotation_001.cj`
- [`j_p75yFw7h.txt:2119`](../../../j_p75yFw7h.txt#L2119)：`LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_2.cj`
- [`j_p75yFw7h.txt:2439`](../../../j_p75yFw7h.txt#L2439)：`LLT/API/std/libast/nodes/test_generic_param_exception_001.cj`
- [`j_p75yFw7h.txt:2570`](../../../j_p75yFw7h.txt#L2570)：`LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_3.cj`
- [`j_p75yFw7h.txt:2719`](../../../j_p75yFw7h.txt#L2719)：`LLT/API/std/libast/nodes/test_generic_constraint_exception_001.cj`
- [`j_p75yFw7h.txt:3331`](../../../j_p75yFw7h.txt#L3331)：`LLT/API/std/libast/decls/macroexpandparam/test_macro_expand_param.cj`
- [`j_p75yFw7h.txt:3522`](../../../j_p75yFw7h.txt#L3522)：`LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_1.cj`
- [`j_p75yFw7h.txt:3621`](../../../j_p75yFw7h.txt#L3621)：`LLT/API/std/libast/nodes/test_body_exception_001.cj`
- [`j_p75yFw7h.txt:4391`](../../../j_p75yFw7h.txt#L4391)：`LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_6.cj`
- [`j_p75yFw7h.txt:4474`](../../../j_p75yFw7h.txt#L4474)：`LLT/API/std/libast/types/reftype/test_exception_001.cj`
- [`j_p75yFw7h.txt:4523`](../../../j_p75yFw7h.txt#L4523)：`LLT/API/std/libast/tokens/toTokens/test_primitive_totokens_001.cj`
- [`j_p75yFw7h.txt:4623`](../../../j_p75yFw7h.txt#L4623)：`LLT/API/std/libast/nodes/test_constructor_exception_001.cj`
- [`j_p75yFw7h.txt:4707`](../../../j_p75yFw7h.txt#L4707)：`LLT/API/std/libast/tokens/toTokens/test_string_totokens_001.cj`
- [`j_p75yFw7h.txt:4861`](../../../j_p75yFw7h.txt#L4861)：`LLT/Runtime/CJNative/debuginfo_zero_size_static_var.cj`

去重后涉及 15 个用例。日志中的同一用例重试仅计算一次。

## 根因分析

整合快照把三方冲突标记提交进测试源码和输出注释。源码中的标记造成语法错误，注释中的标记及变化的 fileID 使比较器拒绝正确输出。

冲突标记不只是注释：部分标记位于真实源码，直接造成 parse error。宏 token 的语义断言仍检查类型和值；移除的是内部文件编号、位置和冲突产生的整段旧输出。

## 修改方案

保留 main 侧按语义匹配的断言；宏插值断言保留 token 类型和值，去掉不稳定的内部 fileID。

修改文件：

- [cangjie_test/testsuites/LLT/API/std/libast/decls/classdecl/test_annotation_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/decls/classdecl/test_annotation_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/decls/macroexpandparam/test_macro_expand_param.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/decls/macroexpandparam/test_macro_expand_param.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/nodes/test_annotation_exception_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/nodes/test_annotation_exception_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/nodes/test_body_exception_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/nodes/test_body_exception_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/nodes/test_constructor_exception_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/nodes/test_constructor_exception_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/nodes/test_generic_constraint_exception_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/nodes/test_generic_constraint_exception_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/nodes/test_generic_param_exception_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/nodes/test_generic_param_exception_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/tokens/toTokens/test_primitive_totokens_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/tokens/toTokens/test_primitive_totokens_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/tokens/toTokens/test_string_totokens_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/tokens/toTokens/test_string_totokens_001.cj)
- [cangjie_test/testsuites/LLT/API/std/libast/types/reftype/test_exception_001.cj](../../../../cangjie_test/testsuites/LLT/API/std/libast/types/reftype/test_exception_001.cj)
- [cangjie_test/testsuites/LLT/Runtime/CJNative/debuginfo_zero_size_static_var.cj](../../../../cangjie_test/testsuites/LLT/Runtime/CJNative/debuginfo_zero_size_static_var.cj)
- [cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_1.cj](../../../../cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_1.cj)
- [cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_2.cj](../../../../cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_2.cj)
- [cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_3.cj](../../../../cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_3.cj)
- [cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_6.cj](../../../../cangjie_test/testsuites/LLT/compiler/Macro/nested/inQuoteDollarInterpolate/test_macro_6.cj)

## 验证结果

原测试框架定向运行全部 15 个日志失败用例：15/15 通过。日志：/tmp/manifest-conflict-tests.log。

所有本机执行结果来自 Linux x86_64；未执行 macOS 构建。`/tmp` 路径是本次本地验证记录，不是仓库依赖。

## 提交

- `cangjie_test`：`94a3f6ed45782c08988b651fefe1deca1e4443ff`；提交标题：`[patch 01] test: resolve imported conflict markers and stabilize macro token checks`

## 修复补丁

补丁由本报告对应的修复提交使用 `git format-patch --full-index --binary` 导出，保留提交说明和作者信息。

| 仓库 | 补丁文件 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `ab8934ea3f46bc74c56914f1f9ecef128a623d3a` |

在尚未包含修复、且代码版本与补丁基线兼容的工作副本中，从工作区根目录执行：

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/test/01-test-conflict-markers/cangjie_test.patch
```

当前工作区已包含这些修复，无需重复应用。跨仓补丁需要分别应用到表中对应仓库；整体应用顺序见[总览](../../README.md)。
