// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.
// ASTKIND(ast kind, ast value for human reading, ast node, ast size for notify checking related code)
#ifdef ASTKIND
ASTKIND(ANNOTATION, "annotation", Annotation, 528)
ASTKIND(MODIFIER, "modifier", Modifier, 280)
ASTKIND(DECL, "*decl", Decl, 744) // begin of subtypes of Decl.
ASTKIND(MAIN_DECL, "main_decl", MainDecl, 768)
ASTKIND(FUNC_DECL, "func_decl", FuncDecl, 888)
ASTKIND(MACRO_DECL, "macro_decl", MacroDecl, 760)
ASTKIND(CLASS_LIKE_DECL, "class_like_decl", ClassLikeDecl, 840) // begin: subtypes of ClassLikeDecl and InheritableDecl.
ASTKIND(CLASS_DECL, "class_decl", ClassDecl, 856)
ASTKIND(INTERFACE_DECL, "interface_decl", InterfaceDecl, 848) // end: subtypes of ClassLikeDecl.
ASTKIND(EXTEND_DECL, "extend_decl", ExtendDecl, 904)
ASTKIND(ENUM_DECL, "enum_decl", EnumDecl, 944)
ASTKIND(STRUCT_DECL, "struct_decl", StructDecl, 808) // end of subtypes of InheritableDecl.
ASTKIND(TYPE_ALIAS_DECL, "type_alias_decl", TypeAliasDecl, 776)
ASTKIND(PRIMARY_CTOR_DECL, "primary_ctor_decl", PrimaryCtorDecl, 760)
ASTKIND(BUILTIN_DECL, "builtin_decl", BuiltInDecl, 776)
ASTKIND(VAR_DECL, "var_decl", VarDecl, 824) // begin of subtypes of VarDecl and VarDeclAbstract.
ASTKIND(PROP_DECL, "prop_decl", PropDecl, 912)
ASTKIND(MACRO_EXPAND_PARAM, "macro_expand_param", MacroExpandParam, 1664)
ASTKIND(THIS_PARAM, "this_param", ThisParam, 904)
ASTKIND(FUNC_PARAM, "func_param", FuncParam, 888)                                // end of subtypes of VarDecl.
ASTKIND(VAR_WITH_PATTERN_DECL, "var_with_pattern_decl", VarWithPatternDecl, 816) // end of subtypes of VarDeclAbstract.
ASTKIND(GENERIC_PARAM_DECL, "generic_param_decl", GenericParamDecl, 768)
ASTKIND(PACKAGE_DECL, "package_decl", PackageDecl, 752)
ASTKIND(MACRO_EXPAND_DECL, "macro_expand_decl", MacroExpandDecl, 1520)
ASTKIND(INVALID_DECL, "invalid_decl", InvalidDecl, 744) // end of subtypes of Decl.
ASTKIND(PATTERN, "*pattern", Pattern, 280)              // begin of subTypes of Pattern.
ASTKIND(VAR_PATTERN, "var_pattern", VarPattern, 296)
ASTKIND(CONST_PATTERN, "const_pattern", ConstPattern, 296)
ASTKIND(TUPLE_PATTERN, "tuple_pattern", TuplePattern, 376)
ASTKIND(ENUM_PATTERN, "enum_pattern", EnumPattern, 384)
ASTKIND(VAR_OR_ENUM_PATTERN, "var_or_enum_pattern", VarOrEnumPattern, 368)
ASTKIND(TYPE_PATTERN, "type_pattern", TypePattern, 344)
ASTKIND(EXCEPT_TYPE_PATTERN, "except_type_pattern", ExceptTypePattern, 376)
ASTKIND(COMMAND_TYPE_PATTERN, "effect_type_pattern", CommandTypePattern, 376)
ASTKIND(WILDCARD_PATTERN, "wildcard_pattern", WildcardPattern, 280)
ASTKIND(INVALID_PATTERN, "invalid_pattern", InvalidPattern, 280) // end of subtypes of Pattern.
ASTKIND(TYPE, "*type", Type, 448)                                // begin of subtypes of Type.
ASTKIND(REF_TYPE, "ref_type", RefType, 632)
ASTKIND(QUALIFIED_TYPE, "qualified_type", QualifiedType, 640)
ASTKIND(OPTION_TYPE, "option_type", OptionType, 496)
ASTKIND(CONSTANT_TYPE, "constant_type", ConstantType, 480)
ASTKIND(VARRAY_TYPE, "varray_type", VArrayType, 528)
ASTKIND(PRIMITIVE_TYPE, "primitive_type", PrimitiveType, 488)
ASTKIND(PAREN_TYPE, "paren_type", ParenType, 504)
ASTKIND(FUNC_TYPE, "func_type", FuncType, 552)
ASTKIND(TUPLE_TYPE, "tuple_type", TupleType, 536)
ASTKIND(THIS_TYPE, "this_type", ThisType, 448)
ASTKIND(INVALID_TYPE, "invalid_type", InvalidType, 448) // end of subtypes of Type.
ASTKIND(EXPR, "*expr", Expr, 456)                       // begin of subtypes of Expr.
ASTKIND(WILDCARD_EXPR, "wildcard_expr", WildcardExpr, 456)
ASTKIND(CALL_EXPR, "call_expr", CallExpr, 648)
ASTKIND(PAREN_EXPR, "paren_expr", ParenExpr, 504)
ASTKIND(MEMBER_ACCESS, "member_access_expr", MemberAccess, 792) // begin of subtypes of NameReferenceExpr.
ASTKIND(REF_EXPR, "ref_expr", RefExpr, 712)                     // end of subtypes of NameReferenceExpr.
ASTKIND(OPTIONAL_EXPR, "optional_expr", OptionalExpr, 488)
ASTKIND(OPTIONAL_CHAIN_EXPR, "optional_chain_expr", OptionalChainExpr, 456)
ASTKIND(PRIMITIVE_TYPE_EXPR, "primitive_type_expr", PrimitiveTypeExpr, 504)
ASTKIND(RETURN_EXPR, "return_expr", ReturnExpr, 488)
ASTKIND(LIT_CONST_EXPR, "lit_const_expr", LitConstExpr, 568)
ASTKIND(INTERPOLATION_EXPR, "interpolation_expr", InterpolationExpr, 520)
ASTKIND(STR_INTERPOLATION_EXPR, "str_interpolation_expr", StrInterpolationExpr, 536)
ASTKIND(ASSIGN_EXPR, "assign_expr", AssignExpr, 504) // begin of OverloadableExpr.
ASTKIND(UNARY_EXPR, "unary_expr", UnaryExpr, 488)
ASTKIND(BINARY_EXPR, "binary_expr", BinaryExpr, 488)
ASTKIND(INC_OR_DEC_EXPR, "inc_or_dec_expr", IncOrDecExpr, 488)
ASTKIND(SUBSCRIPT_EXPR, "subscript_expr", SubscriptExpr, 552) // end of OverloadableExpr.
ASTKIND(IS_EXPR, "is_expr", IsExpr, 488)
ASTKIND(AS_EXPR, "as_expr", AsExpr, 488)
ASTKIND(RANGE_EXPR, "range_expr", RangeExpr, 536)
ASTKIND(ARRAY_LIT, "array_lit_expr", ArrayLit, 552)
ASTKIND(ARRAY_EXPR, "array_expr", ArrayExpr, 568)
ASTKIND(POINTER_EXPR, "pointer_expr", PointerExpr, 472)
ASTKIND(TUPLE_LIT, "tuple_lit_expr", TupleLit, 552)
ASTKIND(MATCH_EXPR, "match_expr", MatchExpr, 600)
ASTKIND(BLOCK, "block", Block, 536)
ASTKIND(IF_EXPR, "if_expr", IfExpr, 568)
ASTKIND(LET_PATTERN_DESTRUCTOR, "let_pattern_destructor", LetPatternDestructor, 536)
ASTKIND(TOKEN_PART, "token_part", TokenPart, 472)
ASTKIND(QUOTE_EXPR, "quote_expr", QuoteExpr, 536)
ASTKIND(TRY_EXPR, "try_expr", TryExpr, 776)
ASTKIND(WHILE_EXPR, "while_expr", WhileExpr, 536)
ASTKIND(JUMP_EXPR, "jump_expr", JumpExpr, 472)
ASTKIND(EXCLAVE_EXPR, "exclave_expr", ExclaveExpr, 488)
ASTKIND(LAMBDA_EXPR, "lambda_expr", LambdaExpr, 504)
ASTKIND(TRAIL_CLOSURE_EXPR, "trailing_closure_expr", TrailingClosureExpr, 520)
ASTKIND(FOR_IN_EXPR, "for_in_expr", ForInExpr, 584)
ASTKIND(DO_WHILE_EXPR, "do_while_expr", DoWhileExpr, 552)
ASTKIND(TYPE_CONV_EXPR, "type_conv_expr", TypeConvExpr, 552)
ASTKIND(THROW_EXPR, "throw_expr", ThrowExpr, 488)
ASTKIND(PERFORM_EXPR, "perform_expr", PerformExpr, 488)
ASTKIND(RESUME_EXPR, "resume_expr", ResumeExpr, 552)
ASTKIND(SPAWN_EXPR, "spawn_expr", SpawnExpr, 536)
ASTKIND(SYNCHRONIZED_EXPR, "synchronized_expr", SynchronizedExpr, 536)
ASTKIND(MACRO_EXPAND_EXPR, "macro_expand_expr", MacroExpandExpr, 1368)
ASTKIND(IF_AVAILABLE_EXPR, "if_available_expr", IfAvailableExpr, 472)
ASTKIND(INVALID_EXPR, "invalid_expr", InvalidExpr, 488) // end of subtypes of Expr.
ASTKIND(GENERIC, "generic", Generic, 416)
ASTKIND(GENERIC_CONSTRAINT, "generic_constraint", GenericConstraint, 400)
ASTKIND(MATCH_CASE, "match_case", MatchCase, 376)
ASTKIND(MATCH_CASE_OTHER, "match_case_other", MatchCaseOther, 312)
ASTKIND(FUNC_ARG, "func_arg", FuncArg, 432)
ASTKIND(FUNC_PARAM_LIST, "func_param_list", FuncParamList, 368)
ASTKIND(FUNC_BODY, " func_body", FuncBody, 464)
ASTKIND(STRUCT_BODY, "struct_body", StructBody, 344)
ASTKIND(CLASS_BODY, "class_body", ClassBody, 344)
ASTKIND(INTERFACE_BODY, "interface_body", InterfaceBody, 344)
ASTKIND(DUMMY_BODY, "dummy_body", DummyBody, 272)
ASTKIND(IMPORT_CONTENT, "import_content", ImportContent, 616)
ASTKIND(IMPORT_SPEC, "import_spec", ImportSpec, 944)
ASTKIND(PACKAGE_SPEC, "package_spec", PackageSpec, 480)
ASTKIND(PACKAGE, "package", Package, 432)
ASTKIND(FEATURE_ID, "feature_id", FeatureId, 320)
ASTKIND(FEATURES_SET, "features_set", FeaturesSet, 368)
ASTKIND(FEATURES_DIRECTIVE, "features_directive", FeaturesDirective, 328)
ASTKIND(FILE, "file", File, 568)
ASTKIND(NODE, "node", Node, 272)
#endif
