// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements the CHIRContext class in CHIR.
 */

#include "cangjie/CHIR/IR/CHIRContext.h"

#include <thread>
#include "cangjie/Basic/Print.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/IR/Type/ClassDef.h"
#include "cangjie/CHIR/IR/Type/EnumDef.h"
#include "cangjie/CHIR/IR/Type/ExtendDef.h"
#include "cangjie/CHIR/IR/Type/StructDef.h"
#include "cangjie/CHIR/IR/Value/Value.h"

using namespace Cangjie::CHIR;

namespace {
const int ALLOCATED_VALUES_START_IDX = 0;
const int ALLOCATED_VALUES_END_IDX = 1;
const int ALLOCATED_STRUCTS_START_IDX = 2;
const int ALLOCATED_STRUCTS_END_IDX = 3;
const int ALLOCATED_CLASSES_START_IDX = 4;
const int ALLOCATED_CLASSES_END_IDX = 5;
const int ALLOCATED_ENUMS_START_IDX = 6;
const int ALLOCATED_ENUMS_END_IDX = 7;
}

std::mutex CHIRContext::dynamicAllocatedTysMtx;
size_t TypePtrHash::operator()(const Type* ptr) const
{
    return ptr != nullptr ? ptr->Hash() : 0;
}

bool TypePtrEqual::operator()(const Type* ptr1, const Type* ptr2) const
{
    if (ptr1 == nullptr && ptr2 == nullptr) {
        return true;
    }
    return ptr1 != nullptr && ptr2 != nullptr && *ptr1 == *ptr2;
}

void CHIRContext::DeleteAllocatedInstance(std::vector<size_t>& idxs)
{
    // Delete the allocated instances.
    for (size_t i = idxs[ALLOCATED_VALUES_START_IDX]; i < idxs[ALLOCATED_VALUES_END_IDX]; i++) {
        SafeDelete(allocatedValues[i]);
    }
    for (size_t i = idxs[ALLOCATED_STRUCTS_START_IDX]; i < idxs[ALLOCATED_STRUCTS_END_IDX]; i++) {
        SafeDelete(allocatedStructs[i]);
    }
    for (size_t i = idxs[ALLOCATED_CLASSES_START_IDX]; i < idxs[ALLOCATED_CLASSES_END_IDX]; i++) {
        SafeDelete(allocatedClasses[i]);
    }
    for (size_t i = idxs[ALLOCATED_ENUMS_START_IDX]; i < idxs[ALLOCATED_ENUMS_END_IDX]; i++) {
        SafeDelete(allocatedEnums[i]);
    }
}

void CHIRContext::DeleteAllocatedTys()
{
    for (auto inst : std::as_const(this->dynamicAllocatedTys)) {
        SafeDelete(inst);
    }
    this->dynamicAllocatedTys.clear();

    for (auto inst : std::as_const(this->constAllocatedTys)) {
        SafeDelete(inst);
    }
    this->constAllocatedTys.clear();

    for (auto inst : std::as_const(this->allocatedExtends)) {
        SafeDelete(inst);
    }
    this->allocatedExtends.clear();

    for (auto [bg, ptrs] : std::as_const(this->allocatedPtrInFuncOrLambda)) {
        for (auto ptr : ptrs) {
            SafeDelete(ptr);
        }
        SafeDelete(bg);
    }
    this->allocatedPtrInFuncOrLambda.clear();

    if (this->curPackage != nullptr) {
        SafeDelete(this->curPackage);
        this->curPackage = nullptr;
    }
}

Package* CHIRContext::GetCurPackage() const
{
    return this->curPackage;
}

void CHIRContext::SetCurPackage(Package* pkg)
{
    this->curPackage = pkg;
}

// Tasks are evenly distributed to obtain the start and end subscripts of the data to be processed by each thread.
static void DivideArray(size_t len, size_t threadNum, std::vector<std::vector<size_t>>& indexs)
{
    size_t size = len / threadNum;
    size_t remainder = len % threadNum;
    size_t start = 0;
    size_t end = 0;
    for (size_t i = 0; i < threadNum; i++) {
        start = end;
        end = start + size;
        if (remainder > 0) {
            end++;
            remainder--;
        }
        indexs[i].push_back(start);
        indexs[i].push_back(end);
    }
    return;
}

