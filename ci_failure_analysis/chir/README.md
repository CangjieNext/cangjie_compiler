# chir

[返回总览](../README.md)

本类包含 CHIR 默认参数归属 API 和 Nothing 消除阶段 IR 断言的专项用例适配。两个补丁均修改 test 仓，报告保留这一事实。

| 编号 | 分析与补丁 | 分类依据 |
| --- | --- | --- |
| 10 | [10 CHIR Lambda 默认参数归属 API 迁移](10-chir-default-parameter-api/README.md) | 属于 CHIR 默认参数归属 API 的专项用例适配，修改位于 test 仓。 |
| 14 | [14 Nothing 清理测试检查的 dump 阶段过早](14-nothing-region-cleanup/README.md) | 在不可达块清理后的 dump 上检查整函数区域配对，修改位于 test 仓。 |
