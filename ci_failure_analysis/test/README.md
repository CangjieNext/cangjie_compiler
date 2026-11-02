# test

[返回总览](../README.md)

本类包含测试材料冲突、过期断言、编译命令和压力用例构造问题。11、12 涉及 Sema 用例，02 涉及 std.ast，16 涉及 runtime；这些修复均只调整测试。CHIR API/IR 专项用例归 [chir](../chir/README.md)。

| 编号 | 分析与补丁 | 分类依据 |
| --- | --- | --- |
| 01 | [01 测试残留三方冲突与内部 fileID 断言](01-test-conflict-markers/README.md) | 测试源码残留冲突标记以及内部 fileID 断言不稳定，修复仅涉及测试材料。 |
| 02 | [02 AST 片段解析异常的输出契约](02-ast-fragment-output/README.md) | std.ast 当前异常契约正确，修复的是过期的输出副作用断言。 |
| 11 | [11 负例 SCAN-IN 诊断基线与库源码行号更新](11-diagnostic-baselines/README.md) | 三个负例的语义检查已经正确报错，仅更新过期的诊断签名、格式及库源码位置断言。 |
| 12 | [12 数组负例的冲突残留与文本 SCAN 基线](12-json-diagnostic-driver/README.md) | 清理合并冲突，保留 cjc-frontend，使用完整文本 SCAN 及最新库行号。 |
| 16 | [16 栈溢出测试递归帧过小导致扫描超时](16-stackoverflow-handler/README.md) | 极小递归帧使压力用例在到达目标路径前耗尽时间，修复测试构造而非 runtime 实现。 |
