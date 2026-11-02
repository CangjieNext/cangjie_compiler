# 19 Array.copyTo 普通与 local? 重载遗漏 main 新诊断

补丁编号：`patch 19`。分类：`stdlib`。

[返回总览](../../README.md) · [分类目录](../README.md)

## 日志证据

- [j_beJnOSEo.txt:18554](../../../j_beJnOSEo.txt#L18554)：`HLT/API/std/io/ByteArrayStream/ByteArrayStream_seek_Begin.cj`
- [j_beJnOSEo.txt:18764](../../../j_beJnOSEo.txt#L18764)：`HLT/API/std/io/ByteArrayStream/ByteArrayStream_seek_End.cj`
- [j_beJnOSEo.txt:19477](../../../j_beJnOSEo.txt#L19477)：`HLT/API/std/io/ByteArrayStream/ByteArrayStream_seek_Current.cj`

去重后共 3 个原失败用例。

## 根因分析

ByteBuffer.read 在 seek 越过末尾后向 Array.copyTo 传入 copyLen=-5。main 和 local! 重载使用含参数值的新诊断，普通和 local? 重载却残留 Negative copy length. 等旧消息。三个 ByteArrayStream 用例期待的是 main 当前的诊断，因此这里应补齐库迁移时遗漏的内容。

## 修改方案

把普通及 local? copyTo 的五参数边界诊断与单参数目标过小诊断同步到 main；local! 已有相同诊断。共更新 12 条字符串，保持检查顺序、异常类型及三种复制 intrinsic 不变。已逐个核对六个重载与 main 对应函数的诊断列表完全一致。

同时在现有 test_local_array_copy_boundaries.cj 中加强 local? 与 local! 负长度异常消息检查；三个原 ByteArrayStream 用例验证普通重载。

修改文件：

- [cangjie_runtime/stdlib/libs/std/core/array.cj](../../../../cangjie_runtime/stdlib/libs/std/core/array.cj)
- [cangjie_test/testsuites/LLT/API/std/core/local_mode/test_local_array_copy_boundaries.cj](../../../../cangjie_test/testsuites/LLT/API/std/core/local_mode/test_local_array_copy_boundaries.cj)

## 验证结果

标准库完整构建 **203 步成功**并安装 SDK，核对 core/io/collection/unittest 的 CJO、静态库、动态库共 **12 个产物哈希一致**。三个原 HLT ByteArrayStream 用例 **3/3** 通过（内部 12 个 TestCase）；现有 local-mode 复制边界回归 **1/1** 通过。

本机记录：

- `/tmp/manifest-20261105-stdlib-build.log`
- `/tmp/manifest-20261105-stdlib-install.log`
- `/tmp/manifest-20261105-stdlib-sdk-hashes.json`
- `/tmp/manifest-20261105-byte-fixed-results.json`
- `/tmp/manifest-20261105-llt-fixed-results.json`

## 补丁与提交

各根因独立导出 patch；工作分支按此前要求汇总为每仓一个提交。表中的分解提交保留在备份引用下，用于导出和审阅本项修改。

| 仓库 | 独立补丁 | 导出提交 | 父提交 | 当前汇总提交 |
| --- | --- | --- | --- | --- |
| `cangjie_runtime` | [cangjie_runtime.patch](cangjie_runtime.patch) | `119e0ef5868e08e85f018f2d0b2da7df3689bb88` | `f3fafd0288bf53f00fc208eb4ee7e22ac7a9b37d` | `b94d9cd38f30048c5291a7da4b29b3cf8c7f807c` |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `ed606e82311cb14dc00981aa942d73bc4abfdfc6` | `c85975e76749394452694e982da5a8d8fe7de253` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` |

当前源码已包含修复。向兼容基线移植时，在相应仓库执行 `git am /path/to/stdlib/19-array-copy-diagnostics/仓库名.patch`。跨仓问题 19 分别应用 runtime 和 test 补丁，先重建、安装库后再运行测试。
