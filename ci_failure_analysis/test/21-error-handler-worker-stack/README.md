# 21 错误处理回调测试默认扩栈到 1GB 导致超时

补丁编号：`patch 21`。分类：`test`。

[返回总览](../../README.md) · [分类目录](../README.md)

## 日志证据

- [j_9LCB8BBR.txt:2953](../../../j_9LCB8BBR.txt#L2953)：`LLT/API/std/core/error_handler_stackoverflow_throws_error.cj`

去重后共 1 个原失败用例。

## 根因分析

该用例通过真实递归触发 StackOverflowError，再由错误处理回调触发第二次 Error。虽然已用 VArray 扩大递归帧，运行时仍允许协程扩栈到 1GB（schedule.h 的 CJTHREAD_MAX_STACK_SIZE）。大帧只能减少递归层数，仍依赖扩栈极限；CI 两轮超时后重试又通过，本机原例也可通过。结合扩栈路径判断，这是测试构造在高负载下的内存与扫描压力问题。

## 修改方案

把该用例初始协程栈固定为 1MB，在触发溢出的 worker 内通过现有 CJ_CJThreadSetStackGrow(false) 关闭该线程继续扩栈，并检查返回码。保留原递归、原错误处理器及其二次 Error 路径，增加 StackOverflowError 输出检查。

使用线程级接口可保留 SDK 的全局 stack-map 模式；直接用 cjStackGrow=0 会与带 stack-pointer map 的 SDK 不兼容。Unix 和 Windows 分别设置初始栈大小，本次只验证 Linux x86_64。

修改文件：

- [cangjie_test/testsuites/LLT/API/std/core/error_handler_stackoverflow_throws_error.cj](../../../../cangjie_test/testsuites/LLT/API/std/core/error_handler_stackoverflow_throws_error.cj)

## 验证结果

原 LLT 框架在重建后的 SDK 上 **1/1** 通过。编译后的程序并发重复 **10/10** 均触发 StackOverflowError 及 throws an error in the error handler；最长 2.29 秒，最大 RSS **8960 KB**。程序按原测试设计异常终止，框架通过比较器验证预期错误输出。

本机记录：

- `/tmp/manifest-20261105-llt-fixed-results.json`
- `/tmp/manifest-20261105-stack-repeat-results.json`
- `/tmp/manifest-20261105-stack-verified-results.json`

## 补丁与提交

各根因独立导出 patch；工作分支按此前要求汇总为每仓一个提交。表中的分解提交保留在备份引用下，用于导出和审阅本项修改。

| 仓库 | 独立补丁 | 导出提交 | 父提交 | 当前汇总提交 |
| --- | --- | --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `01a0a67650cfefb7be716fa6a0870c8cf5097c96` | `c2627e11603be99ef651b96f7b823adaa9f2111d` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` |

当前源码已包含修复。向兼容基线移植时，在相应仓库执行 `git am /path/to/test/21-error-handler-worker-stack/仓库名.patch`。跨仓问题 19 分别应用 runtime 和 test 补丁，先重建、安装库后再运行测试。
