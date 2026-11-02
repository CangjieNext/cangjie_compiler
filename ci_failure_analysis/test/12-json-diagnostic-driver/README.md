# 12 数组负例的冲突残留与文本 SCAN 基线

补丁编号：`patch 12`。

## 分类

主分类：`test`。相关模块：sema、stdlib。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:4038`](../../../j_p75yFw7h.txt#L4038)：`LLT/compiler/Sema/array/array_invalid_api.cj`

## 根因分析

原日志中 cjc-frontend 已能输出 JSON，但测试源码残留三方冲突标记，首先产生 expected declaration, found '<<'，未进入预期的三个语义错误检查。合并内容还混合了文本 SCAN 和 JSON ASSERT。原先将其归因于 frontend 不支持 JSON 不准确。

## 修改方案

保留 %frontendCompiler 入口，清理冲突标记及重复预期；按要求使用文本 SCAN，令 %cmp_opt 选择 noColor 文本输出。完整核对 clone 歧义、size 不可变、clone 参数类型不符三种错误及总数 3；更新库位置为 array.cj:632、677 和 array_common.cj:86。不改成 cjc，不使用 ASSERT。

修改文件：

- [testsuites/LLT/compiler/Sema/array/array_invalid_api.cj](../../../../cangjie_test/testsuites/LLT/compiler/Sema/array/array_invalid_api.cj)

## 验证结果

原 LLT 框架定向运行 patch 11 的三个用例与 patch 12 的一个用例：4/4 通过。记录：`/tmp/manifest-scan-baselines-check.log`、`/tmp/manifest-scan-baselines-results.json`。完整重建远端标准库和 stdx 后，四个用例再次全部通过（4/4），结果为 `/tmp/manifest-upstream-validation-results.json`。

本机平台为 Linux x86_64，未执行 macOS 构建。

## 提交

- `cangjie_test`：`0ef5c93c24fdf1c0899e98e2118a07dc7c2340b7`；提交标题：`[patch 12] test: refresh frontend array diagnostic SCAN baseline`

## 修复补丁

基于父提交 `c79c9c1bc406284b9fb05c5164b6fee8d7999475`。补丁直接覆盖原修复提交，没有追加修订提交。

[cangjie_test.patch](cangjie_test.patch)

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/test/12-json-diagnostic-driver/cangjie_test.patch
```

当前工作区已包含修复，无需重复应用。