CHIRContext::CHIRContext(std::unordered_map<unsigned int, std::string>* fnMap, size_t threadsNum)
    : curPackage(nullptr), fileNameMap(fnMap), threadsNum(threadsNum)
{
    unitTy = GetType<UnitType>();
    boolTy = GetType<BooleanType>();
    runeTy = GetType<RuneType>();
    nothingTy = GetType<NothingType>();
    int8Ty = GetType<IntType>(Type::TypeKind::TYPE_INT8);
    int16Ty = GetType<IntType>(Type::TypeKind::TYPE_INT16);
    int32Ty = GetType<IntType>(Type::TypeKind::TYPE_INT32);
    int64Ty = GetType<IntType>(Type::TypeKind::TYPE_INT64);
    intNativeTy = GetType<IntType>(Type::TypeKind::TYPE_INT_NATIVE);
    uint8Ty = GetType<IntType>(Type::TypeKind::TYPE_UINT8);
    uint16Ty = GetType<IntType>(Type::TypeKind::TYPE_UINT16);
    uint32Ty = GetType<IntType>(Type::TypeKind::TYPE_UINT32);
    uint64Ty = GetType<IntType>(Type::TypeKind::TYPE_UINT64);
    uintNativeTy = GetType<IntType>(Type::TypeKind::TYPE_UINT_NATIVE);
    float16Ty = GetType<FloatType>(Type::TypeKind::TYPE_FLOAT16);
    float32Ty = GetType<FloatType>(Type::TypeKind::TYPE_FLOAT32);
    float64Ty = GetType<FloatType>(Type::TypeKind::TYPE_FLOAT64);
    cstringTy = GetType<CStringType>();
    voidTy = GetType<VoidType>();
}
 
CHIRContext::~CHIRContext()
{
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        FreeWholePackage();
#ifndef CANGJIE_ENABLE_GCOV
    } catch (...) {
        // Never propagate from destructor (would call std::terminate). Per-pointer cleanup uses SafeDelete to limit
        // leaks when a member destructor throws; this catches any other failure (e.g. resource exhaustion).
    }
#endif
}

// FileName API
void CHIRContext::RegisterSourceFileName(unsigned fileId, const std::string& fileName) const
{
    // we need to insert or assign, because this `fileNameMap` may be set in deserialization when
    // we are compiling specific package, so this old `fileNameMap` is from common package,
    // it's not guaranteed that common package's file order and size are same with specific's
    fileNameMap->insert_or_assign(fileId, fileName);
}

const std::string& CHIRContext::GetSourceFileName(unsigned fileId) const
{
    if (auto it = this->fileNameMap->find(fileId); it != this->fileNameMap->end()) {
        return it->second;
    }
    return INVALID_NAME;
}

const std::unordered_map<unsigned int, std::string>* CHIRContext::GetFileNameMap() const
{
    return this->fileNameMap;
}

StructType* CHIRContext::GetStructType(
    const std::string& package, const std::string& name, const std::vector<std::string>& genericType) const
{
    std::vector<StructDef*> structs = this->curPackage->GetStructs();
    std::vector<StructDef*> importStructs = this->curPackage->GetImportedStructs();
    structs.insert(structs.end(), importStructs.cbegin(), importStructs.cend());
    for (auto it : structs) {
        if (it->GetPackageName() != package || it->GetSrcCodeIdentifier() != name) {
            continue;
        }
        if (it->TestAttr(Attribute::GENERIC_INSTANTIATED)) {
            continue;
        }
        auto structType = StaticCast<StructType*>(it->GetType());
        auto argTypes = structType->GetGenericArgs();
        if (std::equal(genericType.begin(), genericType.end(), argTypes.begin(),
                       [](const std::string& a, const Type* b) { return a == b->ToString(); })) {
            return structType;
        }
    }
    return nullptr;
}

void CHIRContext::MergeTypes()
{
    this->constAllocatedTys.merge(this->dynamicAllocatedTys);
}

StructType* CHIRContext::GetStringTy() const
{
    return GetStructType("std.core", "String");
}

