# 04 LSP 回归测试：改用用户提供的最新补丁

补丁编号：`patch 04`。

## 分类

主分类：`lsp`。当前修复只涉及 tools 仓的 LSP 测试设施、测试数据和精确预期结果。

[返回总览](../../README.md) · [分类目录](../README.md)

## 当前处理状态

此前已撤回三份 LSP 修改（03 的 test 补丁、04 的 test/tools 补丁），使用用户放入 tools 仓的 `pr1361-lsp-tests.patch`。随后按最新要求，03 单独恢复预期 ID 135；04 的旧 HLT 候选基线修改仍保持撤回。

最新 tools 提交：`b3a167b7c408736d0d56ef8e179454a3c2eaeb1c`，标题 `[patch 04] fix: stabilize LSP regression tests for local mode SDK`；作者沿用 `yangwei327 <yangwei327@huawei.com>`。

原始补丁提交：`73f0de43dd9c3d29cb78a8621360509118bf2585`。输入文件 SHA-256：`b9e88f507706a95871f8cbc6b10591bf7c4cc7351597542e2fe5e1e44c1361b1`。

已核对应用后的 Git patch-id 与输入补丁一致（`ee4862b7eed876f7dec3e99afb5b502d5f5b4872`），未改动提供的代码 diff。tools 中独立的 CODEOWNERS 和超时设置修改保留。

## 影响与原日志证据

- [`j_SaXp48Wm.txt:13686`](../../../j_SaXp48Wm.txt#L13686)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_009.info`
- [`j_SaXp48Wm.txt:13715`](../../../j_SaXp48Wm.txt#L13715)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_007.info`
- [`j_SaXp48Wm.txt:13744`](../../../j_SaXp48Wm.txt#L13744)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_090.info`
- [`j_SaXp48Wm.txt:13772`](../../../j_SaXp48Wm.txt#L13772)：`HLT/Tools/cjlsp/testcases/autotestcase/completion/textDocument_completion_087.info`
- [`j_SaXp48Wm.txt:13804`](../../../j_SaXp48Wm.txt#L13804)：`HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_001.info`
- [`j_SaXp48Wm.txt:13837`](../../../j_SaXp48Wm.txt#L13837)：`HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_012.info`
- [`j_SaXp48Wm.txt:13870`](../../../j_SaXp48Wm.txt#L13870)：`HLT/Tools/cjlsp/testcases/autotestcase/deprecated/textDocument_completion_011.info`
- [`j_ltC6grLp.txt:5456`](../../../j_ltC6grLp.txt#L5456)：`LSP_Completion_Base_001`
- [`j_ltC6grLp.txt:7271`](../../../j_ltC6grLp.txt#L7271)：`LSP_Completion_Atomic_001`
- [`j_ltC6grLp.txt:8429`](../../../j_ltC6grLp.txt#L8429)：`LSP_Completion_2022041402911`

去重后涉及 10 个用例。日志中的同一用例重试仅计算一次。


## 根因与最新修改方案

原补全失败来自 local mode 标准库公共 API 变化：异常名称、构造函数、deprecated 属性和补全候选集合与旧基线不一致。此前的方案分别修改了 test 的 HLT 预期和 tools 的三个基线。

最新补丁覆盖 tools 仓 39 个文件，包含以下调整：

- 同步补全、语义高亮等精确基线，修正 Range 测试源码及请求文本。
- `ReadExpectedResult` 使用逐行读取和不抛异常的 JSON 解析；文件缺失、空文件或损坏记录通过 GTest 记录失败，并增加五个解析回归测试。
- 隔离 SyscapCheck、选项解析和编译项目测试的全局状态、回调及生命周期，补充有效断言。
- 增加 deprecated 构造函数和泛型静态实现的补全契约用例，并补回 `ExtractInterface_Apply_020` 的注册。

## 撤回记录

| 仓库 | 撤回内容 | 撤回提交 |
| --- | --- | --- |
| `cangjie_tools` | 04 的三个旧补全基线 | `c55aeab9f0fdb5fa3441a075767aef4b9e46351c` |
| `cangjie_test` | 04 的七个 HLT 基线 | `36c51da58e8860a01b254c52c7ed9d4d2d0260fe` |
| `cangjie_test` | 03 的 HLT 响应 ID | `4a2398c8f291bf85e87d84f525f737435655305c` |

采用 revert 提交保留历史，没有重写其他修复提交。撤回时已核对 test 仓七个相关 HLT 文件与原 `ba871b8d23` 基线一致；随后按最新要求仅恢复 03 的预期 ID 135，候选基线保持原样。

**覆盖范围：** 最新 tools 补丁没有修改 test 仓 HLT 数据。03 已通过 `f8161bc58b` 单独恢复预期 ID 135，请求与响应 ID 现已一致。对日志捕获响应的完整回放仍有候选列表差异：预期 109 项、捕获 99 项；旧 HLT 候选基线未重新加入，不能将此前 HLT 7/7 回放结果计入当前版本。

## 验证结果

Linux x86_64：以 `ENABLE_TEST=ON`、`NO_EXCEPTIONS=ON`、`-fno-exceptions` 构建 GTest 和 LSPServer 均成功。已同步当前 SDK 头文件与 LSP 库。

相关单元测试 **103/103 通过**：`ExpectedResultTest`、`SyscapCheckTest`、`CompilerCangjieProjectTest`，以及受影响的五个 `UtilsTest`。

补丁涉及的 25 个补全和语义高亮用例全部通过，补回注册的 `ExtractInterface_Apply_020` 也通过：**26/26**。其中原失败的三个 tools 用例、新增的两个补全契约用例均通过。合计本次定向验证 **129/129** 通过。

本次未执行完整 LSP 测试流水线或 macOS 构建。

记录：`/tmp/manifest-lsp-supplied-configure.log`、`/tmp/manifest-lsp-supplied-build.log`、`/tmp/manifest-lsp-supplied-server-build.log`；单元测试结果 `/tmp/manifest-lsp-supplied-units.json`；特性结果 `/tmp/manifest-lsp-supplied-features.json`。

## 当前补丁与历史附件

| 用途 | 仓库 | 文件 | 基于的父提交 |
| --- | --- | --- | --- |
| 当前修复 | `cangjie_tools` | [cangjie_tools-pr1361.patch](cangjie_tools-pr1361.patch) | `c55aeab9f0fdb5fa3441a075767aef4b9e46351c` |
| 已撤回的历史参考 | `cangjie_test` | [cangjie_test.patch](cangjie_test.patch) | `94a3f6ed45782c08988b651fefe1deca1e4443ff` |
| 已撤回的历史参考 | `cangjie_tools` | [cangjie_tools.patch](cangjie_tools.patch) | `30436ffa22e60e02ff325a2afaf94f419533aa5f` |

当前 tools 补丁从 `b3a167b7c408736d0d56ef8e179454a3c2eaeb1c` 导出，保留编号和原作者。04 的两份旧导出文件保留为历史附件；03 的 ID 修复已恢复，其补丁在对应目录按最新提交重新导出。

在兼容基线的 tools 仓中应用当前导出文件即可：

```bash
git -C cangjie_tools am ../cangjie_compiler/ci_failure_analysis/lsp/04-lsp-api-baselines/cangjie_tools-pr1361.patch
```

当前工作区已经包含新补丁，无需重复应用。本次尚未推送。
