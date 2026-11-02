# 14 Nothing 清理测试检查的 dump 阶段过早

补丁编号：`patch 14`。

## 分类

主分类：`chir`。属于 Nothing 消除与不可达块清理后的 IR 专项断言，修改位于 test 仓。

相关模块：test。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:3907`](../../../j_p75yFw7h.txt#L3907)：`LLT/compiler/CHIR/ModalType/nothing_type_region_pair01.cj`

去重后涉及 1 个用例。

## 根因分析

原 local mode 分支和当前分支均已在 DCE 的 Nothing 消除逻辑中调用 `AppendEndRegionIfNeeded`，在新 `Exit` 前补回仍未关闭区域的 `EndRegion`。日志中的 `%12 = EndRegion()` 就是补回的清理。

测试原先检查 `NothingTypeExprElimination` 刚结束的中间 dump。这个阶段已改写退出路径，但旧的不可达 continuation block 尚未被删除；其中仍有 `%9 = EndRegion()`。对整个函数要求 `scan-1 EndRegion()` 因而计数为 2。

随后 `UnreachableBlockElimination` 从入口遍历 CFG 并删除不可达块。对同一用例的实际 dump 核对结果：

| dump 阶段 | `foo` 中的 block | `EndRegion()` 数量 |
| --- | --- | ---: |
| `7_NothingTypeExprElimination` | 0、2、3 | 2 |
| `8_UnreachableBlockElimination` | 仅 0 | 1 |

阶段调用与 dump 时机见 [CHIR.cpp](../../../src/CHIR/CHIR.cpp:1103)，不可达块删除见 [DeadCodeElimination.cpp](../../../src/CHIR/Optimization/DeadCodeElimination.cpp:765)。

## 修改方案

只将用例中选择 dump 的文件名后缀从 `_NothingTypeExprElimination.chirtxt` 改为 `_UnreachableBlockElimination.chirtxt`。继续提取整个 `foo` 函数，保留原有 `StartRegion()`、`EndRegion()` 各一次断言和程序编译、运行检查。

本修复的净修改只有一行，不增加筛选脚本、依赖或断言。

修改文件：

- [cangjie_test/testsuites/LLT/compiler/CHIR/ModalType/nothing_type_region_pair01.cj](../../../../cangjie_test/testsuites/LLT/compiler/CHIR/ModalType/nothing_type_region_pair01.cj)

## 验证结果

用原 LLT 框架定向运行最终版本的用例：1/1 通过。验证日志：`/tmp/manifest-nothing-after-unreachable-final.log`；结构化结果：`/tmp/manifest-nothing-after-unreachable-final-results.json`。

重新导出的补丁仅包含该用例的一行修改；已核对补丁可在对应父提交上应用，并与提交的实际 diff 一致。

本机验证平台为 Linux x86_64，未执行 macOS 构建。`/tmp` 路径为本次本地验证记录。

## 提交

- `cangjie_test`：`cd1e10565d83c8051b4b417c2d5eb13d33b774ed`；提交标题：`[patch 14] test: verify Nothing region cleanup after unreachable block elimination`

## 修复补丁

补丁由上述最终修复提交使用 `git format-patch --full-index --binary` 导出，保留提交说明和作者信息。

| 仓库 | 补丁文件 | 基于的父提交 |
| --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `0ef5c93c24fdf1c0899e98e2118a07dc7c2340b7` |

在尚未包含修复、且代码版本与补丁基线兼容的工作副本中，从工作区根目录执行：

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/chir/14-nothing-region-cleanup/cangjie_test.patch
```

当前工作区已包含修复，无需重复应用。整体应用顺序见[总览](../../README.md)。
