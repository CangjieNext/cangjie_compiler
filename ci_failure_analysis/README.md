# 四份构建失败日志：分析与当前补丁索引

四份日志去重后共 **54 个用例、16 类根因**。同一 `completion_007` 同时涉及 03 和 04 两类问题。

当前源码保留 **11 项本地修复、11 个当前修复提交**。目录中共 **18 个 patch 文件**：11 个对应当前修复，另外 7 个为历史参考（stdlib 05—09 五个，以及 LSP 04 的两个旧补丁）。LSP 04 改用用户提供的 tools 最新补丁，03 已单独恢复预期 ID 135；旧 HLT 候选基线仍保持撤回。stdlib 六项问题的原失败用例此前已验证 **19/19 通过**。

所有本地修复提交标题带 `[patch NN]`；覆盖旧提交的 11、12、14、15 仍各自只有一个修复提交。上游已有提交沿用远端历史，不改写其提交信息。

## 分类

分类按根因和修复内容确定，不等同于仓库名。stdlib 包含 std 和 stdx，codegen 包含 LLVM。七类以外的问题：无。

| 分类 | 问题数 | 当前状态 |
| --- | ---: | --- |
| [sema](sema/README.md) | 0 | 相关诊断基线属于 test。 |
| [chir](chir/README.md) | 2 | 10、14 为专项测试适配。 |
| [codegen](codegen/README.md) | 1 | 15 已替换为用户指定 LLVM 补丁并验证。 |
| [runtime](runtime/README.md) | 0 | GC 根因在 LLVM；压力用例构造属于 test。 |
| [stdlib](stdlib/README.md) | 6 | 05—09 使用上游修复，13 已合入本地；19/19 通过。 |
| [lsp](lsp/README.md) | 2 | 03 已恢复 ID 修复；04 使用 tools 最新补丁，HLT 候选仍有差异。 |
| [test](test/README.md) | 5 | 01、02、11、12、16；11/12 恢复完整 SCAN。 |

## 日志覆盖

| 日志 | 去重失败数 | 问题编号 |
| --- | ---: | --- |
| `j_0KkvPtXM.txt` | 2 | 02 |
| `j_SaXp48Wm.txt` | 7 | 03、04 |
| `j_ltC6grLp.txt` | 3 | 04 |
| `j_p75yFw7h.txt` | 42 | 01、05—16 |

## 根因、状态与提交

| 编号 | 分类 | 分析与方案 | 用例数 | 当前状态与提交 |
| --- | --- | --- | ---: | --- |
| 01 | test | [测试残留三方冲突与内部 fileID 断言](test/01-test-conflict-markers/README.md) | 15 | 本地修复<br>cangjie_test `94a3f6ed45` |
| 02 | test | [AST 片段解析异常的输出契约](test/02-ast-fragment-output/README.md) | 2 | 本地修复<br>cangjie_test `0f1522aa18` |
| 03 | lsp | [LSP 补全响应 ID 与请求不一致](lsp/03-lsp-response-id/README.md) | 1 | ID 修复，验证通过<br>cangjie_test `f8161bc58b`；候选差异归 04 |
| 04 | lsp | [LSP 回归测试：最新补丁与撤回记录](lsp/04-lsp-api-baselines/README.md) | 10 | 改用 tools 最新补丁<br>cangjie_tools `b3a167b7c4`；未覆盖 test HLT |
| 05 | stdlib | [普通 UTF-8 校验遗漏新诊断](stdlib/05-utf8-messages/README.md) | 4 | 上游修复，验证通过<br>cangjie_runtime `59b8426871` |
| 06 | stdlib | [Rune UTF-8 解码遗漏负索引检查](stdlib/06-rune-negative-index/README.md) | 1 | 上游修复，验证通过<br>cangjie_runtime `59b8426871` |
| 07 | stdlib | [ArrayList 闭区间切片越过逻辑长度](stdlib/07-arraylist-closed-range/README.md) | 1 | 上游修复，验证通过<br>cangjie_runtime `59b8426871` |
| 08 | stdlib | [JsonArray 缺少 local 下标重载](stdlib/08-json-local-index/README.md) | 11 | 上游修复，验证通过<br>cangjie_stdx `fcfc21ffff` |
| 09 | stdlib | [local JSON 浮点序列化遗漏非有限值检查](stdlib/09-json-nonfinite/README.md) | 1 | 上游修复，验证通过<br>cangjie_stdx `e39f677e2e` |
| 10 | chir | [CHIR Lambda 默认参数归属 API 迁移](chir/10-chir-default-parameter-api/README.md) | 1 | 本地修复<br>cangjie_test `1c8cc4fa92` |
| 11 | test | [负例 SCAN-IN 诊断基线与库源码行号更新](test/11-diagnostic-baselines/README.md) | 3 | 本地修复<br>cangjie_test `c79c9c1bc4` |
| 12 | test | [数组负例的冲突残留与文本 SCAN 基线](test/12-json-diagnostic-driver/README.md) | 1 | 本地修复<br>cangjie_test `0ef5c93c24` |
| 13 | stdlib | [std.ast 词法选项迁移遗漏 effect handler 开关](stdlib/13-ast-eh-lexer/README.md) | 1 | 本地修复，验证通过<br>cangjie_runtime `2ddd31937e` |
| 14 | chir | [Nothing 清理测试检查的 dump 阶段过早](chir/14-nothing-region-cleanup/README.md) | 1 | 本地修复<br>cangjie_test `cd1e10565d` |
| 15 | codegen | [local finalizer 提前执行被错误标成 GC leaf](codegen/15-finalizer-gc-roots/README.md) | 1 | 本地修复<br>llvm-project `73a56bb60f` |
| 16 | test | [栈溢出测试递归帧过小导致扫描超时](test/16-stackoverflow-handler/README.md) | 1 | 本地修复<br>cangjie_test `b5c5c05a5e` |

