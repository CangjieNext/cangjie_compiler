// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements tostring utils API for CHIR
 */

#include "cangjie/CHIR/Utils/ToStringUtils.h"

#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/Utils/Utils.h"
#include "cangjie/Utils/Utils.h"

namespace Cangjie::CHIR {

const std::unordered_map<CHIR::IntrinsicKind, std::string> INTRINSIC_KIND_TO_STRING_MAP{
    {CHIR::NOT_INTRINSIC, "notIntrinsic"}, {CHIR::NOT_IMPLEMENTED, "notImplemented"},

    // For hoisting, but we should later split arraybuilder
    // into allocation and initialisation
    {CHIR::ARRAY_INIT, "arrayInit"},

    // CORE
    {CHIR::SIZE_OF, CHIR::SIZE_OF_NAME}, {CHIR::ALIGN_OF, CHIR::ALIGN_OF_NAME},
    {CHIR::GET_TYPE_FOR_TYPE_PARAMETER, CHIR::GET_TYPE_FOR_TYPE_PARAMETER_NAME},
    {CHIR::IS_SUBTYPE_TYPES, CHIR::IS_SUBTYPE_TYPES_NAME},
    {CHIR::ARRAY_ACQUIRE_RAW_DATA, CHIR::ARRAY_ACQUIRE_RAW_DATA_NAME},
    {CHIR::ARRAY_RELEASE_RAW_DATA, CHIR::ARRAY_RELEASE_RAW_DATA_NAME},
    {CHIR::ARRAY_BUILT_IN_COPY_TO, CHIR::ARRAY_BUILT_IN_COPY_TO_NAME}, {CHIR::ARRAY_GET, CHIR::ARRAY_GET_NAME},
    {CHIR::ARRAY_SET, CHIR::ARRAY_SET_NAME}, {CHIR::ARRAY_GET_UNCHECKED, CHIR::ARRAY_GET_UNCHECKED_NAME},
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    {CHIR::ARRAY_GET_REF_UNCHECKED, CHIR::ARRAY_GET_REF_UNCHECKED_NAME},
#endif
    {CHIR::ARRAY_SET_UNCHECKED, CHIR::ARRAY_SET_UNCHECKED_NAME}, {CHIR::ARRAY_SIZE, CHIR::ARRAY_SIZE_NAME},
    {CHIR::ARRAY_CLONE, CHIR::ARRAY_CLONE_NAME}, {CHIR::ARRAY_SLICE_INIT, CHIR::ARRAY_SLICE_INIT_NAME},
    {CHIR::ARRAY_SLICE, CHIR::ARRAY_SLICE_NAME}, {CHIR::ARRAY_SLICE_RAWARRAY, CHIR::ARRAY_SLICE_RAWARRAY_NAME},
    {CHIR::ARRAY_SLICE_START, CHIR::ARRAY_SLICE_START_NAME}, {CHIR::ARRAY_SLICE_SIZE, CHIR::ARRAY_SLICE_SIZE_NAME},
    {CHIR::ARRAY_SLICE_GET_ELEMENT, CHIR::ARRAY_SLICE_GET_ELEMENT_NAME},
    {CHIR::ARRAY_SLICE_SET_ELEMENT, CHIR::ARRAY_SLICE_SET_ELEMENT_NAME},
    {CHIR::ARRAY_SLICE_GET_ELEMENT_UNCHECKED, CHIR::ARRAY_SLICE_GET_ELEMENT_UNCHECKED_NAME},
    {CHIR::ARRAY_SLICE_SET_ELEMENT_UNCHECKED, CHIR::ARRAY_SLICE_SET_ELEMENT_UNCHECKED_NAME},
    {CHIR::FILL_IN_STACK_TRACE, CHIR::FILL_IN_STACK_TRACE_NAME},
    {CHIR::DECODE_STACK_TRACE, CHIR::DECODE_STACK_TRACE_NAME},
    {CHIR::DUMP_CURRENT_THREAD_INFO, CHIR::DUMP_CURRENT_THREAD_INFO_NAME},
    {CHIR::DUMP_ALL_THREADS_INFO, CHIR::DUMP_ALL_THREADS_INFO_NAME},

    {CHIR::CHR, "chr"}, {CHIR::ORD, "ord"},

    {CHIR::CPOINTER_GET_POINTER_ADDRESS, CHIR::CPOINTER_GET_POINTER_ADDRESS_NAME},
    {CHIR::CPOINTER_INIT0, "pointerInit0"}, // CPointer constructor with no parameters
    {CHIR::CPOINTER_INIT1, "pointerInit1"}, // CPointer constructor with one parameter
    {CHIR::CPOINTER_READ, CHIR::CPOINTER_READ_NAME}, {CHIR::CPOINTER_WRITE, CHIR::CPOINTER_WRITE_NAME},
    {CHIR::CPOINTER_ADD, CHIR::CPOINTER_ADD_NAME},

    {CHIR::CSTRING_INIT, "_CString_init_"},
    {CHIR::CSTRING_CONVERT_CSTR_TO_PTR, CHIR::CSTRING_CONVERT_CSTR_TO_PTR_NAME},
    {CHIR::BIT_CAST, CHIR::BIT_CAST_NAME},

    {CHIR::INOUT_PARAM, "_inout_"},

    {CHIR::REGISTER_WATCHED_OBJECT, "registerWatchedObject"},

    {CHIR::OBJECT_REFEQ, CHIR::OBJECT_REFEQ_NAME},
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    {CHIR::RAW_ARRAY_REFEQ, CHIR::RAW_ARRAY_REFEQ_NAME},
    {CHIR::FUNC_REFEQ, CHIR::FUNC_REFEQ_NAME},
#endif
    {CHIR::OBJECT_ZERO_VALUE, CHIR::OBJECT_ZERO_VALUE_NAME},

    {CHIR::INVOKE_GC, CHIR::INVOKE_GC_NAME}, {CHIR::SET_GC_THRESHOLD, CHIR::SET_GC_THRESHOLD_NAME},
    {CHIR::GET_MAX_HEAP_SIZE, CHIR::GET_MAX_HEAP_SIZE_NAME},
    {CHIR::GET_ALLOCATE_HEAP_SIZE, CHIR::GET_ALLOCATE_HEAP_SIZE_NAME},
    {CHIR::DUMP_CJ_HEAP_DATA, CHIR::DUMP_CJ_HEAP_DATA_NAME},
    {CHIR::GET_GC_COUNT, CHIR::GET_GC_COUNT_NAME},
    {CHIR::GET_GC_TIME_US, CHIR::GET_GC_TIME_US_NAME},
    {CHIR::GET_GC_FREED_SIZE, CHIR::GET_GC_FREED_SIZE_NAME},
    {CHIR::START_CJ_CPU_PROFILING, CHIR::START_CJ_CPU_PROFILING_NAME},
    {CHIR::STOP_CJ_CPU_PROFILING, CHIR::STOP_CJ_CPU_PROFILING_NAME},
    {CHIR::GET_REAL_HEAP_SIZE, CHIR::GET_REAL_HEAP_SIZE_NAME},
    {CHIR::GET_THREAD_NUMBER, CHIR::GET_THREAD_NUMBER_NAME},
    {CHIR::GET_BLOCKING_THREAD_NUMBER, CHIR::GET_BLOCKING_THREAD_NUMBER_NAME},
    {CHIR::GET_NATIVE_THREAD_NUMBER, CHIR::GET_NATIVE_THREAD_NUMBER_NAME},

    {CHIR::VARRAY_SET, CHIR::VARRAY_SET_NAME}, {CHIR::VARRAY_GET, CHIR::VARRAY_GET_NAME},

    // About Future
    {CHIR::FUTURE_INIT, CHIR::FUTURE_INIT_NAME},
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    {CHIR::FUTURE_IS_COMPLETE, CHIR::FUTURE_IS_COMPLETE_NAME}, {CHIR::FUTURE_WAIT, CHIR::FUTURE_WAIT_NAME},
    {CHIR::FUTURE_NOTIFYALL, CHIR::FUTURE_NOTIFYALL_NAME},
#endif
    {CHIR::IS_THREAD_OBJECT_INITED, CHIR::IS_THREAD_OBJECT_INITED_NAME},
    {CHIR::GET_THREAD_OBJECT, CHIR::GET_THREAD_OBJECT_NAME},
    {CHIR::SET_THREAD_OBJECT, CHIR::SET_THREAD_OBJECT_NAME},

    {CHIR::OVERFLOW_CHECKED_ADD, CHIR::OVERFLOW_CHECKED_ADD_NAME},
    {CHIR::OVERFLOW_CHECKED_SUB, CHIR::OVERFLOW_CHECKED_SUB_NAME},
    {CHIR::OVERFLOW_CHECKED_MUL, CHIR::OVERFLOW_CHECKED_MUL_NAME},
    {CHIR::OVERFLOW_CHECKED_DIV, CHIR::OVERFLOW_CHECKED_DIV_NAME},
    {CHIR::OVERFLOW_CHECKED_MOD, CHIR::OVERFLOW_CHECKED_MOD_NAME},
    {CHIR::OVERFLOW_CHECKED_POW, CHIR::OVERFLOW_CHECKED_POW_NAME},
    {CHIR::OVERFLOW_CHECKED_INC, CHIR::OVERFLOW_CHECKED_INC_NAME},
    {CHIR::OVERFLOW_CHECKED_DEC, CHIR::OVERFLOW_CHECKED_DEC_NAME},
    {CHIR::OVERFLOW_CHECKED_NEG, CHIR::OVERFLOW_CHECKED_NEG_NAME},
    {CHIR::OVERFLOW_THROWING_ADD, CHIR::OVERFLOW_THROWING_ADD_NAME},
    {CHIR::OVERFLOW_THROWING_SUB, CHIR::OVERFLOW_THROWING_SUB_NAME},
    {CHIR::OVERFLOW_THROWING_MUL, CHIR::OVERFLOW_THROWING_MUL_NAME},
    {CHIR::OVERFLOW_THROWING_DIV, CHIR::OVERFLOW_THROWING_DIV_NAME},
    {CHIR::OVERFLOW_THROWING_MOD, CHIR::OVERFLOW_THROWING_MOD_NAME},
    {CHIR::OVERFLOW_THROWING_POW, CHIR::OVERFLOW_THROWING_POW_NAME},
    {CHIR::OVERFLOW_THROWING_INC, CHIR::OVERFLOW_THROWING_INC_NAME},
    {CHIR::OVERFLOW_THROWING_DEC, CHIR::OVERFLOW_THROWING_DEC_NAME},
    {CHIR::OVERFLOW_THROWING_NEG, CHIR::OVERFLOW_THROWING_NEG_NAME},
    {CHIR::OVERFLOW_SATURATING_ADD, CHIR::OVERFLOW_SATURATING_ADD_NAME},
    {CHIR::OVERFLOW_SATURATING_SUB, CHIR::OVERFLOW_SATURATING_SUB_NAME},
    {CHIR::OVERFLOW_SATURATING_MUL, CHIR::OVERFLOW_SATURATING_MUL_NAME},
    {CHIR::OVERFLOW_SATURATING_DIV, CHIR::OVERFLOW_SATURATING_DIV_NAME},
    {CHIR::OVERFLOW_SATURATING_MOD, CHIR::OVERFLOW_SATURATING_MOD_NAME},
    {CHIR::OVERFLOW_SATURATING_POW, CHIR::OVERFLOW_SATURATING_POW_NAME},
    {CHIR::OVERFLOW_SATURATING_INC, CHIR::OVERFLOW_SATURATING_INC_NAME},
    {CHIR::OVERFLOW_SATURATING_DEC, CHIR::OVERFLOW_SATURATING_DEC_NAME},
    {CHIR::OVERFLOW_SATURATING_NEG, CHIR::OVERFLOW_SATURATING_NEG_NAME},
    {CHIR::OVERFLOW_WRAPPING_ADD, CHIR::OVERFLOW_WRAPPING_ADD_NAME},
    {CHIR::OVERFLOW_WRAPPING_SUB, CHIR::OVERFLOW_WRAPPING_SUB_NAME},
    {CHIR::OVERFLOW_WRAPPING_MUL, CHIR::OVERFLOW_WRAPPING_MUL_NAME},
    {CHIR::OVERFLOW_WRAPPING_DIV, CHIR::OVERFLOW_WRAPPING_DIV_NAME},
    {CHIR::OVERFLOW_WRAPPING_MOD, CHIR::OVERFLOW_WRAPPING_MOD_NAME},
    {CHIR::OVERFLOW_WRAPPING_POW, CHIR::OVERFLOW_WRAPPING_POW_NAME},
    {CHIR::OVERFLOW_WRAPPING_INC, CHIR::OVERFLOW_WRAPPING_INC_NAME},
    {CHIR::OVERFLOW_WRAPPING_DEC, CHIR::OVERFLOW_WRAPPING_DEC_NAME},
    {CHIR::OVERFLOW_WRAPPING_NEG, CHIR::OVERFLOW_WRAPPING_NEG_NAME},
    {CHIR::REFLECTION_INTRINSIC_START_FLAG, "reflectionIntrinsicStart"},
    // REFLECTION
#define REFLECTION_KIND_TO_RUNTIME_FUNCTION(REFLECTION_KIND, CJ_FUNCTION, RUNTIME_FUNCTION)                            \
    {CHIR::REFLECTION_KIND, #CJ_FUNCTION},
#include "cangjie/CHIR/Utils/LLVMReflectionIntrinsics.def"
#undef REFLECTION_KIND_TO_RUNTIME_FUNCTION
    {CHIR::REFLECTION_INTRINSIC_END_FLAG, "reflectionIntrinsicEnd"},

    {CHIR::SLEEP, CHIR::SLEEP_NAME},

    {CHIR::SOURCE_FILE, CHIR::SOURCE_FILE_NAME}, {CHIR::SOURCE_LINE, CHIR::SOURCE_LINE_NAME},

    {CHIR::IDENTITY_HASHCODE, CHIR::IDENTITY_HASHCODE_NAME},
    {CHIR::IDENTITY_HASHCODE_FOR_ARRAY, CHIR::IDENTITY_HASHCODE_FOR_ARRAY_NAME},

#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
    // SYNC
    {CHIR::ATOMIC_LOAD, CHIR::ATOMIC_LOAD_NAME}, {CHIR::ATOMIC_STORE, CHIR::ATOMIC_STORE_NAME},
    {CHIR::ATOMIC_SWAP, CHIR::ATOMIC_SWAP_NAME},
    {CHIR::ATOMIC_COMPARE_AND_SWAP, CHIR::ATOMIC_COMPARE_AND_SWAP_NAME},
    {CHIR::ATOMIC_FETCH_ADD, CHIR::ATOMIC_FETCH_ADD_NAME}, {CHIR::ATOMIC_FETCH_SUB, CHIR::ATOMIC_FETCH_SUB_NAME},
    {CHIR::ATOMIC_FETCH_AND, CHIR::ATOMIC_FETCH_AND_NAME}, {CHIR::ATOMIC_FETCH_OR, CHIR::ATOMIC_FETCH_OR_NAME},
    {CHIR::ATOMIC_FETCH_XOR, CHIR::ATOMIC_FETCH_XOR_NAME}, {CHIR::MUTEX_INIT, CHIR::MUTEX_INIT_NAME},
    {CHIR::CJ_MUTEX_LOCK, CHIR::MUTEX_LOCK_NAME}, {CHIR::MUTEX_TRY_LOCK, CHIR::MUTEX_TRY_LOCK_NAME},
    {CHIR::MUTEX_CHECK_STATUS, CHIR::MUTEX_CHECK_STATUS_NAME}, {CHIR::MUTEX_UNLOCK, CHIR::MUTEX_UNLOCK_NAME},
    {CHIR::WAITQUEUE_INIT, CHIR::WAITQUEUE_INIT_NAME}, {CHIR::MONITOR_INIT, CHIR::MONITOR_INIT_NAME},
    {CHIR::MOITIOR_WAIT, CHIR::MOITIOR_WAIT_NAME}, {CHIR::MOITIOR_NOTIFY, CHIR::MOITIOR_NOTIFY_NAME},
    {CHIR::MOITIOR_NOTIFY_ALL, CHIR::MOITIOR_NOTIFY_ALL_NAME},
    {CHIR::MULTICONDITION_WAIT, CHIR::MULTICONDITION_WAIT_NAME},
    {CHIR::MULTICONDITION_NOTIFY, CHIR::MULTICONDITION_NOTIFY_NAME},
    {CHIR::MULTICONDITION_NOTIFY_ALL, CHIR::MULTICONDITION_NOTIFY_ALL_NAME},
    {CHIR::VECTOR_COMPARE_32, CHIR::VECTOR_COMPARE_32_NAME},
    {CHIR::VECTOR_INDEX_BYTE_32, CHIR::VECTOR_INDEX_BYTE_32_NAME},
    {CHIR::CJ_CORE_CAN_USE_SIMD, CHIR::CJ_CORE_CAN_USE_SIMD_NAME},
    {CHIR::CROSS_ACCESS_BARRIER, CHIR::CROSS_ACCESS_BARRIER_NAME},
    {CHIR::CREATE_EXPORT_HANDLE, CHIR::CREATE_EXPORT_HANDLE_NAME},
    {CHIR::GET_EXPORTED_REF, CHIR::GET_EXPORTED_REF_NAME},
    {CHIR::REMOVE_EXPORTED_REF, CHIR::REMOVE_EXPORTED_REF_NAME},
    {CHIR::GET_JSLAMBDA_ADDR, CHIR::GET_JSLAMBDA_ADDR_NAME},
#endif
    {CHIR::CJ_TLS_DYN_SET_SESSION_CALLBACK, CHIR::CJ_TLS_DYN_SET_SESSION_CALLBACK_NAME},
    {CHIR::CJ_TLS_DYN_SSL_INIT, CHIR::CJ_TLS_DYN_SSL_INIT_NAME},
    // CodeGen
    {CHIR::CG_UNSAFE_BEGIN, "cgUnsafeBegin"}, {CHIR::CG_UNSAFE_END, "cgUnsafeEnd"},
    // CHIR 2: Exception intrinsic
    {CHIR::BEGIN_CATCH, "beginCatch"},

    // CHIR 2: Math intrinsic
    {CHIR::ABS, "abs"}, {CHIR::FABS, "fabs"}, {CHIR::FLOOR, "floor"}, {CHIR::CEIL, "ceil"}, {CHIR::TRUNC, "trunc"},
    {CHIR::SIN, "sin"}, {CHIR::COS, "cos"}, {CHIR::EXP, "exp"}, {CHIR::EXP2, "exp2"}, {CHIR::LOG, "log"},
    {CHIR::LOG2, "log2"}, {CHIR::LOG10, "log10"}, {CHIR::SQRT, "sqrt"}, {CHIR::ROUND, "round"}, {CHIR::POW, "pow"},
    {CHIR::POWI, "powi"},
    // preinitialize intrinsic
    {CHIR::PREINITIALIZE, "preinitialize"},
    {CHIR::IS_THREAD_OBJECT_INITED, IS_THREAD_OBJECT_INITED_NAME},

    // Box cast intrinsic
    {CHIR::OBJECT_AS, "object.as"}, {CHIR::IS_NULL, "isNull"},
    {CHIR::BLACK_BOX, "blackBox"},

    // spawn related
    {CHIR::EXCLUSIVE_SCOPE, "exclusiveScopeImpl"},
};

void PrintIndent(std::ostream& stream, size_t indent)
{
    for (unsigned i = 0; i < indent * 2; i++) { // 2 denote width of indent.
        stream << " ";
    }
}

std::string GetGenericConstaintsStr(const std::vector<GenericType*>& genericTypeParams)
{
    std::stringstream ss;
    ss << "[";
    size_t i = 0;
    for (auto genericTypeParam : genericTypeParams) {
        auto upperBounds = genericTypeParam->GetUpperBounds();
        if (upperBounds.empty()) {
            ++i;
            continue;
        }
        ss << genericTypeParam->ToString();
        ss << " <: ";
        size_t j = 0;
        for (auto upperBound : upperBounds) {
            ss << upperBound->ToString();
            if (j < upperBounds.size() - 1) {
                ss << " & ";
            }
            ++j;
        }
        if (i < genericTypeParams.size() - 1) {
            ss << ", ";
        }
        ++i;
    }
    ss << "]";

    // Skip the "[]" if there is no constraints
    constexpr int bracketsSize{2};
    if (ss.str().size() == bracketsSize) {
        return "";
    }
    return ss.str();
}

std::string GetBlockStr(const Block& block, size_t indent)
{
    std::stringstream ss;
    PrintIndent(ss, indent);
    ss << block.GetAttributeInfo().ToString();
    ss << "Block " << block.GetIdentifier() << ": ";
    ss << "// preds: ";
    auto predecessors = block.GetPredecessors();
    for (size_t i = 0; i < predecessors.size(); i++) {
        if (i > 0) {
            ss << ", ";
        }
        ss << "#" << predecessors[i]->GetIdentifierWithoutPrefix();
    }
    if (auto annostr = block.ToStringAnnotationMap(); !annostr.empty()) {
        ss << " // " << annostr;
    }
    if (block.IsLandingPadBlock()) {
        PrintIndent(ss, indent + 1);
        ss << "// exceptions: " << GetExceptionsStr(block.GetExceptions());
    }
    ss << std::endl;
    auto exprs = block.GetExpressions();
    for (auto& expr : exprs) {
        PrintIndent(ss, indent + 1);
        if (auto res = expr->GetResult(); res != nullptr) {
            if (expr->GetResult()->IsRetValue()) {
                ss << "[ret] ";
            }
            ss << res->GetAttributeInfo().ToString();
            ss << res->GetIdentifier();
            if (!res->GetSrcCodeIdentifier().empty()) {
                ss << "[" << res->GetSrcCodeIdentifier() << "]";
            }
            ss << ": " << res->GetType()->ToString() << " = ";
        }
        ss << expr->ToString(indent + 1);
        ss << "\n";
    }
    return ss.str();
}

std::string GetBlockGroupStr(const BlockGroup& blockGroup, size_t indent)
{
    std::stringstream ss;
    PrintIndent(ss, indent);
    ss << "{ "
       << "// Block Group: " << blockGroup.GetIdentifier() << "\n";

    if (!blockGroup.GetBlocks().empty()) {
        auto cmp = [](const Ptr<const Block> b1, const Ptr<const Block> b2) {
            return b1->GetIdentifier() < b2->GetIdentifier();
        };
        auto blocks = Utils::VecToSortedSet<decltype(cmp)>(blockGroup.GetBlocks(), cmp);
        auto sortedBlock = TopologicalSort(blockGroup.GetEntryBlock());
        for (auto block : sortedBlock) {
            ss << GetBlockStr(*block, indent);
            blocks.erase(block);
        }

        // print orphan block
        for (auto block : blocks) {
            ss << GetBlockStr(*block, indent);
        }
    }

    PrintIndent(ss, indent);
    ss << "}";
    return ss.str();
}

std::string GetGenericTypeParamsStr(const std::vector<GenericType*>& genericTypeParams)
{
    std::string res;
    if (!genericTypeParams.empty()) {
        res += "<";
        size_t i = 0;
        for (auto genericTypeParam : genericTypeParams) {
            res += genericTypeParam->ToString();
            if (i < genericTypeParams.size() - 1) {
                res += ", ";
            }
            ++i;
        }
        res += ">";
    }

    return res;
}

std::string GetGenericTypeParamsConstraintsStr(const std::vector<GenericType*>& genericTypeParams)
{
    std::string res;
    if (!genericTypeParams.empty()) {
        auto constraintsStr = GetGenericConstaintsStr(genericTypeParams);
        if (!constraintsStr.empty()) {
            res += "genericConstraints: " + constraintsStr;
        }
    }

    return res;
}

std::string GetLambdaStr(const Lambda& lambda, size_t indent)
{
    std::stringstream ss;
    if (lambda.IsCompileTimeValue()) {
        ss << "[compileTimeVal] ";
    }
    if (lambda.IsLocalFunc()) {
        ss << "[localFunc] ";
    }
    ss << "Lambda";
    if (!lambda.GetIdentifier().empty()) {
        ss << "[" << lambda.GetIdentifier() << "]";
    }
    ss << GetGenericTypeParamsStr(lambda.GetGenericTypeParams());
    ss << "(";
    auto& params = lambda.GetParams();
    if (!params.empty()) {
        ss << params[0]->GetAttributeInfo().ToString();
        ss << params[0]->GetIdentifier();
        if (auto srcName = params[0]->GetSrcCodeIdentifier(); !srcName.empty()) {
            ss << "[" << srcName << "]";
        }
        ss << ": " << params[0]->GetType()->ToString();
    }
    for (size_t i = 1; i < params.size(); i++) {
        ss << ", ";
        ss << params[i]->GetAttributeInfo().ToString();
        ss << params[i]->GetIdentifier();
        if (auto srcName = params[i]->GetSrcCodeIdentifier(); !srcName.empty()) {
            ss << "[" << srcName << "]";
        }
        ss << ": " << params[i]->GetType()->ToString();
    }
    ss << ")";
    ss << "=> {";
    ss << " // ";
    ss << " srcCodeIdentifier: " << lambda.GetSrcCodeIdentifier();
    ss << GetGenericTypeParamsConstraintsStr(lambda.GetGenericTypeParams());
    ss << "\n" << GetBlockGroupStr(*lambda.GetBody(), indent + 1);
    ss << "}";
    return ss.str();
}

std::string FuncSymbolStr(const Function& func)
{
    std::stringstream ss;
    ss << func.GetAttributeInfo().ToString();
    if (func.IsFastNative()) {
        ss << "[fastNative] ";
    }
    if (func.IsCFFIWrapper()) {
        ss << "[CFFIWrapper] ";
    }
    ss << "Func " << func.GetIdentifier();
    ss << GetGenericTypeParamsStr(func.GetGenericTypeParams());
    ss << "(";
    auto& params = func.GetParams();
    if (!params.empty()) {
        ss << params[0]->GetAttributeInfo().ToString();
        ss << params[0]->GetIdentifier();
        if (!params[0]->GetSrcCodeIdentifier().empty()) {
            ss << "[" << params[0]->GetSrcCodeIdentifier() << "]";
        }
        ss << ": " << params[0]->GetType()->ToString();
    }
    for (size_t i = 1; i < params.size(); i++) {
        ss << ", ";
        ss << params[i]->GetAttributeInfo().ToString();
        ss << params[i]->GetIdentifier();
        if (!params[i]->GetSrcCodeIdentifier().empty()) {
            ss << "[" << params[i]->GetSrcCodeIdentifier() << "]";
        }
        ss << ": " << params[i]->GetType()->ToString();
    }
    ss << ") : " << func.GetReturnType()->ToString();
    std::stringstream attrss;
    attrss << GetGenericTypeParamsConstraintsStr(func.GetGenericTypeParams());
    if (auto kind = func.GetFuncKind(); kind != FuncKind::DEFAULT) {
        if (attrss.str() != "") {
            attrss << ", ";
        }
        attrss << "kind: " << FUNCKIND_TO_STRING.at(kind);
    }
    auto annostr = func.ToStringAnnotationMap();
    if (attrss.str() != "" && annostr != "") {
        attrss << ", ";
    }
    attrss << annostr;
    if (auto gFunc = func.GetGenericDecl(); gFunc && gFunc->IsFuncWithBody()) {
        attrss << ", genericDecl: " << gFunc->GetIdentifierWithoutPrefix();
    }
    if (func.GetParentCustomTypeDef() != nullptr) {
        attrss << ", declared parent: " << func.GetParentCustomTypeDef()->GetIdentifierWithoutPrefix();
    }
    if (attrss.str() != "") {
        ss << " // " << attrss.str();
    }
    if (func.GetParamDftValHostFunc()) {
        ss << " paramDftValHostFunc: " << func.GetParamDftValHostFunc()->GetIdentifier();
    }
    if (func.GetFuncKind() == FuncKind::LAMBDA) {
        if (func.GetOriginalLambdaType()) {
            ss << " originalLambdaInfo: " << GetGenericTypeParamsStr(func.GetOriginalGenericTypeParams())
               << func.GetOriginalLambdaType()->ToString();
        }
    }
    auto funcAnno = func.GetAnnoInfo();
    if (funcAnno.IsAvailable()) {
        ss << " funcAnnoInfo: " << funcAnno.mangledName;
    }
    if (!func.GetSrcCodeIdentifier().empty()) {
        ss << " srcCodeIdentifier: " << func.GetSrcCodeIdentifier();
    }
    std::vector<Parameter*> paramWithAnnos;
    paramWithAnnos.reserve(params.size());
    std::copy_if(params.begin(), params.end(), std::back_inserter(paramWithAnnos),
        [](const Ptr<const Parameter>& param) { return param->GetAnnoInfo().IsAvailable(); });
    if (!paramWithAnnos.empty()) {
        ss << " paramAnnoInfo: ";
        for (size_t i = 0; i < paramWithAnnos.size(); ++i) {
            if (i != paramWithAnnos.size() - 1) {
                ss << ", ";
            }
            auto paramAnno = paramWithAnnos[i]->GetAnnoInfo();
            ss << paramWithAnnos[i]->GetSrcCodeIdentifier() + " : " + paramAnno.mangledName;
        }
    }
    ss << "\n";
    return ss.str();
}

std::string GetFuncStr(const Function& func, size_t indent)
{
    std::stringstream ss;
    ss << FuncSymbolStr(func);
    if (func.GetBody()) {
        ss << GetBlockGroupStr(*func.GetBody(), indent);
    }
    return ss.str();
}

std::string GetImportedVarStr(const GlobalVar& value)
{
    std::stringstream ss;
    ss << "#from <" << value.GetPackageName() << "> ";
    ss << "import " << value.GetAttributeInfo().ToString() << value.GetIdentifier() << ": "
       << value.GetType()->ToString();
    std::stringstream attrss;
    auto annostr = value.ToStringAnnotationMap();
    if (attrss.str() != "" && annostr != "") {
        attrss << ", ";
    }
    attrss << annostr;
    if (attrss.str() != "") {
        ss << " // " << attrss.str();
    }
    return ss.str();
}

std::string GetImportedFuncStr(const Function& value)
{
    std::stringstream ss;
    ss << "#from <" << value.GetPackageName() << "> ";
    ss << "import " << value.GetAttributeInfo().ToString();
    if (value.IsFastNative()) {
        ss << "[fastNative] ";
    }
    if (value.IsCFFIWrapper()) {
        ss << "[CFFIWrapper] ";
    }
    ss << value.GetIdentifier();
    // params
    const auto& params = value.GetParams();
    ss << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        ss << params[i]->ToString();
        if (i != params.size() - 1) {
            ss << ", ";
        }
    }
    ss << ")";
    ss << ": " << value.GetType()->ToString();
    std::stringstream attrss;
    if (auto genericTypeParams = value.GetGenericTypeParams(); !genericTypeParams.empty()) {
        auto constraintsStr = GetGenericConstaintsStr(genericTypeParams);
        if (!constraintsStr.empty()) {
            attrss << "genericConstraints: " << constraintsStr;
        }
    }
    if (auto kind = value.GetFuncKind(); kind != FuncKind::DEFAULT) {
        if (attrss.str() != "") {
            attrss << ", ";
        }
        attrss << "kind: " << FUNCKIND_TO_STRING.at(kind);
    }
    auto annostr = value.ToStringAnnotationMap();
    if (attrss.str() != "" && annostr != "") {
        attrss << ", ";
    }
    attrss << annostr;
    if (auto gFunc = value.GetGenericDecl(); gFunc && gFunc->IsFuncWithBody()) {
        attrss << ", genericDecl: " << gFunc->GetIdentifierWithoutPrefix();
    }
    if (attrss.str() != "") {
        ss << " // " << attrss.str();
    }
    if (value.GetParamDftValHostFunc()) {
        ss << " paramDftValHostFunc: " << value.GetParamDftValHostFunc()->GetIdentifier();
    }
    if (auto anno = value.GetAnnoInfo(); anno.IsAvailable()) {
        ss << " funcAnnoInfo: " << anno.mangledName;
    }
    ss << " srcCodeIdentifier: " << value.GetSrcCodeIdentifier();
    if (auto rawMangledName = value.GetRawMangledName(); !rawMangledName.empty()) {
        ss << " rawMangledName: " << rawMangledName;
    }
    return ss.str();
}

std::string GetExceptionsStr(const std::vector<ClassType*>& exceptions)
{
    std::stringstream ss;
    ss << "[ ";
    if (exceptions.empty()) {
        ss << "ALL";
    } else {
        size_t i = 0;
        for (auto& e : exceptions) {
            ss << e->ToString();
            if (i != exceptions.size() - 1) {
                ss << ", ";
            }
            ++i;
        }
    }
    ss << " ]";
    return ss.str();
}

void AddCommaOrNot(std::stringstream& ss)
{
    if (ss.str() != "") {
        ss << ", ";
    }
}

std::string PackageAccessLevelToString(const Package::AccessLevel& level)
{
    auto it = PACKAGE_ACCESS_LEVEL_TO_STRING_MAP.find(level);
    CJC_ASSERT(it != PACKAGE_ACCESS_LEVEL_TO_STRING_MAP.end());
    return it->second;
}

std::string CustomTypeKindToString(const CustomTypeDef& def)
{
    if (def.IsInterface()) {
        return "interface";
    } else if (def.IsClass()) {
        return "class";
    } else if (def.IsStruct()) {
        return "struct";
    } else if (def.IsEnum()) {
        return "enum";
    } else if (def.IsExtend()) {
        return "extend";
    }
    CJC_ABORT();
    return "";
}

std::string BoolToString(bool flag)
{
    if (flag) {
        return "true";
    } else {
        return "false";
    }
}

StringWrapper ThisTypeToString(const Type* thisType)
{
    StringWrapper res;
    if (thisType) {
        res.Append("ThisType: ");
        res.Append(thisType->ToString());
    }
    return res;
}

std::string InstTypeArgsToString(const std::vector<Type*>& instTypeArgs)
{
    std::string res;
    if (!instTypeArgs.empty()) {
        res += "<";
        for (auto type : instTypeArgs) {
            res += type->ToString();
            res += ", ";
        }
        // remove the last ", "
        res.pop_back();
        res.pop_back();
        res += ">";
    }
    return res;
}

std::string SuccessorsToString(const std::vector<Block*>& successors)
{
    std::string res;
    if (successors.size() > 0) {
        // normal case
        res += "normal: " + successors[0]->GetIdentifier();
    }
    if (successors.size() > 1) {
        // exception case, exception list
        res += ", exception";
        if (successors[1]->IsLandingPadBlock()) {
            res += " " + GetExceptionsStr(successors[1]->GetExceptions()) + ": ";
        } else {
            res += ": ";
        }
        res += successors[1]->GetIdentifier();
    }
    constexpr size_t rethrowIndex = 2;
    if (successors.size() > rethrowIndex) {
        // rethrow case
        res += ", rethrow: " + successors[rethrowIndex]->GetIdentifier();
    }
    return res;
}

std::string ExprOperandsToString(const std::vector<Value*>& args)
{
    std::string res;
    if (!args.empty()) {
        for (auto arg : args) {
            res += arg->GetIdentifier();
            res += ", ";
        }
        res.pop_back();
        res.pop_back();
    }
    return res;
}

std::string ExprWithExceptionOperandsToString(const std::vector<Value*>& args, const std::vector<Block*>& successors)
{
    std::string res;
    for (auto arg : args) {
        res += arg->GetIdentifier();
        res += ", ";
    }
    res += SuccessorsToString(successors);
    return res;
}

std::string OverflowToString(Cangjie::OverflowStrategy ofStrategy)
{
    static const std::unordered_map<Cangjie::OverflowStrategy, std::string> OVERFLOW_STR_MAP {
        {Cangjie::OverflowStrategy::NA, "NA"},
        {Cangjie::OverflowStrategy::CHECKED, "CHECKED"},
        {Cangjie::OverflowStrategy::WRAPPING, "WRAPPING"},
        {Cangjie::OverflowStrategy::THROWING, "THROWING"},
        {Cangjie::OverflowStrategy::SATURATING, "SATURATING"},
    };
    return "Overflow: " + OVERFLOW_STR_MAP.at(ofStrategy);
}
} // namespace Cangjie::CHIR
