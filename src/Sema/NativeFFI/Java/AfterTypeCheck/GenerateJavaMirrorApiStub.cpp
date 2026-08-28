// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "GenerateJavaMirrorApiStub.h"
#include "NativeFFI/Utils.h"
#include "InteropLibBridge.h"

#include "cangjie/AST/Create.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"

namespace Cangjie::Native::FFI::Java {

FuncDecl& GenerateJavaMirrorApiStub::InsertWrappingConstructor(ClassDecl& mirror) const
{
    auto ctor = CreateWrappingConstructorStub(mirror);
    auto& res = *ctor;
    ctor->funcBody->parentClassLike = &mirror;
    ctor->fullPackageName = mirror.fullPackageName;
    ctor->outerDecl = &mirror;
    ctor->curFile = mirror.curFile;

    mirror.body->decls.emplace_back(std::move(ctor));
    return res;
}

FuncDecl& GenerateJavaMirrorApiStub::InsertJStringOfStringConstructor(ClassDecl& jstring) const
{
    auto ctor = CreateJStringOfStringConstructorStub(jstring);
    auto& res = *ctor;
    ctor->funcBody->parentClassLike = &jstring;
    ctor->fullPackageName = jstring.fullPackageName;
    ctor->outerDecl = &jstring;
    ctor->curFile = jstring.curFile;
    ctor->begin = jstring.begin;

    jstring.body->decls.emplace_back(std::move(ctor));
    return res;
}

FuncDecl& GenerateJavaMirrorApiStub::InsertAbstractJavaReferenceGetterStub(InterfaceDecl& mirror) const
{
    auto getter = CreateAbstractJavaReferenceGetterStub();
    auto& res = *getter;
    getter->funcBody->parentClassLike = &mirror;
    getter->fullPackageName = mirror.fullPackageName;
    getter->outerDecl = &mirror;
    // Every AST decl must carry a curFile (see CHIR AST2CHIR::CreateFuncSignatureAndSetGlobalCache).
    getter->curFile = mirror.curFile;
    getter->begin = mirror.begin;

    mirror.body->decls.emplace_back(std::move(getter));
    return res;
}

OwnedPtr<FuncDecl> GenerateJavaMirrorApiStub::CreateWrappingConstructorStub(ClassDecl& mirror) const
{
    auto curFile = mirror.curFile;
    auto isJObject = IsJObject(mirror);
    static auto& javaEntityDecl = *ilib.GetJavaEntityDecl();

    auto param = CreateFuncParam("$ref", CreateRefType(javaEntityDecl), nullptr, javaEntityDecl.GetTy());

    std::vector<OwnedPtr<Node>> ctorNodes;

    if (isJObject) {
        // For JObject, body for wrapping constructor is generated at this stage.
        auto lhsRef = WithinFile(CreateRefExpr(*GetJavaRefField(mirror)), curFile);
        auto rhs = WithinFile(CreateRefExpr(*param), curFile);

        auto unitTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
        auto refAssignment = CreateAssignExpr(std::move(lhsRef), std::move(rhs), unitTy);
        ctorNodes.push_back(ilib.CreateEnsureNotNullCall(WithinFile(CreateRefExpr(*param), curFile)));
        ctorNodes.push_back(std::move(refAssignment));
    }

    std::vector<Ptr<Ty>> ctorFuncParamTys;
    ctorFuncParamTys.push_back(param->GetTy());
    auto ctorFuncTy = typeManager.GetFunctionTy(std::move(ctorFuncParamTys), mirror.GetTy());

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.push_back(std::move(param));
    auto paramList = CreateFuncParamList(std::move(ctorParams));
    paramList->curFile = curFile;

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.push_back(std::move(paramList));

    auto ctorFuncBody = CreateFuncBody(std::move(paramLists),
        CreateRefType(mirror), CreateBlock(std::move(ctorNodes), mirror.GetTy()), mirror.GetTy());

    auto fd = CreateFuncDecl("init", std::move(ctorFuncBody), ctorFuncTy);
    fd->funcBody->funcDecl = fd.get();
    fd->constructorCall = ConstructorCall::SUPER;
    fd->EnableAttr(Attribute::PUBLIC, Attribute::IN_CLASSLIKE, Attribute::CONSTRUCTOR);

    return fd;
}

OwnedPtr<FuncDecl> GenerateJavaMirrorApiStub::CreateJStringOfStringConstructorStub(ClassDecl& jstring) const
{
    static const std::string stringParamName = "s";

    CJC_ASSERT(&jstring == utils.GetJStringDecl());

    // Initially, it inserts constructor stub
    auto& stringDecl = utils.GetStringDecl();
    auto param = CreateFuncParam(stringParamName, CreateRefType(stringDecl), nullptr, stringDecl.GetTy());

    auto ctorFuncTy = typeManager.GetFunctionTy({param->GetTy()}, jstring.GetTy());

    std::vector<OwnedPtr<FuncParam>> ctorParams;
    ctorParams.emplace_back(std::move(param));

    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(CreateFuncParamList(std::move(ctorParams)));

    auto ctorFuncBody = CreateFuncBody(std::move(paramLists),
        CreateRefType(jstring), CreateBlock({}, jstring.GetTy()), jstring.GetTy());

    auto fd = CreateFuncDecl("init", std::move(ctorFuncBody), ctorFuncTy);
    fd->funcBody->funcDecl = fd.get();
    fd->constructorCall = ConstructorCall::SUPER;
    fd->EnableAttr(Attribute::PUBLIC, Attribute::CONSTRUCTOR, Attribute::IN_CLASSLIKE);

    return fd;
}

namespace {
/**
 * @brief Generates a synthetic function stub based on an existing function declaration.
 *
 * This function creates a clone of the provided function declaration (fd),
 * replaces its outerDecl to synthetic class, and then inserts the
 * modified function declaration into the specified synthetic class declaration.
 *
 * @param synthetic The class declaration where the cloned function stub will be inserted.
 * @param fd The original function declaration that will be cloned and modified.
 */
void GenerateSyntheticClassFuncStub(ClassDecl& synthetic, FuncDecl& fd)
{
    OwnedPtr<FuncDecl> funcStub = ASTCloner::Clone(Ptr(&fd));
    funcStub->DisableAttr(Attribute::DEFAULT);

    // remove foreign anno from cloned func decl
    for (auto it = funcStub->annotations.begin(); it != funcStub->annotations.end(); ++it) {
        if ((*it)->kind == AnnotationKind::FOREIGN_NAME) {
            funcStub->annotations.erase(it);
            break;
        }
    }

    RebindClonedStubToSynthetic(*funcStub, synthetic);
    synthetic.body->decls.emplace_back(std::move(funcStub));
}
} // namespace

void GenerateJavaMirrorApiStub::InsertSyntheticAbstractWrapperMethodStubs(ClassDecl& wrapper) const
{
    auto members = memberMap.at(&wrapper);
    for (const auto& idMemberSignature : members) {
        const auto& signature = idMemberSignature.second;

        // only abstract functions must be inside synthetic class
        if (!signature.decl->TestAttr(Attribute::ABSTRACT)) {
            continue;
        }

        // JObject already has implementation of java ref getter
        if (Interop::Java::IsJavaRefGetter(*signature.decl)) {
            continue;
        }

        switch (signature.decl->astKind) {
            case ASTKind::FUNC_DECL:
                GenerateSyntheticClassFuncStub(wrapper, *StaticAs<ASTKind::FUNC_DECL>(signature.decl));
                break;
            default:
                continue;
        }
    }
}

OwnedPtr<FuncDecl> GenerateJavaMirrorApiStub::CreateAbstractJavaReferenceGetterStub() const
{
    static auto& javaEntityDecl = *ilib.GetJavaEntityDecl();
    std::vector<OwnedPtr<FuncParam>> callParams;
    std::vector<OwnedPtr<FuncParamList>> paramLists;
    paramLists.emplace_back(CreateFuncParamList(std::move(callParams)));

    auto funcBody = CreateFuncBody(
        std::move(paramLists),
        CreateRefType(javaEntityDecl),
        nullptr,
        TypeManager::GetNothingTy());

    static Ptr<FuncTy> funcTy = typeManager.GetFunctionTy({}, javaEntityDecl.GetTy());

    auto fd = CreateFuncDecl(JAVA_REF_GETTER_FUNC_NAME, std::move(funcBody), funcTy);
    fd->EnableAttr(Attribute::PUBLIC, Attribute::IN_CLASSLIKE, Attribute::ABSTRACT);
    fd->funcBody->funcDecl = fd.get();

    return fd;
}

GenerateJavaMirrorApiStub::GenerateJavaMirrorApiStub(TypeManager& typeManager,
    InteropLibBridge& ilib,
    Interop::Java::Utils& utils,
    const std::unordered_map<Ptr<const AST::InheritableDecl>, MemberMap>& memberMap) : typeManager(typeManager),
    ilib(ilib), utils(utils), memberMap(memberMap)
{
}

void GenerateJavaMirrorApiStub::Process(AfterTypeCheckContext& ctx)
{
    for (auto mirror : ctx.GetJavaMirrors()) {
        if (mirror->TestAttr(Attribute::IS_BROKEN)) {
            continue;
        }

        if (auto mirrorClass = As<ASTKind::CLASS_DECL>(mirror)) {
            InsertWrappingConstructor(*mirrorClass);

            if (mirrorClass == utils.GetJStringDecl()) {
                InsertJStringOfStringConstructor(*mirrorClass);
            }
            if (IsSyntheticMirrorWrapper(*mirrorClass)) {
                InsertSyntheticAbstractWrapperMethodStubs(*mirrorClass);
            }
        } else if (auto mirrorInterface = As<ASTKind::INTERFACE_DECL>(mirror)) {
            InsertAbstractJavaReferenceGetterStub(*mirrorInterface);
        }
    }
}

} // namespace Cangjie::Native::FFI::Java