## 本次验证

- runtime 当前为 `2ddd31937eb4823e6d0cc6cec0bcb564cbb4c0a5`，在上游 `59b84268718a52946f0c965fb7cc56a45947cfda` 上仅加回第 13 项；stdx 继续使用上游 `e39f677e2e09de00557e3f528b5e9e2eb7c3f6b8`。
- 上次完整重建标准库 203 步、stdx 135 步后为 23/24 通过，唯一失败是 13。本次加回 enableEH 转发，标准库增量构建 154 步并安装 SDK，已核对 astFFI 静态库及共享库与新构建产物一致。
- 此前合入第 13 项时，LLT **27/27 通过**：之前的 24 例全部通过，另检查了现有 EH 关闭负例和两个词法异常用例。其中 stdlib 的原失败用例 **19/19**；patch 11/12 四个 SCAN 用例 **4/4**；patch 15 原 finalizer 用例也通过。
- 第 13 项 `test_cangjieLex_eh.cj` 已通过，`handle`、`perform`、`resume` 不再因遗漏 enableEH 而降为普通标识符。同目录四个有效用例 **4/4** 通过。
- 指定 LLVM 补丁另有此前独立验证：原例默认重复 **30/30**、`-O2` 重复 **10/10**；原例及相邻 finalizer 用例 **8/8**；LLVM 现有 lit **3/3** 通过。
- LSP 本次已构建新的 LSPServer 和 GTest，相关单元测试 **103/103**、特性用例 **26/26** 通过。旧 HLT 候选基线仍保持撤回，之前的 7/7 回放结果只作历史记录；03 已单独恢复预期 ID 135。捕获响应回放仍有 `result` 差异（109 项预期、99 项捕获）。特性用例结果见第 04 项报告。
- 所有执行验证来自 Linux x86_64，macOS 尚未验证。本次没有推送。

最近一次 LLT 结果：`/tmp/manifest-patch13-applied-results.json`；日志：`/tmp/manifest-patch13-applied-tests.log`。LSP 本次结果：`/tmp/manifest-lsp-supplied-units.json`、`/tmp/manifest-lsp-supplied-features.json`。修复前记录保留在 `/tmp/manifest-upstream-validation-results.json`。各报告保留独立验证记录。

## 补丁使用

本目录保留 18 个导出 patch。stdlib 05—09 的五个参考文件及 LSP 04 的两个旧补丁仅作历史附件；其余 11 个对应当前修复。LSP 当前使用 03 的 `cangjie_test.patch` 和 04 的 `cangjie_tools-pr1361.patch`。

先同步上述 runtime、stdx 远端版本，再按问题编号应用当前补丁。03 应用 ID 修复，04 仅应用 tools 最新文件；其他问题按各报告中的父提交和 `git am` 命令操作。当前工作区已经包含当前修复，无需重复应用。

第 11 项使用 SCAN-IN 更新完整诊断与新行号；第 12 项保留 cjc-frontend 并使用文本 SCAN；第 14 项只改 dump 文件名为 UnreachableBlockElimination；第 15 项代码与用户给定的 `0001-fix-remote-gc-leaf-function-for-finaluzer.patch` 完全一致。

## 本地复现

从工作区根目录 `source cangjie_compiler/output/envsetup.sh`，设置 `CANGJIE_STDX_PATH` 为 `cangjie_stdx/target/linux_x86_64_cjnative/dynamic/stdx`。使用原 LLT 框架和 cjnative 配置，按各报告列出的用例定向运行。
