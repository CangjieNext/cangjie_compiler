// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares some utility constants.
 */

#ifndef CANGJIE_CONSTANTSUTILS_H
#define CANGJIE_CONSTANTSUTILS_H

#include <map>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdint>

namespace Cangjie {
enum class OverflowStrategy : uint8_t { NA, CHECKED, WRAPPING, THROWING, SATURATING, OVERFLOW_STRATEGY_END };

constexpr std::string_view MAIN_INVOKE = "$mainInvoke";
constexpr std::string_view ENV_NAME = "$env";
constexpr std::string_view TEST_ENTRY_NAME = "$test.entry";
constexpr std::string_view BOX_DECL_PREFIX = "$BOX_";
constexpr std::string_view STATIC_INIT_VAR = "$init";
constexpr std::string_view STATIC_INIT_FUNC = "static.init";
constexpr std::string_view TO_ANY{"$toAny"};

constexpr std::string_view INOUT_GHOST_SYSCALL = "_inout_";

constexpr std::string_view ITER_COMPILER = "iter-compiler";
constexpr std::string_view V_COMPILER = "v-compiler";
constexpr std::string_view INVALID_IDENTIFIER = "<invalid identifier>";
constexpr std::string_view RESOURCE_NAME = "v-freshExc";

constexpr std::string_view CANGJIE_HOME = "CANGJIE_HOME";
constexpr std::string_view SOURCEFILE = "sourceFile";
constexpr std::string_view SOURCELINE = "sourceLine";
constexpr std::string_view SANCOV_VARIABLE_FLAG = "$sancov$";

// File extension.
constexpr std::string_view SERIALIZED_FILE_EXTENSION = ".cjo";
constexpr std::string_view SERIALIZED_FILE_FLAG_EXTENSION = ".cjo.flag";
constexpr std::string_view CJ_D_FILE_EXTENSION = ".cj.d";
constexpr std::string_view FULL_BCHIR_SERIALIZED_FILE_EXTENSION = ".full.bchir";
constexpr std::string_view BCHIR_SERIALIZED_FILE_EXTENSION = ".bchir";
constexpr std::string_view CACHED_AST_EXTENSION = ".cachedast";
constexpr std::string_view CACHED_IMPORTED_CJO_EXTENSION = ".iast";
constexpr std::string_view CACHED_CHIR_OPT_EXTENSION = ".cachedchiropt";
constexpr std::string_view CACHED_GLOBAL_DECL_DEP_EXTENSION = ".cachedgdecldep";
constexpr std::string_view CACHED_MODIFIED_AST_EXTENSION = ".cachedModified";
constexpr std::string_view CACHED_LOG_EXTENSION = ".log";
constexpr std::string_view CHIR_SERIALIZATION_FILE_EXTENSION = ".chir";
constexpr std::string_view CHIR_READABLE_FILE_EXTENSION = ".chirtxt";

// Built-in type name.
constexpr std::string_view CLASS_EXCEPTION = "Exception";
constexpr std::string_view CLASS_ERROR = "Error";
constexpr std::string_view CLASS_COMMAND = "Command";
constexpr std::string_view CLASS_HANDLER_FRAME = "HandlerFrame";
constexpr std::string_view CLASS_IMMEDIATE_FRAME = "ImmediateFrame";
constexpr std::string_view CLASS_DOUBLE_RESUME_EXCEPTION = "DoubleResumeException";
constexpr std::string_view CLASS_FRAME_EXCEPTION_WRAPPER = "ImmediateFrameExceptionWrapper";
constexpr std::string_view CLASS_FRAME_ERROR_WRAPPER = "ImmediateFrameErrorWrapper";
constexpr std::string_view CLASS_EARLY_RETURN = "ImmediateEarlyReturn";
constexpr std::string_view OPTION_NAME = "Option";
constexpr std::string_view OPTION_VALUE_CTOR = "Some";
constexpr std::string_view OPTION_NONE_CTOR = "None";
constexpr std::string_view ANY_NAME = "Any";
constexpr std::string_view OBJECT_NAME = "Object";
constexpr std::string_view JOBJECT_NAME = "JObject";
constexpr std::string_view OCOBJECT_NAME = "NSObject";
constexpr std::string_view CTYPE_NAME = "CType";
constexpr std::string_view RAW_ARRAY_NAME = "RawArray";
constexpr std::string_view CPOINTER_NAME = "CPointer";
constexpr std::string_view CSTRING_NAME = "CString";
constexpr std::string_view CFUNC_NAME = "CFunc";
constexpr std::string_view VARRAY_NAME = "VArray";
constexpr std::string_view TOSTRING_NAME = "ToString";
constexpr std::string_view BOX_NAME = "Box";

constexpr std::string_view JTYPE_NAME = "JType";
constexpr std::string_view JARRAY_NAME = "JArray";

// Macro with context.
inline const int MACRO_DEF_NUM = 2;
constexpr std::string_view MC_EXCEPTION = "MacroContextException";
inline const int MACRO_COMMON_ARGS = 1;
inline const int MACRO_ATTR_ARGS = 2;
constexpr std::string_view MACRO_OBJECT_NAME = "MACRO_OBJECT";
constexpr std::string_view IF_AVAILABLE = "IfAvailable";

// Standard library package name
// Please do not change the const char[] type to std::string type because the initialization order of
// std::string and std::map types across translation units is undefined,especially on the Windows platform.
constexpr std::string_view DEFAULT_PACKAGE_NAME = "default";
inline constexpr const char CORE_PACKAGE_NAME[] = "std.core";
inline constexpr const char SYNC_PACKAGE_NAME[] = "std.sync";
inline constexpr const char MATH_PACKAGE_NAME[] = "std.math";
inline constexpr const char OVERFLOW_PACKAGE_NAME[] = "std.overflow";
inline constexpr const char RUNTIME_PACKAGE_NAME[] = "std.runtime";
inline constexpr const char UNICODE_PACKAGE_NAME[] = "std.unicode";
inline constexpr const char UNITTEST_MOCK_PACKAGE_NAME[] = "std.unittest.mock";
inline constexpr const char UNITTEST_MOCK_INTERNAL_PACKAGE_NAME[] = "std.unittest.mock.internal";
inline constexpr const char AST_PACKAGE_NAME[] = "std.ast";
inline constexpr const char INTEROP_PACKAGE_NAME[] = "std.interop";
inline constexpr const char NET_PACKAGE_NAME[] = "std.net";
inline constexpr const char REFLECT_PACKAGE_NAME[] = "std.reflect";
inline constexpr const char REF_PACKAGE_NAME[] = "std.ref";
inline constexpr const char EFFECT_INTERNALS_PACKAGE_NAME[] = "stdx.effect";
inline constexpr const char EFFECT_PACKAGE_NAME[] = "stdx.effect";

// Standard library class name
constexpr std::string_view STD_LIB_ARRAY = "Array";
constexpr std::string_view STD_LIB_FUTURE = "Future";
constexpr std::string_view STD_LIB_MONITOR = "Monitor";
constexpr std::string_view STD_LIB_MUTEX = "Mutex";
constexpr std::string_view STD_LIB_OPTION = "Option";
constexpr std::string_view STD_LIB_STRING = "String";
constexpr std::string_view STD_LIB_WAIT_QUEUE = "WaitQueue";
constexpr std::string_view STD_LIB_WEAK_REF = "WeakRef";
constexpr std::string_view INTEROP_LIB_EXPORTED_REF = "ExportedRef";
constexpr std::string_view INTEROP_LIB_FOREIGN_PROXY = "ForeignProxy";

// Global init function name for CHIR, CodeGen.
constexpr std::string_view ANNOTATION_VAR_POSTFIX{"@Annotation@"};

// Closure Conversion
constexpr std::string_view CC_DEF_PREFIX = "$C";
constexpr std::string_view GENERIC_VIRTUAL_FUNC = "$GenericVirtualFunc";
constexpr std::string_view INST_VIRTUAL_FUNC = "$InstVirtualFunc";

// CFFI
constexpr std::string_view CFFI_FUNC_SUFFIX = "$real";

// Identifier for generated flag to check if instance of a class is initialized.
constexpr std::string_view HAS_INITED_IDENT = "$hasInited";

namespace Interop::Java {
constexpr std::string_view INTEROP_JOBJECT_NAME = "JObject";
constexpr std::string_view INTEROP_JSTRING_NAME = "JString";
constexpr std::string_view INTEROP_JARRAY_NAME = "JArray";
constexpr std::string_view INTEROP_JAVA_LANG_PACKAGE = "java.lang";
constexpr std::string_view JAVA_REF_FIELD_NAME = "$javaref";
constexpr std::string_view JAVA_FWD_CLASS_SUFFIX = "_fwd";
constexpr std::string_view JAVA_INTERFACE_FWD_CLASS_DEFAULT_METHOD_SUFFIX = "_default_impl";
constexpr std::string_view JAVA_SELF_OBJECT = "selfobj";
constexpr auto JAVA_REF_GETTER_FUNC_NAME = "$getJavaRef";
constexpr auto INTEROPLIB_CFFI_JAVA_ENTITY = "Java_CFFI_JavaEntity";
constexpr auto JAVA_OBJECT_CONTROLLER = "JavaObjectController";
constexpr auto JAVA_OBJECT_CONTROLLER_NAME = "controller";
constexpr auto JAVA_OVERRIDE_MASK_NAME = "overrideMask";
constexpr auto ENV = "env";
} // namespace Cangjie::Interop::Java

namespace Interop::ObjC {
constexpr std::string_view OBJ_C_ID_IDENT = "ObjCId";
constexpr std::string_view OBJ_C_POINTER_IDENT = "ObjCPointer";
constexpr std::string_view OBJ_C_FUNC_IDENT = "ObjCFunc";
constexpr std::string_view OBJ_C_BLOCK_IDENT = "ObjCBlock";
constexpr std::string_view OBJ_C_LANG_PACKAGE_IDENT = "objc.lang";
constexpr std::string_view NATIVE_HANDLE_IDENT = "$obj";
constexpr std::string_view GET_OBJ_C_CLASS_IDENT = "$getObjCClass";
constexpr std::string_view REGISTRY_ID_IDENT = "$registryId";
constexpr std::string_view REGISTRY_ID_SETTER_SELECTOR = "set$registryId:";
constexpr std::string_view SYNTHETIC_CLASS_SUFFIX = "$impl";
constexpr std::string_view NATIVE_HANDLE_GETTER_IDENT = "$getObj";
constexpr std::string_view OBJ_C_FWD_CLASS_SUFFIX = "_fwd";
} // namespace Interop::ObjC

// Headless instrinsics
constexpr std::string_view GET_TYPE_FOR_TYPE_PARAMETER_FUNC_NAME = "getTypeForTypeParameter";
constexpr std::string_view IS_SUBTYPE_TYPES_FUNC_NAME = "isSubtypeTypes";

constexpr std::string_view ORG_NAME_SEPARATOR{"@"};
} // namespace Cangjie
#endif // CANGJIE_CONSTANTSUTILS_H