# codegen

[返回总览](../README.md)

本类包含 LLVM 后端的调用属性和 GC statepoint 修复。15 虽在 runtime 用例中崩溃，实际修复位于 LLVM 后端。

| 编号 | 分析与补丁 | 分类依据 |
| --- | --- | --- |
| 15 | [15 local finalizer 提前执行被错误标成 GC leaf](15-finalizer-gc-roots/README.md) | 修复 LLVM 后端 pass 对 finalizer 调用的 GC 属性，使 statepoint 保护存活引用。 |
