#include "ExclusiveScope.h"
#include "cangjie/AST/Create.h"
#include "cangjie/AST/Types.h"
#include "cangjie/AST/Utils.h"
#include "TypeCheckUtil.h"
#include "NativeFFI/Utils.h"

namespace Cangjie::Native::FFI::Java {

using namespace AST;

OwnedPtr<Expr> WrapWithExclusiveScope(
    AfterTypeCheckContext& ctx,
    OwnedPtr<Block> body,
    Ptr<Ty> retTy)
{
    auto exclusiveDecl = ctx.importManager.GetCoreDecl<FuncDecl>("exclusiveScope");
    CJC_NULLPTR_CHECK(exclusiveDecl);
    auto exclusiveRef = CreateRefExpr(*exclusiveDecl);

    exclusiveRef->instTys.emplace_back(retTy);
    auto mapping = TypeCheckUtil::GenerateTypeMapping(*exclusiveDecl, exclusiveRef->instTys);
    exclusiveRef->SetTy(ctx.typeManager.GetInstantiatedTy(exclusiveDecl->GetTy(), mapping));
    exclusiveRef->typeArguments.emplace_back(CreateType(retTy));

    CJC_NULLPTR_CHECK(body);
    auto curFile = body->curFile;

    auto paramLists = Nodes<FuncParamList>(CreateFuncParamList(Nodes<FuncParam>()));
    auto retTypeNode = CreateType(retTy);
    auto lambdaFuncBody = CreateFuncBody(std::move(paramLists), std::move(retTypeNode), std::move(body), retTy);

    auto lambda = CreateLambdaExpr(std::move(lambdaFuncBody));
    lambda->curFile = curFile;
    lambda->SetTy(ctx.typeManager.GetFunctionTy({}, retTy));

    auto args = Nodes<FuncArg>(CreateFuncArg(std::move(lambda)));
    auto realRetTy = StaticCast<FuncTy>(exclusiveRef->GetTy())->retTy;

    return CreateCallExpr(std::move(exclusiveRef), std::move(args), exclusiveDecl, realRetTy, CallKind::CALL_DECLARED_FUNCTION);
}

} // namespace Cangjie::Native::FFI::Java
