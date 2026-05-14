// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Desugar/DesugarInTypeCheck.h"
#include "Diags.h"
#include "TypeCheckerImpl.h"
#include "../Plugin/APILevelVersion.h"

using namespace Cangjie;
using namespace AST;

namespace {
const std::string LEVEL_IDENTGIFIER = "level";
const std::string SYSCAP_IDENTGIFIER = "syscap";
// For level check:
const std::string PKG_NAME_DEVICE_INFO_AT = "ohos.device_info";
// For syscap check:
const std::string PKG_NAME_CANIUSE_AT = "ohos.base";

bool IsValidIfAvailableLevelLiteral(const Expr& expr)
{
    auto lce = DynamicCast<LitConstExpr>(&expr);
    if (!lce) {
        return false;
    }
    if (lce->kind == LitConstKind::INTEGER) {
        return true;
    }
    if (lce->kind != LitConstKind::STRING) {
        return false;
    }
    return APILevelVersion::ParseChecked(lce->stringValue, APILevelVersion::ParseRule::TRIPLE_ONLY).has_value();
}

bool IsLiteralLevelCondition(const Expr& condExpr)
{
    if (condExpr.astKind == ASTKind::CALL_EXPR) {
        auto callExpr = StaticCast<CallExpr>(&condExpr);
        return callExpr->args.size() == 1 && callExpr->args[0]->expr &&
            callExpr->args[0]->expr->astKind == ASTKind::LIT_CONST_EXPR;
    }
    if (condExpr.astKind != ASTKind::BINARY_EXPR) {
        return false;
    }
    auto binaryExpr = StaticCast<BinaryExpr>(&condExpr);
    if (binaryExpr->op == TokenKind::AND) {
        return binaryExpr->leftExpr && binaryExpr->rightExpr && IsLiteralLevelCondition(*binaryExpr->leftExpr) &&
            IsLiteralLevelCondition(*binaryExpr->rightExpr);
    }
    return binaryExpr->rightExpr && binaryExpr->rightExpr->astKind == ASTKind::LIT_CONST_EXPR;
}

bool ChkIfImportDeviceInfo(DiagnosticEngine& diag, const ImportManager& im, const IfAvailableExpr& iae)
{
    if (iae.GetFullPackageName() == PKG_NAME_DEVICE_INFO_AT) {
        return true;
    }
    auto importedPkgs = im.GetAllImportedPackages();
    for (auto& importedPkg : importedPkgs) {
        if (importedPkg->srcPackage && importedPkg->srcPackage->fullPackageName == PKG_NAME_DEVICE_INFO_AT) {
            return true;
        }
    }
    auto builder = diag.DiagnoseRefactor(
        DiagKindRefactor::sema_use_expr_without_import, iae, PKG_NAME_DEVICE_INFO_AT, "IfAvailable");
    builder.AddNote("depend on declaration 'DeviceInfo'");
    return false;
}

bool ChkIfImportBase(DiagnosticEngine& diag, const ImportManager& im, const IfAvailableExpr& iae)
{
    if (iae.GetFullPackageName() == PKG_NAME_CANIUSE_AT) {
        return true;
    }
    auto importedPkgs = im.GetAllImportedPackages();
    for (auto& importedPkg : importedPkgs) {
        if (importedPkg->srcPackage && importedPkg->srcPackage->fullPackageName == PKG_NAME_CANIUSE_AT) {
            return true;
        }
    }
    auto builder =
        diag.DiagnoseRefactor(DiagKindRefactor::sema_use_expr_without_import, iae, PKG_NAME_CANIUSE_AT, "IfAvailable");
    builder.AddNote("depend on declaration 'canIUse'");
    return false;
}

// Mirrors ChkIfImportBase but the diagnostic note points to 'apiAvailable',
// which lives in the same `ohos.base` package as `canIUse`. The @IfAvailable
// `level:` desugar may lower to a call to `apiAvailable(...)` (see issue 824),
// so the package must be imported regardless of whether the particular
// (compatibleSDKVersion, level) combination ends up generating that call.
bool ChkIfImportApiAvailableBase(DiagnosticEngine& diag, const ImportManager& im, const IfAvailableExpr& iae)
{
    if (iae.GetFullPackageName() == PKG_NAME_CANIUSE_AT) {
        return true;
    }
    auto importedPkgs = im.GetAllImportedPackages();
    for (auto& importedPkg : importedPkgs) {
        if (importedPkg->srcPackage && importedPkg->srcPackage->fullPackageName == PKG_NAME_CANIUSE_AT) {
            return true;
        }
    }
    auto builder =
        diag.DiagnoseRefactor(DiagKindRefactor::sema_use_expr_without_import, iae, PKG_NAME_CANIUSE_AT, "IfAvailable");
    builder.AddNote("depend on declaration 'apiAvailable'");
    return false;
}
} // namespace

