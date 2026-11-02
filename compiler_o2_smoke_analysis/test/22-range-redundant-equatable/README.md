# 22 Range 用例冗余继承 Equatable 导致默认实现冲突

补丁编号：`patch 22`。分类：`test`。修改仓库：`cangjie_test`。

[总索引](../../README.md) · [分类索引](../README.md)

## 影响范围

[immutable_range_02.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/TypesAndVariables/immutable/immutable_range_02.cj)，来自 compiler 仓 `后冒烟失败用例.txt` 的 compiler O2 部分。

复现命令显式包含 `-O2`：

```bash
cjc -O2 --error-count-limit=all --diagnostic-format=json -O2 --dy-std immutable_range_02.cj -o immutable_range_02.out
```

## 失败原因

原来 `extend Aoo` 与 `extend Coo` 都声明 `Countable<T> & Comparable<T> & Equatable<T>`。

当前 [Comparable<T>](../../../../cangjie_runtime/stdlib/libs/std/core/comparable.cj) 已继承 [Equatable<T>](../../../../cangjie_runtime/stdlib/libs/std/core/equatable.cj)。显式重复列出 Equatable，会让默认比较运算符沿多条接口路径继承。local-mode 接口新增了接收者模式相关的默认成员，而用例只提供原来的 heap 运算符实现，不能消除这些成员的默认实现冲突。

原编译器在 Sema 阶段报出两条 `sema_interface_member_must_be_implemented`，分别指向 Aoo 和 Coo，消息为：

```text
interface function '!=(this @local!, other: Generics-T): Bool' must be implemented in 'extend Aoo'
interface function '!=(this @local!, other: Generics-T): Bool' must be implemented in 'extend Coo'
```

项目已有的[菱形默认实现冲突负例](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Chir/dispatch/negative/NEG_diamond_default_conflict/main.cj) 明确要求这类继承冲突被拒绝。因此保持编译器规则，修正此 Range 测试的冗余继承声明。本问题发生在 Sema 阶段；本次复现和验证仍全程传入 `-O2`。

## 修改方案

两处扩展分别改为：

```cangjie
extend Aoo <: Countable<Aoo> & Comparable<Aoo> { ... }
extend Coo <: Countable<Coo> & Comparable<Coo> { ... }
```

仅删除两处冗余的 `& Equatable<...>`。Equatable 约束通过 Comparable 的继承关系继续满足，原有 Range 构造、class/struct 字段和全部运算符实现保留。

## 验证结果

- 原编译器二进制保持与复现前完全一致，SHA-256：`b49e9dee18c0eeff37c51f28b6725b1c033e5e66ccb77b386d003e2aa92f26b1`。
- 26 个原用例：修复前 **25 PASS / 1 FAIL**；修复后显式带 `-O2` 验证 **26/26 PASS**。
- 原 `immutable_range_02.cj` 的 `-O2` 编译与程序运行均返回 0。
- 额外运行现有菱形默认实现负例，`-O2` 下仍按预期报错且比较通过；合计 **27/27 PASS**。
- 审计全部 **28 条 cjc 调用**，包括宏依赖编译，均显式包含 `-O2`，没有 `-O0/-O1`。详见 [validation_commands.json](../../validation_commands.json)。

没有更改 compiler 实现或其他 25 个原用例。验证平台为 Linux x86_64。

## 补丁与提交

| 仓库 | 独立补丁 | 导出提交 | 父提交 | 当前汇总提交 |
| --- | --- | --- | --- | --- |
| cangjie_test | [cangjie_test.patch](cangjie_test.patch) | `cf04a2b8038b05f9f0acd0465b09c2a0688dbe8d` | `d4c76b065d53bcf54fef671e9b85aae71d46c2fd` | `ef626381abfca8532b55cd409b7bee78f7f7130e` |

独立补丁只含本问题的两行修改。当前工作区已包含修复，向兼容基线移植可在 test 仓执行 `git am /path/to/cangjie_test.patch`。汇总前提交保留在备份引用 `refs/codex-backups/o2-smoke-20261105-0nyvp2hw-fixes`。

详细运行记录：`/tmp/manifest-o2-smoke-baseline-results.json`、`/tmp/manifest-o2-smoke-verified-results.json`、`/tmp/manifest-o2-smoke-verified.log`。
