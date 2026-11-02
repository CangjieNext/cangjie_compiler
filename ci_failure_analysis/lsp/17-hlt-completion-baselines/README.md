# 17 HLT LSP 补全候选及 deprecated 基线过期

补丁编号：`patch 17`。分类：`lsp`。

[返回总览](../../README.md) · [分类目录](../README.md)

## 日志证据

- [j_2CHGcoDT.txt:13590](../../../j_2CHGcoDT.txt#L13590)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_007.info`
- [j_2CHGcoDT.txt:13617](../../../j_2CHGcoDT.txt#L13617)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_087.info`
- [j_2CHGcoDT.txt:13646](../../../j_2CHGcoDT.txt#L13646)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_009.info`
- [j_2CHGcoDT.txt:13673](../../../j_2CHGcoDT.txt#L13673)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_090.info`
- [j_2CHGcoDT.txt:13706](../../../j_2CHGcoDT.txt#L13706)：`HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_001.info`
- [j_2CHGcoDT.txt:13739](../../../j_2CHGcoDT.txt#L13739)：`HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_012.info`
- [j_2CHGcoDT.txt:13772](../../../j_2CHGcoDT.txt#L13772)：`HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_011.info`

去重后共 7 个原失败用例。

## 根因分析

这七个 HLT 预期仍描述旧的标准库公共 API。local mode 版本移除了 Array/String 的过渡转换接口，异常类使用 UnimplementedException，并更新 CPointerHandle 构造函数的 deprecated 标记。此前仅恢复了 completion_007 的响应 ID 135，候选列表仍是旧版本；tools 仓最新补丁不修改这些 HLT 文件。

CI 日志已收到完整响应并进入 `compare fail`，不是驱动 30 秒等待超时。例如 `completion_007` 的响应 ID 正确为 135，但预期有 109 个候选，实际只有 99 个；其他用例还有接口、异常类名和 deprecated 字段差异。180 秒只用于本机初始化较慢时的临时验证副本，不属于本次修复 patch。

## 修改方案

按当前公共声明同步七个 Rev# JSON 的候选集合、异常名称与 deprecated 字段。请求区和响应 ID 保持原样，继续使用原比较器的精确候选检查。当前补丁先与两轮 CI 捕获响应逐项比较，再运行真实 LSP 服务；没有修改 tools 最新补丁。

修改文件：

- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_007.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_007.info)
- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_009.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_009.info)
- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_087.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_087.info)
- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_090.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_090.info)
- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_001.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_001.info)
- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_011.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_011.info)
- [cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_012.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_012.info)

## 验证结果

两轮捕获响应共 **14/14** 比较通过；七个原 HLT 命令启动真实 LSP 服务后，**7/7** 比较及进程返回码通过。

本机验证使用独立临时目录复制用例及声明的依赖，临时配置本地 LSP 路径与 CANGJIE_STDX_PATH。本机初始化超过原驱动的 30 秒等待，因此仅将验证副本的等待改为 180 秒；仓库中的 lsp_test.py 保持原样。这是本次端到端验证的限制，不将原 30 秒上限下的本机执行记作通过。

本机记录：

- `/tmp/manifest-20261105-lsp-replay.json`
- `/tmp/manifest-20261105-lsp-live-results.json`
- `/tmp/manifest-20261105-lsp-live-plan.json`

## 补丁与提交

各根因独立导出 patch；工作分支按此前要求汇总为每仓一个提交。表中的分解提交保留在备份引用下，用于导出和审阅本项修改。

| 仓库 | 独立补丁 | 导出提交 | 父提交 | 当前汇总提交 |
| --- | --- | --- | --- | --- |
| `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `e41ec80744413ca473fd69b47ab78f4c3fdcf425` | `b4cdabd10e654521d3562cbc476c2b282ce4b651` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` |

当前源码已包含修复。向兼容基线移植时，在相应仓库执行 `git am /path/to/lsp/17-hlt-completion-baselines/仓库名.patch`。跨仓问题 19 分别应用 runtime 和 test 补丁，先重建、安装库后再运行测试。
