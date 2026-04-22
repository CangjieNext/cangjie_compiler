// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/TranslateASTNode/Translator.h"
#include "cangjie/CHIR/AST2CHIR/Utils.h"
#include "cangjie/CHIR/IR/IntrinsicKind.h"

using namespace Cangjie::CHIR;
using namespace Cangjie;

void Translator::SetSymbolTable(const AST::Node& node, Value& val, bool isLocal)
{
    CJC_ASSERT(node.IsDecl() ||
        Utils::In(node.astKind, {AST::ASTKind::BLOCK, AST::ASTKind::TUPLE_PATTERN, AST::ASTKind::ENUM_PATTERN}));
    if (isLocal) {
        localValSymbolTable.Set(node, val);
    } else {
        globalSymbolTable.Set(node, val);
    }
}

void Translator::SetTopLevel(const AST::Decl& decl)
{
    topLevelDecl = &decl;
}
bool Translator::IsTopLevel(const AST::Decl& decl) const
{
    return topLevelDecl == &decl;
}

static bool ShouldInGlobalTable(const AST::Node& node)
{
    // 1. global decl, 'NominalDecl' is stored to indicate 'this' and should be found from local table.
    if (node.TestAttr(AST::Attribute::GLOBAL) && !node.IsNominalDecl()) {
        return true;
    }

    // 2. member method or static member var
    if ((node.TestAttr(AST::Attribute::STATIC) && node.astKind == AST::ASTKind::VAR_DECL) ||
        node.astKind == AST::ASTKind::FUNC_DECL) {
        const auto& decl = StaticCast<const AST::Decl&>(node);
        if (decl.outerDecl != nullptr && decl.outerDecl->IsNominalDecl()) {
            return true;
        }
    }
    return false;
}

Ptr<Value> Translator::GetSymbolTable(const AST::Node& node) const
{
    CJC_ASSERT(node.IsDecl() ||
        Utils::In(node.astKind, {AST::ASTKind::BLOCK, AST::ASTKind::TUPLE_PATTERN, AST::ASTKind::ENUM_PATTERN}));
    // Note this is a temporary solution here, we should be sure that all local consts are in global table
    // once the universal generics are implemented
    if (ShouldInGlobalTable(node)) {
        return globalSymbolTable.Get(node);
    }
    if (node.IsDecl() && localConstVars.HasElement(StaticCast<AST::Decl>(&node))) {
        return globalSymbolTable.Get(node);
    }
    if (auto func = DynamicCast<AST::FuncDecl>(&node); func && localConstFuncs.HasElement(func)) {
        return globalSymbolTable.Get(node);
    }
    return localValSymbolTable.Get(node);
}

Ptr<Type> Translator::TranslateType(AST::Ty& ty)
{
    return chirTy.TranslateType(ty);
}

Ptr<FuncType> Translator::CreateVirtualFuncType(const AST::FuncDecl& decl)
{
    std::vector<Type*> args;
    for (size_t i = 0; i < decl.funcBody->paramLists[0]->params.size(); i++) {
        args.emplace_back(TranslateType(*decl.funcBody->paramLists[0]->params[i]->GetTy()));
    }
    return builder.GetType<FuncType>(args, builder.GetUnitTy());
}

Ptr<CustomTypeDef> Translator::GetNominalSymbolTable(const AST::Node& node)
{
    return chirTy.GetGlobalNominalCache(StaticCast<const AST::Decl&>(node));
}

DebugLocation Translator::GetValueDebugLocationInfo(const Value& value) const
{
    DebugLocation loc;
    if (auto localVar = DynamicCast<const LocalVar*>(&value); localVar) {
        loc = localVar->GetExpr()->GetDebugLocation();
    } else {
        loc = value.GetDebugLocation();
    }
    loc.SetScopeInfo(scopeInfo);
    return loc;
}

DebugLocation Translator::TranslateLocation(const AST::Node& node) const
{
    return TranslateLocation(node.begin, node.end);
}

