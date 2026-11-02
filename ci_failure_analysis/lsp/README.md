# lsp

[返回总览](../README.md)

tools 使用最新 `pr1361-lsp-tests.patch`（提交 `b3a167b7c4`）；test 按最新要求单独恢复 03 的预期 ID 修复（提交 `f8161bc58b`）。04 的两份旧候选基线 patch 保留为历史附件。

| 编号 | 分析与补丁 | 当前状态 |
| --- | --- | --- |
| 03 | [LSP 补全响应 ID 与请求不一致](03-lsp-response-id/README.md) | 预期 ID 已改为 135，与请求及捕获响应一致，ID 比较通过。 |
| 04 | [LSP 回归测试：最新补丁与撤回记录](04-lsp-api-baselines/README.md) | tools 单元测试 103/103、特性用例 26/26 通过；旧 HLT 候选基线保持撤回。 |

`completion_007` 的捕获响应回放仍有候选列表差异（预期 109 项、捕获 99 项），并非 ID 差异。旧版 HLT 7/7 回放仅作历史记录。
