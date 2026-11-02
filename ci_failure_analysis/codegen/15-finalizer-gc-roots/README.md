# 15 local finalizer 提前执行被错误标成 GC leaf

补丁编号：`patch 15`。

## 分类

主分类：`codegen`。修复 LLVM 后端对 local finalizer 注册和移除调用的 GC 属性。

相关模块：runtime、GC。

[返回总览](../../README.md) · [分类目录](../README.md)

## 影响与日志证据

- [`j_p75yFw7h.txt:2688`](../../../j_p75yFw7h.txt#L2688)：`LLT/Runtime/CJNative/local_mode/finalizer/finalizer_contains_local_heap_ref_gc.cj`

## 根因分析

`CJInsertRemoveLocalFinalizer` 在最后一次使用处插入 `CJ_MCC_RemoveLocalFinalizer`，runtime 会在此同步执行可分配并触发 GC 的析构函数。显式 `gc-leaf-function` 属性使 statepoint 重写跳过调用，导致调用者仍存活的 StringBuilder 及 backing array 引用未正确重定位，后续打印触发 GC 断言。

用户提供的 [0001-fix-remote-gc-leaf-function-for-finaluzer.patch](../../../0001-fix-remote-gc-leaf-function-for-finaluzer.patch) 同时去掉了注册和移除调用声明上的显式 leaf 属性；两个声明原本都以 `GCLeafFunc=false` 创建。

## 修改方案

完全采用该补丁，净修改为两个文件各删除一行：

- [CJInsertRemoveLocalFinalizer.cpp](../../../../llvm-project/llvm/lib/Transforms/Scalar/CJInsertRemoveLocalFinalizer.cpp)：不再为 `CJ_MCC_RemoveLocalFinalizer` 添加 `gc-leaf-function`。
- [CJRuntimeLowering.cpp](../../../../llvm-project/llvm/lib/Transforms/Scalar/CJRuntimeLowering.cpp)：不再为 `CJ_MCC_AddLocalFinalizer` 添加 `gc-leaf-function`。

先前实现中额外清除已有声明属性的逻辑、注释和新增的 `gc_roots.ll` 已撤销。最终提交的代码 diff 与用户提供补丁的 stable patch ID 完全一致，保留补丁作者 yuqiaoyu 的信息，提交标题保留 `patch 15` 编号。

## 验证结果

重新构建 LLVM，安装新的 `libLLVM-15.so` 到本地 SDK，并核对构建产物与安装文件 SHA-256 一致。使用新编译出的测试程序验证：

- 原 LLT 用例和 7 个相邻 finalizer 用例：8/8 通过。
- 原失败用例默认构建重复运行：30/30 通过。
- 同一用例 `-O2` 构建重复运行：10/10 通过。
- 每次均检查最终 count=64、checksum=167552。
- LLVM 现有同目录 lit 测试：3/3 通过。

日志：`/tmp/manifest-provided-finalizer-llvm-build.log`、`/tmp/manifest-provided-finalizer-tests.log`、`/tmp/manifest-provided-finalizer-lit.log`；重复运行结果：`/tmp/manifest-provided-finalizer-repeat/results.json`。

随后切换远端标准库和 stdx、完整重建后，原 finalizer 用例再次通过；记录在 `/tmp/manifest-upstream-validation-results.json`。

验证平台为 Linux x86_64，未执行 macOS 构建。此结果针对用户指定补丁，不沿用此前版本的测试结果。

## 提交

- `llvm-project`：`73a56bb60f321d88a361801c8ed20c29403032b5`；提交标题：`[patch 15] fix: remote gc-leaf-function for finaluzer`

## 修复补丁

| 仓库 | 补丁文件 | 基于的父提交 |
| --- | --- | --- |
| `llvm-project` | [llvm-project.patch](llvm-project.patch) | `c46ae4d8ebe6e3947d2251364d6adb01c1faef86` |

导出的补丁保留用户补丁的代码和作者信息，并带有 `patch 15` 编号。在未包含修复的兼容版本上，从工作区根目录执行：

```bash
git -C llvm-project am ../cangjie_compiler/ci_failure_analysis/codegen/15-finalizer-gc-roots/llvm-project.patch
```

当前工作区已包含修复，无需重复应用。