DebugLocation Translator::TranslateLocation(const Cangjie::Position& begin, const Cangjie::Position& end) const
{
    if (begin.GetStatus() == PositionStatus::IGNORE) {
        return TranslateFileLocation(begin.fileID);
    }
    auto loc = TranslateLocationWithoutScope(builder.GetChirContext(), begin, end);
    loc.SetScopeInfo(scopeInfo);
    return loc;
}

DebugLocation Translator::TranslateFileLocation(unsigned fileID) const
{
    return {builder.GetChirContext().GetSourceFileName(fileID), fileID, {0, 0}, {0, 0}};
}

Ptr<Value> Translator::TypeCastOrBoxIfNeeded(Value& val, Type& expectedTy, const DebugLocation& loc, bool needCheck)
{
    return CHIR::TypeCastOrBoxIfNeeded(val, expectedTy, builder, *currentBlock, loc, needCheck);
}

Ptr<Value> Translator::GetDerefedValue(Ptr<Value> val, const DebugLocation& loc)
{
    CJC_NULLPTR_CHECK(val);
    auto valType = val->GetType();
    if (valType->IsRef()) {
        valType = StaticCast<RefType*>(valType)->GetBaseType();
        // For now, raw class type should not exist. Why, spec indicate that class is reference type.
        if (valType->IsClassOrArray()) {
            return val;
        }
        auto expr = (loc.IsInvalidPos() ? CreateAndAppendExpression<Load>(valType, val, currentBlock)
                                        : CreateAndAppendExpression<Load>(loc, valType, val, currentBlock));
        return expr->GetResult();
    }
    return val;
}

Ptr<Value> Translator::GetImplicitThisParam() const
{
    auto curFunc = GetCurrentFunc();
    CJC_ASSERT(curFunc);
    return curFunc->GetParam(0);
}

void Translator::SetFuncBlockGroup(BlockGroup& group)
{
    blockGroupStack.clear();
    blockGroupStack.emplace_back(&group);
}

void Translator::SetCurrentBlock(Block& block)
{
    currentBlock = &block;
}

Ptr<Block> Translator::GetCurrentBlock() const
{
    return currentBlock;
}

Ptr<Function> Translator::CreateEmptyGVInitFunc(const std::string& mangledName, const std::string& identifier,
    const std::string& rawMangledName, const std::string& pkgName, const Linkage& linkage, const DebugLocation& loc,
    bool isConst)
{
    Function* func = TryGetFromCache<Value, Function>(GLOBAL_VALUE_PREFIX + mangledName, deserializedVals);
    BlockGroup* blockGroup = nullptr;
    if (func) {
        // found deserialized one
        blockGroup = builder.CreateBlockGroup(*func);
        func->ReplaceBody(*blockGroup);
    } else {
        auto funcTy = builder.GetType<FuncType>(std::vector<Type*>{}, builder.GetUnitTy());
        func = builder.CreateFunction(funcTy, mangledName, identifier, rawMangledName, pkgName);
        blockGroup = builder.CreateBlockGroup(*func);
        func->SetFuncKind(FuncKind::GLOBALVAR_INIT);
        func->EnableAttr(Attribute::NO_REFLECT_INFO);
        func->EnableAttr(Attribute::NO_INLINE);
        func->EnableAttr(Attribute::COMPILER_ADD);
        func->EnableAttr(Attribute::INITIALIZER);
        if (isConst) {
            func->EnableAttr(Attribute::CONST);
        }
        func->InitBody(*blockGroup);
    }
    func->template Set<LinkTypeInfo>(Linkage(linkage));
    func->SetDebugLocation(loc);

    auto entry = builder.CreateBlock(blockGroup);
    blockGroup->SetEntryBlock(entry);
    SetFuncBlockGroup(*blockGroup);
    SetCurrentBlock(*entry);

    auto unitTyRef = builder.GetType<RefType>(builder.GetUnitTy());
    auto retVal = CreateAndAppendExpression<Allocate>(unitTyRef, builder.GetUnitTy(), entry);
    func->SetReturnValue(*retVal->GetResult());

    return func;
}

