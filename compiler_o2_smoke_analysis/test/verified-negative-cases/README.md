# 当前通过、未复现原失败的 25 个诊断负例

[总索引](../../README.md) · [分类索引](../README.md)

这 25 个用例在修复前就已通过，修复后再次带 `-O2` 验证也全部通过。用户提供的是失败路径清单，没有原始 CI 失败输出，因此无法确认它们当时失败的具体原因，不能将本次结果写成已经定位并修复了 25 个独立问题。

它们都是诊断负例：应按 `EXEC-PIPE-1`、`ERRCHECK` 或诊断比较管道的约定判断，通过预期诊断检查才是成功。不能只根据 cjc 返回非零就认定用例失败。宏用例还包含一次应当成功的宏依赖编译。

这些用例源码保持原样，本批实际代码补丁只对应[第 22 项](../22-range-redundant-equatable/README.md)。所有编译命令见 [validation_commands.json](../../validation_commands.json)。

| 原用例 | 预期诊断标识 | 修复前 -O2 | 修复后 -O2 |
| --- | --- | --- | --- |
| [Expressions/AssignmentExpressions/assign005/assign50001.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/AssignmentExpressions/assign005/assign50001.cj) | `parse_expected_expression` | PASS | PASS |
| [Expressions/AssignmentExpressions/assign005/assign50003.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/AssignmentExpressions/assign005/assign50003.cj) | `parse_expected_expression` | PASS | PASS |
| [Expressions/AssignmentExpressions/assign005/assign50004.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/AssignmentExpressions/assign005/assign50004.cj) | `parse_expected_expression` | PASS | PASS |
| [Expressions/ConditionalExpressions/IfExpression/if002/if00215.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/ConditionalExpressions/IfExpression/if002/if00215.cj) | `parse_expected_expression` | PASS | PASS |
| [Expressions/FlowExpression/support_type/007/flow_type_70001.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/FlowExpression/support_type/007/flow_type_70001.cj) | `parse_expected_character`、`parse_expected_expression` | PASS | PASS |
| [Expressions/PatternMatching/MatchExpression/match002/match00256.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/PatternMatching/MatchExpression/match002/match00256.cj) | `parse_expected_expression` | PASS | PASS |
| [Expressions/Relational_Operators/Relate_bitwise/bitwise002/bitwise00217.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/Relational_Operators/Relate_bitwise/bitwise002/bitwise00217.cj) | `parse_expected_expression`、`parse_expected_right_delimiter` | PASS | PASS |
| [Expressions/Relational_Operators/Relate_logical/logical002/logical00211.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/Relational_Operators/Relate_logical/logical002/logical00211.cj) | `parse_expected_expression`、`parse_expected_right_delimiter` | PASS | PASS |
| [Expressions/Relational_Operators/Relate_logical/logical004/logical00411.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Expressions/Relational_Operators/Relate_logical/logical004/logical00411.cj) | `parse_expected_expression`、`parse_expected_right_delimiter` | PASS | PASS |
| [Extend/interface/direct_generic_enum_007.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Extend/interface/direct_generic_enum_007.cj) | `sema_interface_is_not_extendable`、`sema_prop_redefinition`、`sema_static_variable_use_generic_parameter` | PASS | PASS |
| [Extend/interface/direct_generic_enum_00701.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Extend/interface/direct_generic_enum_00701.cj) | `sema_interface_is_not_extendable`、`sema_prop_redefinition`、`sema_static_variable_use_generic_parameter` | PASS | PASS |
| [Extend/property/extend_prop_shadow_11_generic.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Extend/property/extend_prop_shadow_11_generic.cj) | `sema_class_need_abstract_modifier_or_func_need_impl`、`sema_prop_redefinition` | PASS | PASS |
| [FFI/cffi/c_ffi_011/c_ffi_01182/c_ffi.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/FFI/cffi/c_ffi_011/c_ffi_01182/c_ffi.cj) | `sema_undeclared_type_name` | PASS | PASS |
| [Generics/where/where_type_08.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Generics/where/where_type_08.cj) | `parse_expected_expression` | PASS | PASS |
| [MetaProgramming/Macro/Attribute/Invoke/005/macro_attri_invoke_50100.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/MetaProgramming/Macro/Attribute/Invoke/005/macro_attri_invoke_50100.cj) | `parse_expected_expression` | PASS | PASS |
| [Property/abstract/property_abstract_class_14.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Property/abstract/property_abstract_class_14.cj) | `sema_class_need_abstract_modifier_or_func_need_impl`、`sema_invalid_member_visibility_in_class`、`sema_prop_redefinition` | PASS | PASS |
| [Property/abstract/property_abstract_class_15.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Property/abstract/property_abstract_class_15.cj) | `sema_invalid_member_visibility_in_class`、`sema_inherit_member_type_inconsistent` | PASS | PASS |
| [Property/abstract/property_abstract_interface_09.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Property/abstract/property_abstract_interface_09.cj) | `sema_inherit_member_type_inconsistent`、`sema_prop_redefinition` | PASS | PASS |
| [Property/property_def_0203.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Property/property_def_0203.cj) | `sema_property_have_same_declaration_in_inherit_immut`、`sema_property_have_same_declaration_in_inherit_mut`、`sema_inherit_member_type_inconsistent`、`sema_prop_redefinition` | PASS | PASS |
| [Property/property_modifier_letvartype02.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/Property/property_modifier_letvartype02.cj) | `sema_inherit_member_type_inconsistent`、`sema_missing_overridden_func`、`sema_property_have_same_declaration_in_inherit_immut`、`sema_prop_redefinition` | PASS | PASS |
| [TypesAndVariables/CompositeType/NewEnum/Member/Prop/001/enum_prop_10004.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/TypesAndVariables/CompositeType/NewEnum/Member/Prop/001/enum_prop_10004.cj) | `sema_immutable_property_with_setter`、`sema_cannot_assign_to_immutable` | PASS | PASS |
| [TypesAndVariables/CompositeType/NewEnum/Option/005/enum_option_50050.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/TypesAndVariables/CompositeType/NewEnum/Option/005/enum_option_50050.cj) | `parse_expected_expression` | PASS | PASS |
| [TypesAndVariables/TypeConversion/PrimitiveTypeConversion/conversion007/conversion00702.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/TypesAndVariables/TypeConversion/PrimitiveTypeConversion/conversion007/conversion00702.cj) | `parse_expected_expression` | PASS | PASS |
| [TypesAndVariables/TypeConversion/PrimitiveTypeConversion/conversion007/conversion00703.cj](../../../../cangjie_test/testsuites/HLT/compiler/cjnative/TypesAndVariables/TypeConversion/PrimitiveTypeConversion/conversion007/conversion00703.cj) | `parse_expected_expression` | PASS | PASS |
| [regression/testcase_645.cj](../../../../cangjie_test/testsuites/HLT/regression/testcase_645.cj) | `parse_expected_expression`、`parse_expected_right_delimiter` | PASS | PASS |

本批没有这些项目的代码修改，所以无需为其生成 patch；本页保留其覆盖情况和验证证据。