namespace {
using Cangjie::StaticCast;

Type* WithModalImpl(CHIRContext& ctx, Type* ty, ModalInfo modal)
{
    if (modal == ty->GetModalInfo()) {
        return ty;
    }
    if (ty->IsRef()) {
        return ctx.GetType<RefType>(WithModalImpl(ctx, StaticCast<RefType*>(ty)->GetBaseType(), modal));
    }
    switch (ty->GetTypeKind()) {
        case Type::TYPE_INT8:
        case Type::TYPE_INT16:
        case Type::TYPE_INT32:
        case Type::TYPE_INT64:
        case Type::TYPE_INT_NATIVE:
        case Type::TYPE_UINT8:
        case Type::TYPE_UINT16:
        case Type::TYPE_UINT32:
        case Type::TYPE_UINT64:
        case Type::TYPE_UINT_NATIVE:
            return ctx.GetType<IntType>(ty->GetTypeKind(), modal);
        case Type::TYPE_FLOAT16:
        case Type::TYPE_FLOAT32:
        case Type::TYPE_FLOAT64:
            return ctx.GetType<FloatType>(ty->GetTypeKind(), modal);
        case Type::TYPE_RUNE:
            return ctx.GetType<RuneType>(modal);
        case Type::TYPE_BOOLEAN:
            return ctx.GetType<BooleanType>(modal);
        case Type::TYPE_UNIT:
            return ctx.GetType<UnitType>(modal);
        case Type::TYPE_NOTHING:
            return ctx.GetType<NothingType>(modal);
        case Type::TYPE_VOID:
            return ctx.GetType<VoidType>(modal);
        case Type::TYPE_CSTRING:
            return ctx.GetType<CStringType>(modal);
        case Type::TYPE_TUPLE:
            return ctx.GetType<TupleType>(StaticCast<TupleType*>(ty)->GetElementTypes(), modal);
        case Type::TYPE_FUNC: {
            auto funcTy = StaticCast<FuncType*>(ty);
            return ctx.GetType<FuncType>(
                funcTy->GetParamTypes(), funcTy->GetReturnType(), funcTy->HasVarArg(), funcTy->IsCFunc(), modal);
        }
        case Type::TYPE_STRUCT:
            return ctx.GetType<StructType>(StaticCast<StructType*>(ty)->GetStructDef(), ty->GetTypeArgs(), modal);
        case Type::TYPE_ENUM:
            return ctx.GetType<EnumType>(StaticCast<EnumType*>(ty)->GetEnumDef(), ty->GetTypeArgs(), modal);
        case Type::TYPE_CLASS:
            return ctx.GetType<ClassType>(StaticCast<ClassType*>(ty)->GetClassDef(), ty->GetTypeArgs(), modal);
        case Type::TYPE_RAWARRAY:
            return ctx.GetType<RawArrayType>(
                StaticCast<RawArrayType*>(ty)->GetElementType(), StaticCast<RawArrayType*>(ty)->GetDims(), modal);
        case Type::TYPE_VARRAY:
            return ctx.GetType<VArrayType>(
                StaticCast<VArrayType*>(ty)->GetElementType(), StaticCast<VArrayType*>(ty)->GetSize(), modal);
        case Type::TYPE_CPOINTER:
            return ctx.GetType<CPointerType>(StaticCast<CPointerType*>(ty)->GetElementType(), modal);
        case Type::TYPE_GENERIC: {
            auto genericTy = StaticCast<GenericType*>(ty);
            return ctx.GetType<GenericType>(genericTy->GetIdentifier(), genericTy->GetSrcCodeIdentifier(), modal);
        }
        case Type::TYPE_REFTYPE:
        case Type::TYPE_BOXTYPE:
        case Type::TYPE_THIS:
        case Type::TYPE_INVALID:
        case Type::MAX_TYPE_KIND:
        default:
            CJC_ABORT();
            return ty;
    }
}
} // namespace

Type* CHIRContext::WithModal(Type* ty, ModalInfo modal)
{
    return WithModalImpl(*this, ty, modal);
}

Type* CHIRContext::ToSelectorType(Type::TypeKind kind) const
{
    switch (kind) {
        case Type::TypeKind::TYPE_UINT32:
            return GetUInt32Ty();
        default:
            return GetBoolTy();
    }
}

void CHIRContext::MergeAllocatedPtrInFuncOrLambda(std::unordered_map<BlockGroup*, std::vector<Base*>>& input)
{
    for (auto& [bg, ptrs] : input) {
        auto it = allocatedPtrInFuncOrLambda.find(bg);
        if (it == allocatedPtrInFuncOrLambda.end()) {
            allocatedPtrInFuncOrLambda.emplace(bg, ptrs);
        } else {
            it->second.insert(it->second.end(), ptrs.begin(), ptrs.end());
        }
    }
}

void CHIRContext::FreeMemoryInFunc(BlockGroup& funcBody)
{
    // don't erase block group ptr from allocatedPtrInFuncOrLambda, because this api supports multi-threaded operation.
    auto it = allocatedPtrInFuncOrLambda.find(&funcBody);
    if (it != allocatedPtrInFuncOrLambda.end()) {
        for (auto ptr : it->second) {
            SafeDelete(ptr);
        }
        it->second.clear();
    }
}

void CHIRContext::FreeWholePackage()
{
    if (threadsNum == 1) {
        std::vector<size_t> indexs{0, allocatedValues.size(), 0, allocatedStructs.size(), 0, allocatedClasses.size(), 0,
            allocatedEnums.size()};
        DeleteAllocatedInstance(indexs);
        DeleteAllocatedTys();
    } else {
        // Delete the allocated instances.
        std::vector<std::thread> threads;
        threads.reserve(threadsNum);
        std::vector<std::vector<size_t>> indexs(threadsNum, std::vector<size_t>());
        DivideArray(allocatedValues.size(), threadsNum - 1, indexs);
        DivideArray(allocatedStructs.size(), threadsNum - 1, indexs);
        DivideArray(allocatedClasses.size(), threadsNum - 1, indexs);
        DivideArray(allocatedEnums.size(), threadsNum - 1, indexs);
        for (size_t i = 0; i < threadsNum - 1; i++) {
            threads.emplace_back([i, &indexs, this]() {
#ifndef CANGJIE_ENABLE_GCOV
                try {
#endif
                    DeleteAllocatedInstance(indexs[i]);
#ifndef CANGJIE_ENABLE_GCOV
                } catch (...) {
                    // Avoid std::terminate if an unexpected exception escapes the worker.
                }
#endif
            });
        }
        threads.emplace_back([this]() {
#ifndef CANGJIE_ENABLE_GCOV
            try {
#endif
                DeleteAllocatedTys();
#ifndef CANGJIE_ENABLE_GCOV
            } catch (...) {
                // Avoid std::terminate if an unexpected exception escapes the worker.
            }
#endif
        });
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    allocatedValues.clear();
    allocatedStructs.clear();
    allocatedClasses.clear();
    allocatedEnums.clear();
}