Ptr<LocalVar> Translator::CreateGetElementRefWithPath(const DebugLocation& loc, Ptr<Value> lhsBase,
    const std::vector<std::string>& path, Ptr<Block> block, const CustomType& customType)
{
    auto [memberType, isReadOnly] = GetInstMemberTypeByNameCheckingReadOnly(customType, path, builder);
    memberType = builder.SubstituteModal(memberType, customType.Modal());
    auto memberRefType = builder.GetType<RefType>(memberType);
    auto getMemberRef =
        CreateAndAppendExpression<GetElementByName>(loc, memberRefType, lhsBase, path, block)->GetResult();
    if (lhsBase->TestAttr(Attribute::READONLY) || isReadOnly) {
        getMemberRef->EnableAttr(Attribute::READONLY);
    }
    return getMemberRef;
}

Value* Translator::TranslateExprArg(const AST::Node& node, Type& targetTy, bool deref)
{
    auto value = TranslateASTNode(node, *this);
    if (targetTy.IsUnit()) {
        return CreateAndAppendConstantExpression<UnitLiteral>(builder.GetUnitTy(), *GetCurrentBlock())->GetResult();
    }
    if (node.GetTy()->IsNothing() || targetTy.IsNothing()) {
        return CreateAndAppendConstantExpression<NullLiteral>(builder.GetNothingType(),
            *GetCurrentBlock())->GetResult();
    }
    if (!value) {
        // unit can be converted to Any, or any other interface it extends
        if (node.GetTy()->IsUnit() && !targetTy.IsUnit()) {
            value = CreateAndAppendConstantExpression<UnitLiteral>(
                builder.GetUnitTy(), *GetCurrentBlock())->GetResult();
        } else {
            return CreateAndAppendConstantExpression<NullLiteral>(builder.GetNothingType(),
                *GetCurrentBlock())->GetResult();
        }
    }
    if (value->GetType()->IsNothing()) {
        return value;
    }
    if (deref) {
        value = GetDerefedValue(value, TranslateLocation(node));
    }
    value = TypeCastOrBoxIfNeeded(*value, targetTy, TranslateLocation(node));
    return value;
}

Value* Translator::TranslateExprArg(const AST::Node& node)
{
    auto value = TranslateASTNode(node, *this);
    if (!value) {
        if (node.GetTy()->IsUnit()) {
            return CreateAndAppendConstantExpression<UnitLiteral>(builder.GetUnitTy(), *GetCurrentBlock())->GetResult();
        }
        return CreateAndAppendConstantExpression<NullLiteral>(builder.GetNothingType(),
            *GetCurrentBlock())->GetResult();
    }
    return value;
}

void Translator::TranslateSubExprToLoc(const AST::Node& node, Value* location)
{
    if (!location) {
        TranslateASTNode(node, *this);
        return;
    }
    const DebugLocation* debugLoc;
    if (auto l = DynamicCast<LocalVar>(location)) {
        debugLoc = &l->GetExpr()->GetDebugLocation();
    } else {
        debugLoc = &location->GetDebugLocation();
    }
    TranslateSubExprToLoc(node, location, *debugLoc);
}

void Translator::TranslateSubExprToLoc(const AST::Node& node, Value* location, const DebugLocation& debugLoc)
{
    auto value = TranslateASTNode(node, *this);
    /// \ref loc is null if it is of Unit or Nothing type. In this case, no further typecast/store is needed.
    if (!location) {
        return;
    }
    auto targetTy = StaticCast<RefType>(location->GetType())->GetBaseType();
    if (targetTy->IsNothing()) {
        return;
    }
    // unit can be converted to Any, or any other interface it extends
    if (node.GetTy()->IsUnit() && !targetTy->IsUnit()) {
        value = CreateAndAppendConstantExpression<UnitLiteral>(
            debugLoc, builder.GetUnitTy(), *currentBlock)->GetResult();
    }
    // this if never collides with the previous if, so no need to write else if
    if (targetTy->IsUnit() || !value) {
        if (!opts.enableCompileDebug) {
            return;
        }
        // create a null/unit value if -g
        if (!value) {
            if (node.GetTy()->IsUnit()) {
                auto unit = CreateAndAppendConstantExpression<UnitLiteral>(
                    debugLoc, builder.GetUnitTy(), *currentBlock);
                value = unit->GetResult();
            } else {
                /// either \ref node is of Nothing type, of every branch of \ref node is of Nothing type, create a
                /// Nothing value here
                auto null = CreateAndAppendConstantExpression<NullLiteral>(
                    debugLoc, builder.GetNothingType(), *currentBlock);
                null->Set<SkipCheck>(SkipKind::SKIP_DCE_WARNING);
                value = null->GetResult();
            }
        }
    }
    value = GetDerefedValue(value, debugLoc);
    if (value->GetType()->IsNothing()) {
        return;
    }
    value = TypeCastOrBoxIfNeeded(*value, *targetTy, debugLoc);
    CreateAndAppendWrappedStore(*value, *location, debugLoc);
}

