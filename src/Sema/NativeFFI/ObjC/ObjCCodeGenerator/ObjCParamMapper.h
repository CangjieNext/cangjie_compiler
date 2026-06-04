// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares class for Objective-C code generation.
 */

#ifndef CANGJIE_SEMA_OBJC_PARAM_MAPPER_H
#define CANGJIE_SEMA_OBJC_PARAM_MAPPER_H

#include <fstream>
#include <string_view>
#include <unordered_set>

#include "NativeFFI/ObjC/AfterTypeCheck/Interop/Context.h"
#include "NativeFFI/ObjC/Utils/Handler.h"
#include "cangjie/AST/Match.h"
#include "cangjie/AST/Types.h"
#include "Utils.h"
//#include "ObjCGenerator.h"

enum class ObjCFunctionType { STATIC, INSTANCE };
enum class GenerationTarget { HEADER, SOURCE, BOTH };
enum class FunctionListFormat { DECLARATION, STATIC_REF, CANGJIE_DECL };
enum class OptionalBlockOp { OPEN, CLOSE, NONE };

namespace Cangjie::Interop::ObjC {

using ArgsList = std::vector<std::pair<std::string, std::string>>;

class ObjCParamMapper {
public:
    explicit ObjCParamMapper(InteropContext& ctx, Ptr<AST::Decl> declArg, Native::FFI::GenericConfigInfo* genericConfig);


    ArgsList ConvertParamsListToArgsList(
        const std::vector<OwnedPtr<AST::FuncParamList>>& paramLists, bool withRegistryId);
    std::string ConvertParamsListToArgsListToString(
        const std::vector<OwnedPtr<AST::FuncParamList>>& paramLists, bool withRegistryId);
    std::vector<std::string> ConvertParamsListToCallableParamsString(
        std::vector<OwnedPtr<AST::FuncParamList>>& paramLists, bool withSelf) const;
    std::string GenerateSetterParamLists(const std::string& type) const;

    std::string GenerateFuncParamLists(const std::vector<OwnedPtr<AST::FuncParamList>>& paramLists,
        const std::vector<std::string>& selectorComponents,
        FunctionListFormat format = FunctionListFormat::DECLARATION,
        const ObjCFunctionType type = ObjCFunctionType::INSTANCE,
        bool hasForeignNameAnno = true);
    std::string GenerateArgumentCast(const AST::Ty& retTy, std::string value) const;
    std::string MapCJTypeToObjCType(const AST::Ty& ty);
    std::string MapCJTypeToObjCType(const OwnedPtr<AST::Type>& type);
    std::string MapCJTypeToObjCType(const OwnedPtr<AST::FuncParam>& param);
    std::unordered_set<std::string> typedefs;

private:
    InteropContext& ctx;
    Ptr<AST::Decl> decl;
    Native::FFI::GenericConfigInfo* genericConfig = nullptr;
};

} // namespace Cangjie::Interop::ObjC

#endif // CANGJIE_SEMA_OBJC_PARAM_MAPPER_H
