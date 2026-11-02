# 03 LSP 补全响应 ID 与请求不一致

补丁编号：`patch 03`。主分类：`lsp`；修改仓库：`cangjie_test`。

[返回总览](../../README.md) · [分类目录](../README.md)

## 当前处理状态

按最新要求，已将 `completion_007` 的预期响应 ID 从 1488 改为 135，与请求及日志捕获的实际响应一致。修复提交 `f8161bc58bc0b1fa1277d715b592d8b1ea8b1fb0`，标题 `[patch 03] test: match completion response to its request ID`。

此前切换 tools 最新补丁时曾撤回本项；本次只恢复 ID 修复。tools 继续使用 `pr1361-lsp-tests.patch`，04 的旧 HLT 候选基线修改仍保持撤回。

## 日志证据与根因

[`j_SaXp48Wm.txt:13715`](../../../j_SaXp48Wm.txt#L13715)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_007.info`。

请求和实际响应的 ID 均为 135，旧预期却写成 1488。测试框架按预期 ID 查找响应，导致找不到已经收到的补全响应。这与候选 API 基线差异是两个独立问题。

## 修改方案

仅修改 [textDocument_completion_007.info](../../../../cangjie_test/testsuites/HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_007.info) 的 `Rev#` JSON 顶层 `id`，从 1488 改为 135。

已对照父提交核对请求区字节及其余预期字段完全一致，JSON 可正常解析。

## 验证结果

- 请求 ID、预期响应 ID、日志捕获响应 ID 均为 **135**。
- 使用原 `lsp_test.py` 的 `split_response_list` 查找响应成功，`json_compare` 对 ID 的比较通过。
- 使用同一比较器回放完整捕获响应，剩余差异只有 `result`：预期 109 项、捕获 99 项。因此本次修复了响应 ID 匹配，不将其记作整个 HLT 用例通过。候选差异归入[第 04 项](../04-lsp-api-baselines/README.md)。

本次没有启动 LSP 服务重新运行完整 HLT 用例。验证记录：`/tmp/manifest-lsp-id-restored.json`；捕获响应：`/tmp/manifest-lsp-completion007-actual.json`。此前更新候选基线后的 7/7 回放是历史结果。

## 当前提交与补丁

| 仓库 | 修复提交 | 父提交 | 补丁 |
| --- | --- | --- | --- |
| `cangjie_test` | `f8161bc58bc0b1fa1277d715b592d8b1ea8b1fb0` | `4a2398c8f291bf85e87d84f525f737435655305c` | [cangjie_test.patch](cangjie_test.patch) |

补丁已按本次提交重新导出。当前源码已包含该 ID 修复，无需重复应用；兼容基线可执行：

```bash
git -C cangjie_test am ../cangjie_compiler/ci_failure_analysis/lsp/03-lsp-response-id/cangjie_test.patch
```

历史原提交：`ab8934ea3f46bc74c56914f1f9ecef128a623d3a`；撤回提交：`4a2398c8f291bf85e87d84f525f737435655305c`。本次新提交仅恢复相同的 ID 修改，未推送。