bool TypeChecker::TypeCheckerImpl::ChkIfAvailableExpr(ASTContext& ctx, Ty& ty, IfAvailableExpr& ie)
{
    auto exprTy = SynIfAvailableExpr(ctx, ie);
    if (!Ty::IsTyCorrect(exprTy)) {
        return false;
    }
    if (!typeManager.IsSubtype(exprTy, &ty)) {
        Sema::DiagMismatchedTypes(diag, ie, ty);
        return false;
    }
    return true;
}

Ptr<Ty> TypeChecker::TypeCheckerImpl::SynIfAvailableExpr(ASTContext& ctx, IfAvailableExpr& iae)
{
    // Desugar before type checker.
    auto ie = DynamicCast<IfExpr>(iae.desugarExpr.get());
    if (!ie) {
        return typeManager.GetInvalidTy();
    }
    // Per issue 824: @IfAvailable is only meaningful on OHOS targets. Other
    // targets must reject it outright before any further checks, since the
    // desugar relies on OHOS-specific runtime symbols (DeviceInfo / apiAvailable).
    if (ci != nullptr && ci->invocation.globalOptions.target.env != Triple::Environment::OHOS) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_platform_unsupported, iae,
            ci->invocation.globalOptions.target.OSToString());
        iae.SetTy(typeManager.GetInvalidTy());
        return iae.GetTy();
    }
    bool res{true};
    bool hasHardError{false};
    bool hasLevelFormatWarning{false};
    auto argName = iae.GetArg()->name;
    if (argName.Empty()) {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_arg_no_name, *iae.GetArg());
        res = false;
        hasHardError = true;
    }
    if (argName == LEVEL_IDENTGIFIER && ie->condExpr) {
        auto hasDeviceInfoImport = ChkIfImportDeviceInfo(diag, importManager, iae);
        res = hasDeviceInfoImport && res;
        hasHardError = !hasDeviceInfoImport || hasHardError;
        // The level-form desugar may lower to apiAvailable(...) (issue 824),
        // which lives in ohos.base. Require the package import unconditionally.
        auto hasApiAvailableImport = ChkIfImportApiAvailableBase(diag, importManager, iae);
        res = hasApiAvailableImport && res;
        hasHardError = !hasApiAvailableImport || hasHardError;
        auto argExpr = iae.GetArg()->expr.get();
        if (!argExpr || !IsValidIfAvailableLevelLiteral(*argExpr)) {
            auto lce = DynamicCast<LitConstExpr>(argExpr);
            if (lce && lce->kind == LitConstKind::STRING) {
                diag.DiagnoseRefactor(
                    DiagKindRefactor::sema_apilevel_invalid_version_format, *iae.GetArg(), lce->stringValue.c_str());
                hasLevelFormatWarning = true;
            } else {
                diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_arg_not_literal, *iae.GetArg());
                hasHardError = true;
            }
            res = false;
        }
        if (!IsLiteralLevelCondition(*ie->condExpr)) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_arg_not_literal, *iae.GetArg());
            res = false;
            hasHardError = true;
        }
    } else if (argName == SYSCAP_IDENTGIFIER && ie->condExpr) {
        auto hasBaseImport = ChkIfImportBase(diag, importManager, iae);
        res = hasBaseImport && res;
        hasHardError = !hasBaseImport || hasHardError;
        CJC_ASSERT(ie->condExpr->astKind == ASTKind::CALL_EXPR);
        auto argExpr = StaticCast<CallExpr>(ie->condExpr.get());
        CJC_ASSERT(argExpr->args.size() == 1);
        if (argExpr->args[0]->expr->astKind != ASTKind::LIT_CONST_EXPR) {
            diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_arg_not_literal, *iae.GetArg());
            res = false;
            hasHardError = true;
        }
    } else {
        diag.DiagnoseRefactor(DiagKindRefactor::sema_ifavailable_unknow_arg_name, MakeRange(iae.GetArg()->name),
            iae.GetArg()->name.Val());
        res = false;
        hasHardError = true;
    }
    auto targetTy = typeManager.GetFunctionTy({}, typeManager.GetPrimitiveTy(TypeKind::TYPE_UNIT));
    res = Check(ctx, targetTy, iae.GetLambda1()) && res;
    res = Check(ctx, targetTy, iae.GetLambda2()) && res;
    if (!res) {
        Ptr<Ty> fallbackTy = typeManager.GetInvalidTy();
        if (!hasHardError && hasLevelFormatWarning) {
            fallbackTy = TypeManager::GetPrimitiveTy(TypeKind::TYPE_UNIT);
        }
        iae.SetTy(fallbackTy);
        return iae.GetTy();
    }
    iae.SetTy(Synthesize({ctx, SynPos::EXPR_ARG}, iae.desugarExpr));
    return iae.GetTy();
}