Ptr<Value> Translator::TranslateSubExprAsValue(const AST::Node& node)
{
    auto value = TranslateASTNode(node, *this);
    if (value && Is<AST::Expr>(node)) {
        value = GetDerefedValue(value, TranslateLocation(node));
    }
    return value;
}

void Translator::TranslateSubExprToDiscarded(const AST::Node& node)
{
    TranslateASTNode(node, *this);
    if (auto b = DynamicCast<AST::LitConstExpr>(&node)) {
        if (b->kind == AST::LitConstKind::UNIT) {
            auto unit = CreateAndAppendConstantExpression<UnitLiteral>(
                TranslateLocation(node), builder.GetUnitTy(), *currentBlock);
            if (b->TestAttr(AST::Attribute::COMPILER_ADD)) {
                unit->GetResult()->EnableAttr(Attribute::COMPILER_ADD);
            }
        }
    }
}

namespace {
const std::unordered_map<std::string, IntrinsicKind> coreIntrinsicMap = {
    {SIZE_OF_NAME, SIZE_OF},
    {ALIGN_OF_NAME, ALIGN_OF},
    {ARRAY_ACQUIRE_RAW_DATA_NAME, ARRAY_ACQUIRE_RAW_DATA},
    {ARRAY_ACQUIRE_RAW_DATA2_NAME, ARRAY_ACQUIRE_RAW_DATA},
    {ARRAY_RELEASE_RAW_DATA_NAME, ARRAY_RELEASE_RAW_DATA},
    {ARRAY_RELEASE_RAW_DATA2_NAME, ARRAY_RELEASE_RAW_DATA},
    {ARRAY_BUILT_IN_COPY_TO_NAME, ARRAY_BUILT_IN_COPY_TO},
    {"intrinsicBuiltInCopyTo2", ARRAY_BUILT_IN_COPY_TO},
    {"intrinsicBuiltInCopyTo3", ARRAY_BUILT_IN_COPY_TO},
    {ARRAY_GET_NAME, ARRAY_GET},
    {ARRAY_SET_NAME, ARRAY_SET},
    {std::string{ARRAY_SET2_NAME}, ARRAY_SET},
    {std::string{ARRAY_SET3_NAME}, ARRAY_SET},
    {std::string{ARRAY_GET_UNCHECKED_NAME}, ARRAY_GET_UNCHECKED},
    {std::string{ARRAY_GET_UNCHECKED2_NAME}, ARRAY_GET_UNCHECKED},
    {std::string{ARRAY_GET_UNCHECKED3_NAME}, ARRAY_GET_UNCHECKED},
    {"arrayGetUncheckedCopy", ARRAY_GET_UNCHECKED},
    {ARRAY_GET_REF_UNCHECKED_NAME, ARRAY_GET_REF_UNCHECKED},
    {ARRAY_SET_UNCHECKED_NAME, ARRAY_SET_UNCHECKED},
    {"arraySetUnchecked2", ARRAY_SET_UNCHECKED},
    {"arraySetUnchecked3", ARRAY_SET_UNCHECKED},
    {"rawArrayAllocate", RAW_ARRAY_ALLOCATE},
    {"rawArrayAllocate2", RAW_ARRAY_ALLOCATE},
    {"rawArrayAllocate3", RAW_ARRAY_ALLOCATE},
    {"arrayInitByValue", ARRAY_INIT_BY_VALUE},
    {"arrayInitByValue2", ARRAY_INIT_BY_VALUE},
    {"arrayInitByValue3", ARRAY_INIT_BY_VALUE},
    {ARRAY_SIZE_NAME, ARRAY_SIZE},
    {ARRAY_CLONE_NAME, ARRAY_CLONE},
    {ARRAY_SLICE_INIT_NAME, ARRAY_SLICE_INIT},
    {ARRAY_SLICE_NAME, ARRAY_SLICE},
    {ARRAY_SLICE_RAWARRAY_NAME, ARRAY_SLICE_RAWARRAY},
    {ARRAY_SLICE_START_NAME, ARRAY_SLICE_START},
    {ARRAY_SLICE_SIZE_NAME, ARRAY_SLICE_SIZE},
    {ARRAY_SLICE_GET_ELEMENT_NAME, ARRAY_SLICE_GET_ELEMENT},
    {ARRAY_SLICE_SET_ELEMENT_NAME, ARRAY_SLICE_SET_ELEMENT},
    {ARRAY_SLICE_GET_ELEMENT_UNCHECKED_NAME, ARRAY_SLICE_GET_ELEMENT_UNCHECKED},
    {ARRAY_SLICE_SET_ELEMENT_UNCHECKED_NAME, ARRAY_SLICE_SET_ELEMENT_UNCHECKED},
    {VECTOR_COMPARE_32_NAME, VECTOR_COMPARE_32},
    {VECTOR_INDEX_BYTE_32_NAME, VECTOR_INDEX_BYTE_32},
    {FILL_IN_STACK_TRACE_NAME, FILL_IN_STACK_TRACE},
    {DECODE_STACK_TRACE_NAME, DECODE_STACK_TRACE},
    {DUMP_CURRENT_THREAD_INFO_NAME, DUMP_CURRENT_THREAD_INFO},
    {DUMP_ALL_THREADS_INFO_NAME, DUMP_ALL_THREADS_INFO},
    {CPOINTER_GET_POINTER_ADDRESS_NAME, CPOINTER_GET_POINTER_ADDRESS},
    {CPOINTER_READ_NAME, CPOINTER_READ},
    {CPOINTER_WRITE_NAME, CPOINTER_WRITE},
    {CPOINTER_ADD_NAME, CPOINTER_ADD},
    {CPOINTER_ADD2_NAME, CPOINTER_ADD},
    {CSTRING_CONVERT_CSTR_TO_PTR_NAME, CSTRING_CONVERT_CSTR_TO_PTR},
    {BIT_CAST_NAME, BIT_CAST},
    {FUTURE_INIT_NAME, FUTURE_INIT},
    {IS_THREAD_OBJECT_INITED_NAME, IS_THREAD_OBJECT_INITED},
    {GET_THREAD_OBJECT_NAME, GET_THREAD_OBJECT},
    {SET_THREAD_OBJECT_NAME, SET_THREAD_OBJECT},
    {FUTURE_IS_COMPLETE_NAME, FUTURE_IS_COMPLETE},
    {FUTURE_WAIT_NAME, FUTURE_WAIT},
    {FUTURE_NOTIFYALL_NAME, FUTURE_NOTIFYALL},
    {OBJECT_REFEQ_NAME, OBJECT_REFEQ},
    {RAW_ARRAY_REFEQ_NAME, RAW_ARRAY_REFEQ},
    {OBJECT_ZERO_VALUE_NAME, OBJECT_ZERO_VALUE},
    {"zeroValue2", OBJECT_ZERO_VALUE},
    {SOURCE_FILE_NAME, SOURCE_FILE},
    {SOURCE_LINE_NAME, SOURCE_LINE},
    {IDENTITY_HASHCODE_NAME, IDENTITY_HASHCODE},
    {IDENTITY_HASHCODE_FOR_ARRAY_NAME, IDENTITY_HASHCODE_FOR_ARRAY},
    {STRLEN_NAME, STRLEN},
    {MEMCPY_S_NAME, MEMCPY_S},
    {MEMSET_S_NAME, MEMSET_S},
    {FREE_NAME, FREE},
    {MALLOC_NAME, MALLOC},
    {STRCMP_NAME, STRCMP},
    {MEMCMP_NAME, MEMCMP},
    {STRNCMP_NAME, STRNCMP},
    {STRCASECMP_NAME, STRCASECMP},
    {ATOMIC_LOAD_NAME, ATOMIC_LOAD},
    {ATOMIC_STORE_NAME, ATOMIC_STORE},
    {ATOMIC_FETCH_ADD_NAME, ATOMIC_FETCH_ADD},
    {ATOMIC_COMPARE_AND_SWAP_NAME, ATOMIC_COMPARE_AND_SWAP},
    {SLEEP_NAME, SLEEP},
    {GET_TYPE_FOR_TYPE_PARAMETER_NAME, GET_TYPE_FOR_TYPE_PARAMETER},
    {IS_SUBTYPE_TYPES_NAME, IS_SUBTYPE_TYPES},
    {std::string{EXCLUSIVE_SCOPE_NAME}, EXCLUSIVE_SCOPE},
};

const std::unordered_map<std::string, IntrinsicKind> overflowIntrinsicMap = {
    {OVERFLOW_CHECKED_ADD_NAME, OVERFLOW_CHECKED_ADD},
    {OVERFLOW_CHECKED_SUB_NAME, OVERFLOW_CHECKED_SUB},
    {OVERFLOW_CHECKED_MUL_NAME, OVERFLOW_CHECKED_MUL},
    {OVERFLOW_CHECKED_DIV_NAME, OVERFLOW_CHECKED_DIV},
    {OVERFLOW_CHECKED_MOD_NAME, OVERFLOW_CHECKED_MOD},
    {OVERFLOW_CHECKED_POW_NAME, OVERFLOW_CHECKED_POW},
    {OVERFLOW_CHECKED_INC_NAME, OVERFLOW_CHECKED_INC},
    {OVERFLOW_CHECKED_DEC_NAME, OVERFLOW_CHECKED_DEC},
    {OVERFLOW_CHECKED_NEG_NAME, OVERFLOW_CHECKED_NEG},
    {OVERFLOW_THROWING_ADD_NAME, OVERFLOW_THROWING_ADD},
    {OVERFLOW_THROWING_SUB_NAME, OVERFLOW_THROWING_SUB},
    {OVERFLOW_THROWING_MUL_NAME, OVERFLOW_THROWING_MUL},
    {OVERFLOW_THROWING_DIV_NAME, OVERFLOW_THROWING_DIV},
    {OVERFLOW_THROWING_MOD_NAME, OVERFLOW_THROWING_MOD},
    {OVERFLOW_THROWING_POW_NAME, OVERFLOW_THROWING_POW},
    {OVERFLOW_THROWING_INC_NAME, OVERFLOW_THROWING_INC},
    {OVERFLOW_THROWING_DEC_NAME, OVERFLOW_THROWING_DEC},
    {OVERFLOW_THROWING_NEG_NAME, OVERFLOW_THROWING_NEG},
    {OVERFLOW_SATURATING_ADD_NAME, OVERFLOW_SATURATING_ADD},
    {OVERFLOW_SATURATING_SUB_NAME, OVERFLOW_SATURATING_SUB},
    {OVERFLOW_SATURATING_MUL_NAME, OVERFLOW_SATURATING_MUL},
    {OVERFLOW_SATURATING_DIV_NAME, OVERFLOW_SATURATING_DIV},
    {OVERFLOW_SATURATING_MOD_NAME, OVERFLOW_SATURATING_MOD},
    {OVERFLOW_SATURATING_POW_NAME, OVERFLOW_SATURATING_POW},
    {OVERFLOW_SATURATING_INC_NAME, OVERFLOW_SATURATING_INC},
    {OVERFLOW_SATURATING_DEC_NAME, OVERFLOW_SATURATING_DEC},
    {OVERFLOW_SATURATING_NEG_NAME, OVERFLOW_SATURATING_NEG},
    {OVERFLOW_WRAPPING_ADD_NAME, OVERFLOW_WRAPPING_ADD},
    {OVERFLOW_WRAPPING_SUB_NAME, OVERFLOW_WRAPPING_SUB},
    {OVERFLOW_WRAPPING_MUL_NAME, OVERFLOW_WRAPPING_MUL},
    {OVERFLOW_WRAPPING_DIV_NAME, OVERFLOW_WRAPPING_DIV},
    {OVERFLOW_WRAPPING_MOD_NAME, OVERFLOW_WRAPPING_MOD},
    {OVERFLOW_WRAPPING_POW_NAME, OVERFLOW_WRAPPING_POW},
    {OVERFLOW_WRAPPING_INC_NAME, OVERFLOW_WRAPPING_INC},
    {OVERFLOW_WRAPPING_DEC_NAME, OVERFLOW_WRAPPING_DEC},
    {OVERFLOW_WRAPPING_NEG_NAME, OVERFLOW_WRAPPING_NEG},
};

const std::unordered_map<std::string, IntrinsicKind> reflectIntrinsicMap = {
#ifdef CANGJIE_CODEGEN_CJNATIVE_BACKEND
#define REFLECTION_KIND_TO_RUNTIME_FUNCTION(REFLECTION_KIND, CJ_FUNCTION, RUNTIME_FUNCTION)                            \
    {REFLECTION_KIND##_NAME, REFLECTION_KIND},
#include "cangjie/CHIR/Utils/LLVMReflectionIntrinsics.def"
#undef REFLECTION_KIND_TO_RUNTIME_FUNCTION
#endif
};

const std::unordered_map<std::string, IntrinsicKind> interOpIntrinsicMap = {
    {CROSS_ACCESS_BARRIER_NAME, CROSS_ACCESS_BARRIER},
    {CREATE_EXPORT_HANDLE_NAME, CREATE_EXPORT_HANDLE},
    {GET_EXPORTED_REF_NAME, GET_EXPORTED_REF},
    {REMOVE_EXPORTED_REF_NAME, REMOVE_EXPORTED_REF},
};

const std::unordered_map<std::string, IntrinsicKind> cjnativeSyncIntrinsicMap = {
    {ATOMIC_LOAD_NAME, ATOMIC_LOAD},
    {ATOMIC_STORE_NAME, ATOMIC_STORE},
    {ATOMIC_SWAP_NAME, ATOMIC_SWAP},
    {ATOMIC_COMPARE_AND_SWAP_NAME, ATOMIC_COMPARE_AND_SWAP},
    {ATOMIC_FETCH_ADD_NAME, ATOMIC_FETCH_ADD},
    {ATOMIC_FETCH_SUB_NAME, ATOMIC_FETCH_SUB},
    {ATOMIC_FETCH_AND_NAME, ATOMIC_FETCH_AND},
    {ATOMIC_FETCH_OR_NAME, ATOMIC_FETCH_OR},
    {ATOMIC_FETCH_XOR_NAME, ATOMIC_FETCH_XOR},
    {MUTEX_INIT_NAME, MUTEX_INIT},
    {MUTEX_LOCK_NAME, CJ_MUTEX_LOCK},
    {MUTEX_TRY_LOCK_NAME, MUTEX_TRY_LOCK},
    {MUTEX_CHECK_STATUS_NAME, MUTEX_CHECK_STATUS},
    {MUTEX_UNLOCK_NAME, MUTEX_UNLOCK},
    {WAITQUEUE_INIT_NAME, WAITQUEUE_INIT},
    {MONITOR_INIT_NAME, MONITOR_INIT},
    {MOITIOR_WAIT_NAME, MOITIOR_WAIT},
    {MOITIOR_NOTIFY_NAME, MOITIOR_NOTIFY},
    {MOITIOR_NOTIFY_ALL_NAME, MOITIOR_NOTIFY_ALL},
    {MULTICONDITION_WAIT_NAME, MULTICONDITION_WAIT},
    {MULTICONDITION_NOTIFY_NAME, MULTICONDITION_NOTIFY},
    {MULTICONDITION_NOTIFY_ALL_NAME, MULTICONDITION_NOTIFY_ALL},
};

const std::unordered_map<std::string, IntrinsicKind> runtimeIntrinsicMap = {
    {INVOKE_GC_NAME, INVOKE_GC},
    {SET_GC_THRESHOLD_NAME, SET_GC_THRESHOLD},
    {DUMP_CJ_HEAP_DATA_NAME, DUMP_CJ_HEAP_DATA},
    {GET_GC_COUNT_NAME, GET_GC_COUNT},
    {GET_GC_TIME_US_NAME, GET_GC_TIME_US},
    {GET_GC_FREED_SIZE_NAME, GET_GC_FREED_SIZE},
    {START_CJ_CPU_PROFILING_NAME, START_CJ_CPU_PROFILING},
    {STOP_CJ_CPU_PROFILING_NAME, STOP_CJ_CPU_PROFILING},
    {BLACK_BOX_NAME, BLACK_BOX},
    {GET_MAX_HEAP_SIZE_NAME, GET_MAX_HEAP_SIZE},
    {GET_ALLOCATE_HEAP_SIZE_NAME, GET_ALLOCATE_HEAP_SIZE},
    {GET_REAL_HEAP_SIZE_NAME, GET_REAL_HEAP_SIZE},
    {GET_THREAD_NUMBER_NAME, GET_THREAD_NUMBER},
    {GET_BLOCKING_THREAD_NUMBER_NAME, GET_BLOCKING_THREAD_NUMBER},
    {GET_NATIVE_THREAD_NUMBER_NAME, GET_NATIVE_THREAD_NUMBER},
    {CROSS_ACCESS_BARRIER_NAME, CROSS_ACCESS_BARRIER},
    {CREATE_EXPORT_HANDLE_NAME, CREATE_EXPORT_HANDLE},
    {GET_EXPORTED_REF_NAME, GET_EXPORTED_REF},
    {REMOVE_EXPORTED_REF_NAME, REMOVE_EXPORTED_REF},
    {FUNC_REFEQ_NAME, FUNC_REFEQ},
};

const std::unordered_map<std::string, IntrinsicKind> mathIntrinsicMap = {
    {ABS_NAME, ABS},
    {FABS_NAME, FABS},
    {FLOOR_NAME, FLOOR},
    {CEIL_NAME, CEIL},
    {TRUC_NAME, TRUNC},
    {SIN_NAME, SIN},
    {COS_NAME, COS},
    {EXP_NAME, EXP},
    {EXP2_NAME, EXP2},
    {LOG_NAME, LOG},
    {LOG2_NAME, LOG2},
    {LOG10_NAME, LOG10},
    {SQRT_NAME, SQRT},
    {ROUND_NAME, ROUND},
    {POW_NAME, POW},
    {POWI_NAME, POWI},
};
} // namespace

namespace Cangjie::CHIR {
const std::unordered_map<std::string, const std::unordered_map<std::string, IntrinsicKind>> packageMap = {
    {CORE_PACKAGE_NAME, coreIntrinsicMap},
    {SYNC_PACKAGE_NAME, cjnativeSyncIntrinsicMap},
    {OVERFLOW_PACKAGE_NAME, overflowIntrinsicMap},
    {RUNTIME_PACKAGE_NAME, runtimeIntrinsicMap},
    {REFLECT_PACKAGE_NAME, reflectIntrinsicMap},
    {MATH_PACKAGE_NAME, mathIntrinsicMap},
    {INTEROP_PACKAGE_NAME, interOpIntrinsicMap},
};

const std::unordered_map<std::string, IntrinsicKind> headlessIntrinsics = {
    {GET_TYPE_FOR_TYPE_PARAMETER_NAME, IntrinsicKind::GET_TYPE_FOR_TYPE_PARAMETER},
    {IS_SUBTYPE_TYPES_NAME, IntrinsicKind::IS_SUBTYPE_TYPES},
};
} // namespace Cangjie::CHIR
