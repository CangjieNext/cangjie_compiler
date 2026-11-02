# compiler O2 后冒烟用例分析

输入：[后冒烟失败用例.txt](../后冒烟失败用例.txt) 的 **compiler O2** 部分，共 **26 个原用例**。本目录独立于 `ci_failure_analysis`，仅保存本批 compiler O2 的分析和补丁。

## 结论

| 项目 | 结果 | 分析与补丁 |
| --- | --- | --- |
| 1 个可复现问题 | 已修复，分类 test，编号 patch 22 | [Range 默认实现继承冲突](test/22-range-redundant-equatable/README.md) · [cangjie_test.patch](test/22-range-redundant-equatable/cangjie_test.patch) |
| 25 个原失败路径 | 修复前后均带 -O2 验证通过，原失败未复现 | [逐项验证记录](test/verified-negative-cases/README.md) |

本次已确认的修复属于 **test**：用例重复继承 Comparable 及其父接口 Equatable，导致新增 local-mode 默认成员冲突。删除两处冗余声明，保留 Equatable 的传递继承关系及原 Range 测试内容。compiler 继承检查规则保持原样。

本批没有其他已确认的修复根因。用户未提供原 CI 日志，25 个当前通过的诊断负例只记录验证结果，不推测它们当时的实际失败原因。

## O2 验证证据

- 原 26 项：修复前 **25 PASS / 1 FAIL**；修复后 **26/26 PASS**。
- 额外的现有菱形默认实现冲突负例通过，合计 **27/27 PASS**。
- 审计了 **28 条 cjc 编译调用**，全部显式包含 `-O2`；宏依赖编译也包含 `-O2`。
- `immutable_range_02` 编译及运行均成功；其余 25 个原用例源码未改。
- SDK 编译器已还原为与复现前完全相同的二进制，编译器实现没有本批代码修改。
- 平台：Linux x86_64；未执行其他平台或完整 CI。

[命令、返回码及结果审计](validation_commands.json)；[原 26 项清单](cases.txt)；[额外控制负例](control_cases.txt)。

## 复现

从工作区的 `cangjie_test` 目录运行：

```bash
source ../cangjie_compiler/output/envsetup.sh
python3 ../cangjie_test_framework/main.py \
  --test_cfg=testsuites/HLT/configs/cjnative/linux_x64-linux_x64/O2.cfg \
  --test_list=../cangjie_compiler/compiler_o2_smoke_analysis/cases.txt \
  -C 'compiler=cjc -O2' \
  --temp_dir=/tmp/compiler-o2-smoke-repro/run \
  --log_dir=/tmp/compiler-o2-smoke-repro/log \
  --json_output=/tmp/compiler-o2-smoke-repro/results.json \
  --timeout=90 --fail_exit -j6
```

验证额外控制负例时，将 `--test_list` 指向本目录 `control_cases.txt`，仍使用上述 O2 配置与 compiler 覆盖参数。

## 文件与提交

- `test/22-range-redundant-equatable/`：根因、修改方案、验证结果和独立 patch。
- `test/verified-negative-cases/`：其余 25 项的预期诊断与实际通过记录。
- 每仓保留一个汇总提交；test 当前汇总为 `ef626381abfca8532b55cd409b7bee78f7f7130e`。独立导出提交及父提交见第 22 项报告。
- 输入报告 SHA-256：`eb02211d50ce6756c65586f6ee40d774a945fb55fc2247d2e1e62106f709441a`。原报告未改动，cjdb/cjpm/lsp/spec 部分未纳入本次修复。
- 本次未推送。
