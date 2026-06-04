// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements class for objc code generation.
 */

#include "ObjCParamMapper.h"
#include "ObjCGenerator.h"
#include "NativeFFI/ObjC/Utils/ASTFactory.h"
#include "NativeFFI/ObjC/Utils/Common.h"
#include "NativeFFI/Utils.h"
#include "cangjie/Sema/TypeManager.h"
#include "cangjie/Utils/FileUtil.h"
#include "Utils.h"
#include <iostream>
#include <set>

namespace {
template <typename T>
std::string JoinVec(const std::vector<T>& vec, std::function<std::string(const T&)> trans,
    const std::string& sep = ", ", const std::string& pre = "", const std::string& suf = "", bool force = true)
{
    std::stringstream ss;
    auto it = vec.cbegin();
    if (it == vec.end()) {
        if (!force) {
            return "";
        }
        ss << pre << suf;
        return ss.str();
    }
    ss << pre;
    do {
        ss << trans(*it);
        ++it;
        if (it != vec.cend()) {
            ss << sep;
        }
    } while (it != vec.cend());
    ss << suf;
    return ss.str();
}
} // namespace

namespace Cangjie::Interop::ObjC {

using namespace Cangjie;
using namespace AST;
using namespace Native::FFI;
using std::string;

ObjCParamMapper::ObjCParamMapper(InteropContext& ctx, Ptr<AST::Decl> declArg, Native::FFI::GenericConfigInfo* genericConfig)
    : ctx(ctx),
      decl(declArg),
      genericConfig(genericConfig) {}


/*
 *  DECLARATION: :(arg1Type)arg1:(arg2Type)arg2:...(argNType)argN
 *  STATIC_REF: (arg1Type, arg2Type, ...argNType)
 */
std::string ObjCParamMapper::GenerateFuncParamLists(const std::vector<OwnedPtr<FuncParamList>>& paramLists,
    const std::vector<std::string>& selectorComponents, FunctionListFormat format, const ObjCFunctionType type,
    bool hasForeignNameAnno)
{
    auto componentIterator = std::begin(selectorComponents);
    // skip function name
    componentIterator++;
    std::string genParams = format == FunctionListFormat::DECLARATION ? "" : "(";
    if (paramLists.empty() || !paramLists[0]) {
        return "";
    }
    for (size_t i = 0; i < paramLists[0]->params.size(); i++) {
        OwnedPtr<FuncParam>& cur = paramLists[0]->params[i];
        switch (format) {
            case FunctionListFormat::DECLARATION:
                if (i != 0) {
                    /*
                     * for CJMapping Objective-C method signature, label shouldn't generate if not use @ForeignName
                     * as it would be more user-friendly to keep both side method signatures the same
                     */
                    auto name = hasForeignNameAnno ? *componentIterator++ : "";
                    genParams += name + ":"; // label
                } else {
                    genParams += ":";
                }
                genParams += "(" + MapCJTypeToObjCType(*cur->GetTy()) + ")";
                genParams += cur->identifier.Val();
                if (i != paramLists[0]->params.size() - 1) {
                    genParams += " ";
                }
                break;
            case FunctionListFormat::STATIC_REF:
                if (paramLists[0]->params.size() == 0 && type != ObjCFunctionType::STATIC) {
                    genParams += Emitter::VOID_POINTER_TYPE;
                }
                genParams += ")";
                break;
            case FunctionListFormat::CANGJIE_DECL:
                genParams += cur->identifier.Val() + ": " + Ty::ToString(cur->type->GetTy());
                if (i != paramLists[0]->params.size() - 1) {
                    genParams += ", ";
                }
                break;
            default:
                break;
        }
    }
    if (format == FunctionListFormat::STATIC_REF || format == FunctionListFormat::CANGJIE_DECL) {
        if (paramLists[0]->params.size() == 0 && type != ObjCFunctionType::STATIC) {
            genParams += Emitter::VOID_POINTER_TYPE;
        }
        genParams += ")";
    }

    return genParams;
}

ArgsList ObjCParamMapper::ConvertParamsListToArgsList(
    const std::vector<OwnedPtr<FuncParamList>>& paramLists, bool withRegistryId)
{
    ArgsList result = ArgsList();

    if (withRegistryId) {
        result.emplace_back(std::pair<std::string, std::string>(
            Emitter::INT64_T, string(Emitter::SELF_NAME) + "." + Emitter::SELF_WEAKLINK_NAME)
        );
    }

    if (!paramLists.empty() && paramLists[0]) {
        for (size_t i = 0; i < paramLists[0]->params.size(); i++) {
            OwnedPtr<FuncParam>& cur = paramLists[0]->params[i];
            auto name = cur->identifier.Val();
            name = GenerateArgumentCast(*cur->GetTy(), std::move(name));
            result.push_back(std::pair<std::string, std::string>(MapCJTypeToObjCType(cur), name));
        }
    }
    return result;
}

std::string ObjCParamMapper::ConvertParamsListToArgsListToString(const std::vector<OwnedPtr<AST::FuncParamList>>& paramLists, bool withRegistryId) {
    std::string result = "";

    if (withRegistryId) {
        result = "self.$registryId";
    }

    if (!paramLists.empty() && paramLists[0]) {
        if (withRegistryId && paramLists[0]->params.size() != 0) {
            result += ", ";
        }
        for (size_t i = 0; i < paramLists[0]->params.size(); i++) {
            OwnedPtr<FuncParam>& cur = paramLists[0]->params[i];
            auto name = cur->identifier.Val();
            name = GenerateArgumentCast(*cur->GetTy(), std::move(name));
            result += name;
            if (i != paramLists[0]->params.size() - 1) {
                result += ", ";
            }
        }
    }
    return result;
}

std::vector<std::string> ObjCParamMapper::ConvertParamsListToCallableParamsString(
    std::vector<OwnedPtr<FuncParamList>>& paramLists, bool withSelf) const
{
    std::vector<string> result = {};

    if (withSelf) {
        result.emplace_back(std::string(Emitter::CAST_TO_VOID_PTR) + Emitter::SELF_NAME);
    }

    if (!paramLists.empty() && paramLists[0]) {
        for (size_t i = 0; i < paramLists[0]->params.size(); i++) {
            OwnedPtr<FuncParam>& cur = paramLists[0]->params[i];
            std::string name = GenerateArgumentCast(*cur->GetTy(), cur->identifier.Val());
            result.push_back(std::move(name));
        }
    }
    return result;
}

std::string ObjCParamMapper::MapCJTypeToObjCType(const Ty& ty)
{
    auto objctype = ctx.typeMapper.Cj2ObjCForObjC(ty);
    if (objctype.decl != "") {
        typedefs.insert(objctype.decl);
    }
    return objctype.usage;
}

std::string ObjCParamMapper::MapCJTypeToObjCType(const OwnedPtr<Type>& type)
{
    if (!type) {
        return Emitter::UNSUPPORTED_TYPE;
    }

    if (IsGenericParam(type->GetTy(), *decl, genericConfig)) {
        // Current generic only support primitive type.
        auto genericActualTy =
            TypeManager::GetPrimitiveTy(GetActualTypeKind(GetGenericActualType(genericConfig, type->GetTy()->name)));
        return MapCJTypeToObjCType(*genericActualTy);
    }
    return MapCJTypeToObjCType(*type->GetTy());
}

std::string ObjCParamMapper::MapCJTypeToObjCType(const OwnedPtr<FuncParam>& param)
{
    if (!param) {
        return Emitter::UNSUPPORTED_TYPE;
    }

    auto paraTy = param->type->GetTy();
    if (IsGenericParam(paraTy, *decl, genericConfig)) {
        // Current generic only support primitive type.
        auto genericActualTy =
            TypeManager::GetPrimitiveTy(GetActualTypeKind(GetGenericActualType(genericConfig, paraTy->name)));
        return MapCJTypeToObjCType(*genericActualTy);
    }
    return MapCJTypeToObjCType(*param->type->GetTy());
}

std::string ObjCParamMapper::GenerateArgumentCast(const Ty& retTy, std::string value) const
{
    if (ctx.typeMapper.IsObjCCJMapping(retTy)) {
        return value + "." + Emitter::SELF_WEAKLINK_NAME;
    }
    const auto& actualTy = retTy.IsCoreOptionType() ? *retTy.typeArgs[0] : retTy;
    if (ctx.typeMapper.IsObjCImpl(actualTy)) {
        return Emitter::CAST_TO_VOID_PTR + std::move(value);
    }
    if (ctx.typeMapper.IsObjCMirror(actualTy) || ctx.typeMapper.IsObjCBlock(actualTy) ||
        ctx.typeMapper.IsObjCCJMappingInterface(actualTy)) {
        return Emitter::CAST_TO_VOID_PTR_RETAINED + std::move(value);
    }
    if (ctx.typeMapper.IsObjCPointer(actualTy)) {
        return Emitter::CAST_TO_VOID_PTR_UNSAFE + std::move(value);
    }
    return value;
}
} // namespace Cangjie::Interop::ObjC
