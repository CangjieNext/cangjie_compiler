# 2026-11-05 三份 CI 失败日志修复

三份日志去重后共 **13 个失败用例、5 类根因**，对应 **patch 17—21**。分类为 lsp、stdlib、test；sema、chir、codegen、runtime 本批没有新增根因，七类以外的问题：无。

| 日志 | 去重用例数 | 根因编号 |
| --- | ---: | --- |
| [j_2CHGcoDT.txt](../j_2CHGcoDT.txt) | 7 | 17 |
| [j_9LCB8BBR.txt](../j_9LCB8BBR.txt) | 3 | 18、20、21 |
| [j_beJnOSEo.txt](../j_beJnOSEo.txt) | 3 | 19 |

## 根因与补丁

| 编号 | 分类 | 分析报告 | 独立 patch | 原失败数 | 验证 |
| --- | --- | --- | --- | ---: | --- |
| 17 | lsp | [HLT LSP 补全候选及 deprecated 基线过期](lsp/17-hlt-completion-baselines/README.md) | [cangjie_test](lsp/17-hlt-completion-baselines/cangjie_test.patch) | 7 | 捕获响应 14/14；真实 LSP 比较 7/7（见等待条件说明） |
| 18 | test | [ArrayList 闭区间异常诊断预期过期](test/18-arraylist-closed-range-message/README.md) | [cangjie_test](test/18-arraylist-closed-range-message/cangjie_test.patch) | 1 | 1/1 文件，58/58 TestCase |
| 19 | stdlib | [Array.copyTo 普通与 local? 重载遗漏 main 新诊断](stdlib/19-array-copy-diagnostics/README.md) | [cangjie_runtime](stdlib/19-array-copy-diagnostics/cangjie_runtime.patch)、[cangjie_test](stdlib/19-array-copy-diagnostics/cangjie_test.patch) | 3 | 3/3 原例，另 1 个 local-mode 回归通过 |
| 20 | test | [Trace 安全测试中的波浪号被 shell 展开](test/20-trace-literal-tilde/README.md) | [cangjie_test](test/20-trace-literal-tilde/cangjie_test.patch) | 1 | 1/1 文件，四条运行命令通过 |
| 21 | test | [错误处理回调测试默认扩栈到 1GB 导致超时](test/21-error-handler-worker-stack/README.md) | [cangjie_test](test/21-error-handler-worker-stack/cangjie_test.patch) | 1 | 1/1 框架比较；额外重复 10/10 |

## 验证范围

- **13 个原失败用例的定向验证通过**，额外的 local-mode 复制边界回归通过。LLT 4/4，ByteArrayStream HLT 3/3，真实 LSP 比较 7/7。
- **LSP 条件说明：** 本机现有 SDK 初始化较慢，验证用例的临时副本把等待从 30 秒延至 180 秒，并配置本地 LSP 路径、CANGJIE_STDX_PATH。仓库的 lsp_test.py 未改，精确比较未放宽；两轮原 CI 捕获响应使用原比较器回放 14/14 通过。
- 标准库完整构建 203 步并安装，core/io/collection/unittest 共 12 个 CJO/静态库/动态库产物与 SDK 哈希一致。
- 栈溢出用例仍真实触发错误处理路径，额外重复 10/10；最大 RSS 8960 KB。源码使用线程级扩栈控制，保留 SDK stack-map 模式。
- 本机执行平台为 Linux x86_64。未运行完整 CI、macOS 或 Windows 流水线。

运行记录位于 `/tmp/manifest-20261105-*`，各分析报告列出具体结果及日志文件。

## 提交组织与使用

本目录有 **6 个独立 patch 文件**，每个根因在每个相关仓库各一份；19 跨 runtime/test 两仓。按 17、18、19、20、21 的顺序移植即可。当前工作区已经包含全部修复。

沿用此前的整理方式，每个仓库的 feature_local_mode 最终只保留一个相对 main 的汇总提交。独立修复提交及原分支已备份，分析报告记录导出提交、父提交和当前汇总提交；这些 patch 继续只包含对应根因的修改。

| 源码仓库 | 当前汇总提交 |
| --- | --- |
| `cangjie_test` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` |
| `cangjie_runtime` | `b94d9cd38f30048c5291a7da4b29b3cf8c7f807c` |

compiler 新增本分析目录，源码分析基线为 `5446df4ffbb8c98740f6cd22bfc9d9e70b6d1d6f`。其他仓库的源码未因本批修复发生变化。尚未推送。

备份引用前缀：`refs/codex-backups/ci-20261105-pm_b2r9d`（`-before` 为原分支，`-fixes` 为独立修复提交链）。
