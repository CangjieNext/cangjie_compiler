# 10 CHIR Lambda 默认参数归属 API 迁移

补丁编号：`patch 10`。

## 分类

主分类：`chir`。属于 CHIR 默认参数归属 API 的专项用例适配，修改位于 test 仓。

相关模块：stdlib（stdx.chir）、test。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2992`](../../../j_p75yFw7h.txt#L2992)：`LLT/API/stdx/chir/function/Expr_try_kinds_props_base_eq.cj`

去重后涉及 1 个用例。日志中的同一用例重试仅计算一次。

## 根因分析

测试继续访问已移除的 Lambda.paramDftValHostFunc；当前 CHIR 模型用 paramDftValHostParam 指向默认参数本身，序列化和打印也已迁移。

新字段指向 Parameter，而不是宿主 Lambda。测试将宿主 lambda 的第一个参数赋给 paramDftValHostParam，同时检查读回的对象及 dump 字段名，保留原测试的默认参数归属验证。

## 修改方案

测试绑定 hostLam.parameters[0]，检查 getter 返回同一参数和新打印字段，保持默认参数归属覆盖。

修改文件：

- [cangjie_test/testsuites/LLT/API/stdx/chir/function/Expr_try_kinds_props_base_eq.cj](../../../../cangjie_test/testsuites/LLT/API/stdx/chir/function/Expr_try_kinds_props_base_eq.cj)

## 验证结果

完整构建并安装 stdx（135 个构建步骤），使用 target/linux_x86_64_cjnative/dynamic/stdx 的新产物运行原 LLT 框架；本组全部用例通过，合计批次 14/14 通过。日志：/tmp/manifest-stdx-tests-fixed.log。

所有本机执行结果来自 Linux x86_64；未执行 macOS 构建。`/tmp` 路径是本次本地验证记录，不是仓库依赖。

## 提交

- `cangjie_test`：`1c8cc4fa9242d8d2f619256ebec3bde2f37d97e6`；提交标题：`[patch 10] test: migrate CHIR lambda default parameter ownership checks`

## 修复补丁

补丁由本报告对应的修复提交使用 `git format-patch --full-index --binary` 导出，保留提交说明和作者信息。

| 仓库 | 补丁文件 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `0f1522aa18d595bcf0628200dcdaac6910219e5c` |

在尚未包含修复、且代码版本与补丁基线兼容的工作副本中，从工作区根目录执行：

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/chir/10-chir-default-parameter-api/cangjie_test.patch
```

当前工作区已包含这些修复，无需重复应用。跨仓补丁需要分别应用到表中对应仓库；整体应用顺序见[总览](../../README.md)。
