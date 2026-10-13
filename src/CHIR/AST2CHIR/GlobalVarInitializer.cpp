// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/CHIR/AST2CHIR/GlobalVarInitializer.h"

#include <unordered_map>
#include <unordered_set>

#include "cangjie/AST/Node.h"
#include "cangjie/CHIR/AST2CHIR/Utils.h"
#include "cangjie/CHIR/IR/Annotation.h"
#include "cangjie/CHIR/IR/Expression/Expression.h"
#include "cangjie/CHIR/IR/IntrinsicKind.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/Utils/Utils.h"

using namespace Cangjie;
using namespace CHIR;

namespace {
inline std::string GetPackageInitFuncName(const std::string& pkgName, const std::string& suffix = "")
{
    auto specialName = SPECIAL_NAME_FOR_INIT_FUNCTION;
    if (suffix == "_literal") {
        specialName = SPECIAL_NAME_FOR_INIT_LITERAL_FUNCTION;
    } else if (suffix == "_importsInit") {
        specialName = SPECIAL_NAME_FOR_IMPORTS_INIT_FUNCTION;
    } else if (suffix == "_literal_importsInit") {
        specialName = SPECIAL_NAME_FOR_LITERAL_IMPORTS_INIT_FUNCTION;
#ifdef NDEBUG
    } else if (suffix != "") {
        CJC_ABORT();
#endif
    }
    return MANGLE_CANGJIE_PREFIX + MANGLE_GLOBAL_PACKAGE_INIT_PREFIX + MangleUtils::GetOptPkgName(pkgName) +
        specialName + MANGLE_FUNC_PARAM_TYPE_PREFIX + MANGLE_VOID_TY_SUFFIX;
}

inline std::string GetFileInitFuncName(
    const std::string& pkgName, const std::string& fileName, const std::string_view suffix)
{
    auto specialName = suffix.empty() ? SPECIAL_NAME_FOR_INIT_FUNCTION : SPECIAL_NAME_FOR_INIT_LITERAL_FUNCTION;
    return MANGLE_CANGJIE_PREFIX + MANGLE_GLOBAL_FILE_INIT_PREFIX + MangleUtils::GetOptPkgName(pkgName) +
        MANGLE_FILE_ID_PREFIX +
        (BaseMangler::IsHashable(fileName) ? BaseMangler::HashToBase62(fileName)
                                           : (BaseMangler::FileNameWithoutExtension(fileName) + "$")) +
        specialName + MANGLE_FUNC_PARAM_TYPE_PREFIX + MANGLE_VOID_TY_SUFFIX;
}

inline std::string GetGVInitFuncName(const std::string& gvName, const std::string& pkgName)
{
    return MANGLE_CANGJIE_PREFIX + MANGLE_GLOBAL_VARIABLE_INIT_PREFIX + MangleUtils::GetOptPkgName(pkgName) +
        MangleUtils::MangleName(gvName) + MANGLE_FUNC_PARAM_TYPE_PREFIX + MANGLE_VOID_TY_SUFFIX;
}

inline std::string GetVarInitName(const AST::VarDecl& var)
{
    return MANGLE_CANGJIE_PREFIX + MANGLE_GLOBAL_VARIABLE_INIT_PREFIX +
        var.mangledName.substr((MANGLE_CANGJIE_PREFIX + MANGLE_NESTED_PREFIX).size(),
            var.mangledName.size() - (MANGLE_CANGJIE_PREFIX + MANGLE_NESTED_PREFIX + MANGLE_SUFFIX).size()) +
        MANGLE_FUNC_PARAM_TYPE_PREFIX + MANGLE_VOID_TY_SUFFIX;
}

void FlattenPatternName(const AST::Pattern& pattern, std::string& name)
{
    switch (pattern.astKind) {
        case AST::ASTKind::VAR_PATTERN:
            name += ":" + StaticCast<AST::VarPattern*>(&pattern)->varDecl->identifier;
            break;
        case AST::ASTKind::TUPLE_PATTERN: {
            auto tuplePattern = StaticCast<AST::TuplePattern*>(&pattern);
            for (auto& p : tuplePattern->patterns) {
                FlattenPatternName(*p, name);
            }
            break;
        }
        case AST::ASTKind::ENUM_PATTERN: {
            auto enumPattern = StaticCast<AST::EnumPattern*>(&pattern);
            for (auto& p : enumPattern->patterns) {
                FlattenPatternName(*p, name);
            }
            break;
        }
        case AST::ASTKind::WILDCARD_PATTERN:
            name += ":_";
            break;
        default:
            break;
    }
}

inline std::string GenerateWildcardVarIdent(const AST::Pattern& pattern)
{
    return GV_INIT_WILDCARD_PATTERN + std::to_string(pattern.begin.line) + "_" + std::to_string(pattern.begin.column);
}

std::string GetGVInitNameForPattern(const AST::VarWithPatternDecl& decl)
{
    if (decl.irrefutablePattern->astKind == AST::ASTKind::TUPLE_PATTERN) {
        std::string nameSuffix;
        FlattenPatternName(*decl.irrefutablePattern, nameSuffix);
        return GetGVInitFuncName("tuple_pattern", decl.fullPackageName) + nameSuffix;
    } else if (decl.irrefutablePattern->astKind == AST::ASTKind::ENUM_PATTERN) {
        std::string nameSuffix;
        FlattenPatternName(*decl.irrefutablePattern, nameSuffix);
        return GetGVInitFuncName("enum_pattern", decl.fullPackageName) + nameSuffix;
    } else if (decl.irrefutablePattern->astKind == AST::ASTKind::WILDCARD_PATTERN) {
        return GetGVInitFuncName(GenerateWildcardVarIdent(*decl.irrefutablePattern), decl.fullPackageName);
    } else {
        CJC_ABORT();
        return "";
    }
}

template <typename T> struct GVInit; // the primary template is never used and therefore never defined

template <> struct GVInit<AST::Package> {
    explicit GVInit(const AST::Package& pkg, const Translator&, const std::string& suffix = "")
        : srcCodeIdentifier(GetPackageInitFuncName(pkg.fullPackageName, suffix)),
          mangledName(srcCodeIdentifier),
          rawMangledName(""),
          packageName(pkg.fullPackageName)
    {
    }

public:
    // non-const, to be moved from
    std::string srcCodeIdentifier;
    std::string mangledName;
    std::string rawMangledName;
    std::string packageName;
    static constexpr Linkage LINKAGE = Linkage::EXTERNAL;
    DebugLocation loc = INVALID_LOCATION;
    bool isConst{false};
};

template <> struct GVInit<AST::File> {
    explicit GVInit(const AST::File& file, const Translator& trans, const std::string& suffix = "")
        : packageName(file.curPackage->fullPackageName),
          srcCodeIdentifier(GetFileInitFuncName(packageName, file.fileName, suffix)),
          mangledName(srcCodeIdentifier),
          rawMangledName(""),
          loc(trans.TranslateFileLocation(file.begin.fileID))
    {
    }

public:
    std::string packageName;
    std::string srcCodeIdentifier;
    std::string mangledName;
    std::string rawMangledName;
    static constexpr Linkage LINKAGE = Linkage::INTERNAL;
    DebugLocation loc;
    bool isConst{false};
};

template <> struct GVInit<AST::VarDecl> {
    explicit GVInit(const AST::VarDecl& var, const Translator& trans, const std::string& suffix = "")
        : srcCodeIdentifier("gv$_" + var.identifier + suffix),
          mangledName(GetVarInitName(var)),
          rawMangledName(var.rawMangleName),
          packageName(var.fullPackageName),
          loc(trans.TranslateLocation(var)),
          isConst(var.IsConst())
    {
    }

public:
    std::string srcCodeIdentifier;
    std::string mangledName;
    std::string rawMangledName;
    std::string packageName;
    static constexpr Linkage LINKAGE = Linkage::INTERNAL;
    DebugLocation loc;
    bool isConst;
};

template <> struct GVInit<AST::VarWithPatternDecl> {
    explicit GVInit(const AST::VarWithPatternDecl& decl, const Translator& trans)
        : srcCodeIdentifier(GetGVInitNameForPattern(decl)),
          mangledName(MANGLE_CANGJIE_PREFIX + MANGLE_GLOBAL_VARIABLE_INIT_PREFIX +
              decl.mangledName.substr((MANGLE_CANGJIE_PREFIX + MANGLE_NESTED_PREFIX).size(),
                  decl.mangledName.size() - (MANGLE_CANGJIE_PREFIX + MANGLE_NESTED_PREFIX + MANGLE_SUFFIX).size()) +
              MANGLE_FUNC_PARAM_TYPE_PREFIX + MANGLE_VOID_TY_SUFFIX),
          rawMangledName(decl.rawMangleName),
          packageName(decl.fullPackageName),
          loc(trans.TranslateLocation(decl)),
          isConst(decl.IsConst())
    {
    }

public:
    std::string srcCodeIdentifier;
    std::string mangledName;
    std::string rawMangledName;
    std::string packageName;
    static constexpr Linkage LINKAGE = Linkage::INTERNAL;
    DebugLocation loc;
    bool isConst;
};

static bool IsSimpleLiteralValue(const AST::Expr& node)
{
    auto realNode = Translator::GetDesugaredExpr(node);
    if (realNode->astKind != AST::ASTKind::LIT_CONST_EXPR) {
        return false;
    }
    switch (realNode->TyKind()) {
        case AST::TypeKind::TYPE_FLOAT16:
        case AST::TypeKind::TYPE_FLOAT64:
        case AST::TypeKind::TYPE_IDEAL_FLOAT:
        case AST::TypeKind::TYPE_FLOAT32:
        case AST::TypeKind::TYPE_UINT8:
        case AST::TypeKind::TYPE_UINT16:
        case AST::TypeKind::TYPE_UINT32:
        case AST::TypeKind::TYPE_UINT64:
        case AST::TypeKind::TYPE_UINT_NATIVE:
        case AST::TypeKind::TYPE_INT8:
        case AST::TypeKind::TYPE_INT16:
        case AST::TypeKind::TYPE_INT32:
        case AST::TypeKind::TYPE_INT64:
        case AST::TypeKind::TYPE_INT_NATIVE:
        case AST::TypeKind::TYPE_IDEAL_INT:
        case AST::TypeKind::TYPE_RUNE:
        case AST::TypeKind::TYPE_BOOLEAN:
            return true;
        case AST::TypeKind::TYPE_STRUCT:
            return node.GetTy()->IsString();
        default:
            return false;
    }
}

static Ptr<Block> GetBlockWithInitializers(const BlockGroup& blockGroup)
{
    for (auto block : blockGroup.GetBlocks()) {
        if (block->TestAttr(Attribute::INITIALIZER)) {
            return block;
        }
    }

    CJC_ABORT(); // there is no block related to initalization process
    return nullptr;
}

bool NeedInitGlobalVarByInitFunc(const AST::VarDecl& decl)
{
    // If decl without initializer (like common let x: Int64).
    if (!decl.initializer) {
        return false;
    }
    // common/specific cannot be initialized with literal value,
    // it can change to non literal initializer in the future.
    // So, initializer function need to be generated for such variables.
    if (decl.IsCommonOrSpecific()) {
        return true;
    }
    return !(IsSimpleLiteralValue(*decl.initializer) && decl.GetTy() == decl.initializer->GetTy());
}

inline bool CanInitBeGenerated(const AST::VarDeclAbstract& varDecl)
{
    return varDecl.initializer;
}

/// Looking for a Block with Attribute::INITIALIZER, remove its Exit node.
inline Ptr<Block> DropExitNodeOfInitializer(const Function& packageInit)
{
    CJC_ASSERT(packageInit.TestAttr(Attribute::INITIALIZER));
    auto packageInitBody = packageInit.GetBody();

    auto blockWithInitializers = GetBlockWithInitializers(*packageInitBody);
    // drop Exit node
    auto exit = blockWithInitializers->GetTerminator();
    exit->RemoveSelfFromBlock();

    return blockWithInitializers;
}
} // namespace

Ptr<Value> GlobalVarInitializer::GetGlobalVariable(const AST::VarDecl& decl)
{
    return globalSymbolTable.Get(decl);
}

template <typename T, typename... Args>
Ptr<Function> GlobalVarInitializer::CreateGVInitFunc(const T& node, Args&&... args) const
{
    auto context = GVInit<T>(node, trans, std::forward<Args>(args)...);
    bool isConst{false};
    // No debug location generated for const var initializer func
    // because these exprs will not be executed at runtime and falsely contribute to coverage
    if constexpr (std::is_same_v<T, AST::VarDecl>) {
        isConst = node.IsConst();
    }
    return trans.CreateEmptyGVInitFunc(std::move(context.mangledName), std::move(context.srcCodeIdentifier),
        std::move(context.rawMangledName), std::move(context.packageName), GVInit<T>::LINKAGE,
        isConst ? INVALID_LOCATION : std::move(context.loc), context.isConst);
}

Function* GlobalVarInitializer::TranslateInitializerToFunction(const AST::VarDecl& decl)
{
    auto variable = GetGlobalVariable(decl);
    auto func = CreateGVInitFunc<AST::VarDecl>(decl);
    auto globalVar = StaticCast<GlobalVar*>(variable);
    globalVar->SetInitFunc(*func);
    // No debug location generated for const var initializer
    auto initNode = trans.TranslateExprArg(
        *decl.initializer, *StaticCast<RefType*>(variable->GetType())->GetBaseType());
    if (decl.IsConst()) {
        for (auto expr : trans.currentBlock->GetExpressions()) {
            expr->SetDebugLocation(INVALID_LOCATION);
        }
    }
    auto loc = decl.IsConst() ? INVALID_LOCATION : trans.TranslateLocation(decl);
    CJC_ASSERT(variable->GetType()->IsRef());
    trans.CreateAndAppendWrappedStore(*initNode, *variable, loc);
    auto curBlock = trans.GetCurrentBlock();
    if (curBlock->GetTerminator() == nullptr) {
        trans.CreateAndAppendTerminator<Exit>(curBlock);
    }

    return func;
}

bool GlobalVarInitializer::IsIncrementalNoChange(const AST::VarDecl& decl) const
{
    return enableIncre && !decl.toBeCompiled;
}

Function* GlobalVarInitializer::TranslateIncrementalNoChangeVar(const AST::VarDecl& decl)
{
    GVInit<AST::VarDecl> context{decl, trans};
    auto ty = builder.GetType<FuncType>(std::vector<Type*>{}, builder.GetUnitTy());
    auto func = builder.CreateFunction(ty, std::move(context.mangledName),
        std::move(context.srcCodeIdentifier), std::move(context.rawMangledName), context.packageName);
    func->SetFuncKind(FuncKind::GLOBALVAR_INIT);
    func->EnableAttr(Attribute::IMPORTED);
    func->Set<LinkTypeInfo>(Cangjie::Linkage::INTERNAL);
    if (decl.isConst) {
        func->EnableAttr(Attribute::CONST);
    }
    return func;
}

Function* GlobalVarInitializer::TranslateSingleInitializer(const AST::VarDecl& decl)
{
    if (!CanInitBeGenerated(decl)) {
        return nullptr;
    }
    if (decl.specificImplementation) {
        return nullptr;
    }
    if (auto func = TryGetDeserialized<Function>(GetVarInitName(decl)); func) {
        if (!decl.TestAttr(AST::Attribute::SPECIFIC)) {
            return func;
        }
    }
    // The variable with literal init value is hanlded in elsewhere
    if (!NeedInitGlobalVarByInitFunc(decl)) {
        return nullptr;
    }

    if (IsIncrementalNoChange(decl) && !IsSrcCodeImportedGlobalDecl(decl, opts)) {
        if (decl.identifier == STATIC_INIT_VAR) {
            // for static variables to be inited in static ctors, the special variable "$init" falls in this branch
            // (as it has a default value "false")
            return TranslateInitializerToFunction(decl);
        } else {
            // For a non-recompile variable which is not inited in `static.init` func, after the variable itself is
            // translated into a pseudo import. As for the init func, we should also create a pseudo import for it
            // and make sure the package init func will call this imported init
            return TranslateIncrementalNoChangeVar(decl);
        }
    } else {
        return TranslateInitializerToFunction(decl);
    }
}

void GlobalVarInitializer::FillGVInitFuncWithApplyAndExit(const std::vector<Ptr<Value>>& varInitFuncs)
{
    auto curBlock = trans.GetCurrentBlock();
    for (auto& func : varInitFuncs) {
        trans.CreateAndAppendGVInitFuncCall(*func);
    }
    trans.CreateAndAppendTerminator<Exit>(curBlock);
}

Function* GlobalVarInitializer::TranslateTupleOrEnumPatternInitializer(const AST::VarWithPatternDecl& decl)
{
    auto func = CreateGVInitFunc<AST::VarWithPatternDecl>(decl);
    if (decl.IsConst()) {
        func->SetDebugLocation(INVALID_LOCATION);
    }
    auto initNode = Translator::TranslateASTNode(*decl.initializer, trans);
    trans.FlattenVarWithPatternDecl(*decl.irrefutablePattern, initNode, false);
    if (decl.IsConst()) {
        for (auto bl : func->GetBody()->GetBlocks()) {
            for (auto expr : bl->GetExpressions()) {
                expr->SetDebugLocation(INVALID_LOCATION);
            }
        }
    }
    auto curBlock = trans.GetCurrentBlock();
    if (curBlock->GetTerminator() == nullptr) {
        trans.CreateAndAppendTerminator<Exit>(curBlock);
    }
    return func;
}

Function* GlobalVarInitializer::TranslateWildcardPatternInitializer(const AST::VarWithPatternDecl& decl)
{
    auto func = CreateGVInitFunc<AST::VarWithPatternDecl>(decl);
    Translator::TranslateASTNode(*decl.initializer, trans);
    auto curBlock = trans.GetCurrentBlock();
    if (curBlock->GetTerminator() == nullptr) {
        trans.CreateAndAppendTerminator<Exit>(curBlock);
    }

    return func;
}

Function* GlobalVarInitializer::TranslateVarWithPatternInitializer(const AST::VarWithPatternDecl& decl)
{
    switch (decl.irrefutablePattern->astKind) {
        case AST::ASTKind::TUPLE_PATTERN:
        case AST::ASTKind::ENUM_PATTERN:
            return TranslateTupleOrEnumPatternInitializer(decl);
        case AST::ASTKind::WILDCARD_PATTERN: {
            return TranslateWildcardPatternInitializer(decl);
        }
        default:
            CJC_ABORT();
            return nullptr;
    }
}

Function* GlobalVarInitializer::TranslateVarInit(const AST::Decl& var)
{
    if (auto vd = DynamicCast<const AST::VarDecl*>(&var)) {
        return TranslateSingleInitializer(*vd);
    } else if (auto vwpd = DynamicCast<const AST::VarWithPatternDecl*>(&var)) {
        return TranslateVarWithPatternInitializer(*vwpd);
    } else {
        CJC_ABORT();
        return nullptr;
    }
}

Ptr<Function> GlobalVarInitializer::TryGetFileInitializer(const AST::File& file, const std::string& suffix)
{
    auto initializerName = GetFileInitFuncName(file.curPackage->fullPackageName, file.fileName, suffix);

    return TryGetDeserialized<Function>(initializerName);
}

void GlobalVarInitializer::RemoveInitializerForVarDecl(const AST::VarDecl& varDecl, Function& fileInit) const
{
    auto declInitName = "@" + GetVarInitName(varDecl);

    auto blockWithInitializers = fileInit.GetBody()->GetBlocks().back();
    Expression* init = nullptr;
    for (auto expr : blockWithInitializers->GetExpressions()) {
        auto kind = expr->GetExprKind();
        if (kind != ExprKind::APPLY) {
            continue;
        }

        auto apply = StaticCast<const Apply*>(expr);
        auto initFunc = apply->GetCallee();
        CJC_ASSERT(initFunc->TestAttr(Attribute::INITIALIZER));
        auto applyCalleeName = initFunc->GetIdentifier();
        if (applyCalleeName == declInitName) {
            init = expr;
            break;
        }
    }

    // if it is in this initializer
    if (init) {
        init->RemoveSelfFromBlock();
    }
}

void GlobalVarInitializer::RemoveCommonInitializersReplacedWithSpecific(
    Function& fileInit, const std::vector<Ptr<const AST::Decl>>& decls) const
{
    for (auto decl : decls) {
        if (decl->IsCommonMatchedWithSpecific()) {
            CJC_ASSERT(decl->astKind == AST::ASTKind::VAR_DECL);
            const AST::VarDecl& varDecl = StaticCast<const AST::VarDecl>(*decl);

            RemoveInitializerForVarDecl(varDecl, fileInit);
        }
    }
}

Ptr<Function> GlobalVarInitializer::TranslateFileInitializer(
    const AST::File& file, const std::vector<Ptr<const AST::Decl>>& decls)
{
    auto fileInit = TryGetFileInitializer(file);
    if (fileInit) {
        CJC_ASSERT(fileInit->TestAttr(Attribute::INITIALIZER));

        RemoveCommonInitializersReplacedWithSpecific(*fileInit, decls);

        return fileInit;
    }

    std::vector<Ptr<Value>> varInitFuncs;
    for (auto decl : decls) {
        if (auto initFunc = TranslateVarInit(*decl)) {
            auto features = decl->curFile->GetFeatures();
            initFunc->SetFeatures(features);
            if (decl->IsConst()) {
                initFuncsForConstVar.emplace_back(initFunc);
                // In incremental compilation scenarios, only changes need to be re-evaluated.
                if (!enableIncre || decl->toBeCompiled) {
                    SetCompileTimeValueFlagRecursively(*StaticCast<Function*>(initFunc));
                }
            }
            varInitFuncs.push_back(initFunc);
        }
    }

    auto func = CreateGVInitFunc<AST::File>(file);
    FillGVInitFuncWithApplyAndExit(varInitFuncs);
    if (varInitFuncs.empty()) {
        func->DisableAttr(Attribute::NO_INLINE);
    }

    return func;
}

bool GlobalVarInitializer::NeedVarLiteralInitFunc(const AST::Decl& decl)
{
    auto vd = DynamicCast<const AST::VarDecl*>(&decl);
    if (vd == nullptr || !vd->initializer || NeedInitGlobalVarByInitFunc(*vd)) {
        return false;
    }
    // common var with specific one should not be retranslated.
    if (decl.TestAttr(AST::Attribute::COMMON) && decl.specificImplementation) {
        return false;
    }
    // `decl` may be a wildcard, like: `var _ = 1`, doesn't need to be translated;
    // incremental no-change var does not have initialiser; they are copied from cached bc in codegen
    if (IsIncrementalNoChange(*vd)) {
        return false;
    }

    CJC_ASSERT(vd->initializer->astKind == AST::ASTKind::LIT_CONST_EXPR);
    auto litExpr = StaticCast<AST::LitConstExpr*>(vd->initializer.get());
    auto globalVar = StaticCast<GlobalVar*>(GetGlobalVariable(*vd));
    globalVar->SetInitializer(*trans.TranslateLitConstant(*litExpr, *litExpr->GetTy()));

    // mutable var decl need to be initialized in `file_literal`, codegen will call `file_literal` in
    // macro expand situation, immutable var decl doesn't need to
    if (!vd->isVar) {
        return false;
    }
    return true;
}

Ptr<Function> GlobalVarInitializer::TranslateFileLiteralInitializer(
    const AST::File& file, const std::vector<Ptr<const AST::Decl>>& decls)
{
    std::list<const AST::VarDecl*> varsToGenInit{};
    for (auto decl : decls) {
        if (NeedVarLiteralInitFunc(*decl)) {
            varsToGenInit.push_back(StaticCast<AST::VarDecl>(decl));
        }
    }
    // do not generate if no literal need initialisation
    if (varsToGenInit.empty()) {
        return nullptr;
    }

    // NOTE: this function is only called by CodeGen and only used for macro expand situation.
    // And only mutable primitive values need to be re-initialized.
    // Literal reset functions do not need 'NO_INLINE' attr.
    auto func = CreateGVInitFunc<AST::File>(file, "_literal");
    func->DisableAttr(Attribute::NO_INLINE);
    func->EnableAttr(Attribute::INITIALIZER);
    func->SetDebugLocation(INVALID_LOCATION);
    for (auto vd : varsToGenInit) {
        auto globalVar = StaticCast<GlobalVar*>(GetGlobalVariable(*vd));
        auto initNode = trans.TranslateExprArg(*vd->initializer);
        // this is in gv init for literal, we can't set breakpoint with cjdb, so we can't set DebugLocationInfo
        // for any expression
        initNode->SetDebugLocation(INVALID_LOCATION);
        trans.CreateAndAppendWrappedStore(*initNode, *globalVar);
    }
    auto currentBlock = trans.GetCurrentBlock();
    trans.CreateAndAppendTerminator<Exit>(currentBlock);

    return func;
}

void GlobalVarInitializer::AddImportedPackageInit(const AST::Package& curPackage, const std::string& suffix)
{
    auto voidTy = builder.GetUnitTy();
    auto initFuncTy = builder.GetType<FuncType>(std::vector<Type*>{}, voidTy);
    for (auto& dep : importManager.GetCurImportedPackages(curPackage.fullPackageName)) {
        const std::string& pkgName = dep->srcPackage->fullPackageName;
        bool doNotCallInit = dep->srcPackage->isMacroPackage;
        if (doNotCallInit) {
            continue;
        }
        auto context = GVInit<AST::Package>(*dep->srcPackage, trans, suffix);
        // Try get deserialized one.
        Function* initFunc = TryGetDeserialized<Function>(context.mangledName);
        // Already be translated when compiling common part.
        if (initFunc) {
            continue;
        }
        auto attrs = AttributeInfo();
        attrs.SetAttr(Attribute::INITIALIZER, true);
        initFunc = builder.CreateFunction(
            initFuncTy, context.mangledName, context.srcCodeIdentifier, context.rawMangledName, pkgName);
        initFunc->EnableAttr(Attribute::IMPORTED);
        initFunc->AppendAttributeInfo(attrs);
        initFunc->EnableAttr(Attribute::PUBLIC);
        trans.CreateAndAppendGVInitFuncCall(*initFunc);
    }
}

// [CJMP]: define inlining policy here and in other places after disabling CHIR transformations of common part
Ptr<Function> GlobalVarInitializer::CreateImportsInitFunc(const AST::Package& curPackage, const std::string& suffix)
{
    auto importsInitFunc = CreateGVInitFunc<AST::Package>(curPackage, suffix + "_importsInit");
    importsInitFunc->Set<LinkTypeInfo>(Linkage::INTERNAL);
    auto curBlockGroup = importsInitFunc->GetBody();
    auto curBlock = curBlockGroup->GetEntryBlock();
    curBlock->EnableAttr(Attribute::INITIALIZER);
    trans.CreateAndAppendTerminator<Exit>(curBlock);

    return importsInitFunc;
}

Ptr<Function> GlobalVarInitializer::GetImportsInitFunc(const AST::Package& curPackage, const std::string& suffix)
{
    auto initializerName = GetPackageInitFuncName(curPackage.fullPackageName, suffix + "_importsInit");
    auto importsInitFunc = TryGetDeserialized<Function>(initializerName);
    CJC_ASSERT(importsInitFunc);

    return importsInitFunc;
}

namespace {
bool DoNotGenerateInitForImport(AST::PackageDecl& dep)
{
    bool doNotCallInit = dep.srcPackage->isMacroPackage;
#ifdef CANGJIE_CODEGEN_CJVM_BACKEND
    const std::string& pkgName = dep.srcPackage->fullPackageName;
    doNotCallInit =
        doNotCallInit || Utils::IsJava8Module(pkgName) || dep.srcPackage->TestAttr(AST::Attribute::TOOL_ADD);
#endif

    return doNotCallInit;
}
} // namespace

void GlobalVarInitializer::UpdateImportsInit(
    const AST::Package& curPackage, Function& importsInitFunc, const std::string& suffix)
{
    Block* curBlock = DropExitNodeOfInitializer(importsInitFunc);
    trans.SetCurrentBlock(*curBlock);

    for (auto& dep : importManager.GetCurImportedPackages(curPackage.fullPackageName)) {
        if (DoNotGenerateInitForImport(*dep)) {
            continue;
        }

        auto context = GVInit<AST::Package>(*dep->srcPackage, trans, suffix);
        if (auto initFunc = TryGetDeserialized<Function>(context.mangledName); initFunc) {
            continue;
        }

        // Creating import initializer
        auto initFuncTy = builder.GetType<FuncType>(std::vector<Type*>{}, builder.GetUnitTy());

        auto attrs = AttributeInfo();
        attrs.SetAttr(Attribute::INITIALIZER, true);
        auto importInit = builder.CreateFunction(initFuncTy, context.mangledName,
            context.srcCodeIdentifier, context.rawMangledName, dep->srcPackage->fullPackageName);
        importInit->EnableAttr(Attribute::IMPORTED);
        importInit->AppendAttributeInfo(attrs);
        importInit->SetFuncKind(FuncKind::GLOBALVAR_INIT);
        importInit->EnableAttr(Attribute::PUBLIC);

        InsertInitializerIntoPackageInitializer(*importInit, importsInitFunc);
    }

    trans.CreateAndAppendTerminator<Exit>(curBlock);
}

void GlobalVarInitializer::AddGenericInstantiatedInit()
{
    auto callContext = IntrisicCallContext{.kind = IntrinsicKind::PREINITIALIZE};
    trans.CreateAndAppendExpression<Intrinsic>(builder.GetUnitTy(), callContext, trans.GetCurrentBlock());
}

Ptr<Function> GlobalVarInitializer::GeneratePackageInitBase(const AST::Package& curPackage, const std::string& suffix)
{
    /*  var initFlag: Bool = false
        func pkg_init_suffix()
        {
            if (initFlag) {
                return
            }
            initFlag = true
            apply all imported package init_suffix funcs
            apply all current package file init_suffix funcs
        }
    */
    auto func = CreateGVInitFunc<AST::Package>(curPackage, suffix);

    // 1. Create global variable as guard condition.
    auto boolTy = builder.GetBoolTy();
    auto initFlagName = suffix.empty() ? GV_PKG_INIT_ONCE_FLAG : "has_invoked_pkg_init_literal";
    GlobalVar* initFlag = TryGetDeserialized<GlobalVar>(initFlagName);
    if (!initFlag) {
        initFlag = builder.CreateGlobalVar(
            builder.GetType<RefType>(boolTy), initFlagName, initFlagName, "", func->GetPackageName());
        initFlag->SetInitializer(*builder.CreateLiteralValue<BoolLiteral>(boolTy, false));
        initFlag->EnableAttr(Attribute::NO_REFLECT_INFO);
        initFlag->EnableAttr(Attribute::COMPILER_ADD);
        initFlag->EnableAttr(Attribute::INITIALIZER);
        initFlag->EnableAttr(Attribute::NO_DEBUG_INFO);
        initFlag->Set<LinkTypeInfo>(Linkage::INTERNAL);
    }

    // 2. Add `if (initFlag) { return }`
    auto curBlockGroup = func->GetBody();
    auto curBlock = curBlockGroup->GetEntryBlock();
    auto flagRef = trans.CreateAndAppendExpression<Load>(boolTy, initFlag, curBlock)->GetResult();

    auto returnBlock = builder.CreateBlock(curBlockGroup);
    auto applyInitFuncBlock = builder.CreateBlock(curBlockGroup);
    trans.CreateAndAppendTerminator<Branch>(flagRef, returnBlock, applyInitFuncBlock, curBlock);

    // if has initialized, return
    trans.CreateAndAppendTerminator<Exit>(returnBlock);

    // 3. set `initFlag` true
    trans.SetCurrentBlock(*applyInitFuncBlock);
    auto trueLit = trans.CreateAndAppendConstantExpression<BoolLiteral>(boolTy, *applyInitFuncBlock, true)->GetResult();
    trans.CreateAndAppendWrappedStore(*trueLit, *initFlag);
    return func;
}

static Ptr<Apply> FindApplyIn(const Block& block, Function& applyCallee)
{
    auto expressions = block.GetExpressions();
    for (auto expression : expressions) {
        if (auto apply = DynamicCast<Apply>(expression)) {
            if (apply->GetCallee() == &applyCallee) {
                return apply;
            }
        }
    }

    return nullptr;
}

void GlobalVarInitializer::InsertInitializerIntoPackageInitializer(Function& init, Function& packageInit)
{
    auto packageInitBody = packageInit.GetBody();
    auto blockWithInitializers = GetBlockWithInitializers(*packageInitBody);

    if (init.TestAttr(Attribute::DESERIALIZED) && packageInit.TestAttr(Attribute::DESERIALIZED)) {
        // It was inserted at previous compilation phase ==>

        auto initCallExpr = FindApplyIn(*blockWithInitializers, init);
        if (initCallExpr) {
            auto lastExpr = blockWithInitializers->GetExpressions().back();
            if (initCallExpr != lastExpr) {
                // ==> need to push to the end
                initCallExpr->MoveAfter(lastExpr);
            }
        } else {
            // But it can be inserted in different initializer, in this case `APPLY` need to be created.
            trans.SetCurrentBlock(*blockWithInitializers);
            trans.CreateAndAppendGVInitFuncCall(init);
        }
        return;
    }

    trans.SetCurrentBlock(*blockWithInitializers);
    trans.CreateAndAppendGVInitFuncCall(init);
}

void GlobalVarInitializer::InsertAnnoFactoryInitsIntoPackageInit(Function& packageInit)
{
    if (initFuncsForAnnoFactory.empty()) {
        return;
    }
    auto blockWithInitializers = GetBlockWithInitializers(*packageInit.GetBody());
    trans.SetCurrentBlock(*blockWithInitializers);

    // Place anno inits right after CJ_MRT_PreInitializePackage (exactly one such intrinsic).
    Expression* insertAfter = nullptr;
    for (auto expr : blockWithInitializers->GetExpressions()) {
        if (auto intrinsic = DynamicCast<Intrinsic>(expr);
            intrinsic && intrinsic->GetIntrinsicKind() == IntrinsicKind::PREINITIALIZE) {
            insertAfter = expr;
            break;
        }
    }
    // Without this anchor, Apply would stay at block end (after user gv inits) — silent mis-order / crash at
    // reflect. Missing PreInitialize is a compiler invariant failure.
    CJC_NULLPTR_CHECK(insertAfter);

    // insertAfter advances after each call so multiple anno inits keep declaration order
    // (all after PreInitialize, before later file/gv inits). FindApplyIn avoids a second Apply.
    for (auto initFunc : initFuncsForAnnoFactory) {
        auto initCallExpr = FindApplyIn(*blockWithInitializers, *initFunc);
        if (!initCallExpr) {
            initCallExpr = StaticCast<Apply*>(trans.CreateAndAppendGVInitFuncCall(*initFunc));
        }
        initCallExpr->MoveAfter(insertAfter);
        insertAfter = initCallExpr;
    }
}

inline std::pair<Function*, Block*> GlobalVarInitializer::PreparePackageInit(const AST::Package& curPackage)
{
    // create/use deserialized base of package initializer function.
    Function* packageInit = builder.GetCurPackage()->GetPackageInitFunc();
    Block* curBlock;
    if (packageInit) {
        curBlock = DropExitNodeOfInitializer(*packageInit);
        auto importsInitFunc = GetImportsInitFunc(curPackage);
        UpdateImportsInit(curPackage, *importsInitFunc);
        trans.SetCurrentBlock(*curBlock);
    } else {
        packageInit = GeneratePackageInitBase(curPackage).get();
        builder.GetCurPackage()->SetPackageInitFunc(packageInit);
        curBlock = trans.GetCurrentBlock();
        // Mark the block to find it later for modification on the next compilations.
        curBlock->EnableAttr(Attribute::INITIALIZER);

        // add apply of imported package init function.
        auto importsInitFunc = CreateImportsInitFunc(curPackage);
        InsertInitializerIntoPackageInitializer(*importsInitFunc, *packageInit);
        UpdateImportsInit(curPackage, *importsInitFunc);
        trans.SetCurrentBlock(*curBlock);
        // add apply of static generic instantiated init
        AddGenericInstantiatedInit();
    }

    return std::make_pair(packageInit, curBlock);
}

/// Generate package initializer. Runtime call order inside package init body:
/// 1. once-guard: return if already initialized
/// 2. set once-guard flag
/// 3. imported packages' init
/// 4. call runtime CJ_MRT_PreInitializePackage
/// 5. annotation factory instance inits
/// 6. current package file / global-var inits
namespace {
// Prefer the specific decl when a common decl was matched; used when ordering inits across
// several parent CHIRs (diamond merge) where the same logical var appears more than once.
Ptr<const AST::Decl> EffectiveInitDecl(Ptr<const AST::Decl> decl)
{
    if (decl->specificImplementation) {
        return decl->specificImplementation;
    }
    return decl;
}

bool IsCurrentPackageFileInit(const Function& func, const std::string& pkgName)
{
    if (!func.TestAttr(Attribute::INITIALIZER) || func.GetPackageName() != pkgName) {
        return false;
    }
    // File-level init mangling: @_CGF... (see MANGLE_GLOBAL_FILE_INIT_PREFIX).
    const auto& id = func.GetIdentifier();
    return id.find("_CGF") != std::string::npos;
}

// Topological order of vars for diamond/multi-parent merge. File-level initOrder is not
// reliable here: the same common file can appear once per parent with different remnants.
std::vector<Ptr<const AST::VarDecl>> OrderVarsByDependencies(
    const std::vector<Ptr<const AST::VarDecl>>& vars)
{
    // Match by mangled name: diamond merge has distinct AST nodes per parent for one var.
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < vars.size(); ++i) {
        index.emplace(vars[i]->mangledName, i);
    }
    std::vector<std::vector<size_t>> succ(vars.size());
    std::vector<size_t> indeg(vars.size(), 0);
    for (size_t i = 0; i < vars.size(); ++i) {
        for (auto dep : vars[i]->dependencies) {
            auto effective = EffectiveInitDecl(dep);
            auto* depVd = DynamicCast<const AST::VarDecl*>(effective.get());
            if (depVd == nullptr) {
                continue;
            }
            auto it = index.find(depVd->mangledName);
            if (it == index.end() || it->second == i) {
                continue;
            }
            // vars[i] depends on effective => effective must run first.
            succ[it->second].push_back(i);
            indeg[i]++;
        }
    }
    std::vector<size_t> queue;
    for (size_t i = 0; i < vars.size(); ++i) {
        if (indeg[i] == 0) {
            queue.push_back(i);
        }
    }
    std::vector<Ptr<const AST::VarDecl>> ordered;
    ordered.reserve(vars.size());
    std::vector<bool> seen(vars.size(), false);
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        size_t u = queue[qi];
        ordered.push_back(vars[u]);
        seen[u] = true;
        for (size_t v : succ[u]) {
            if (--indeg[v] == 0) {
                queue.push_back(v);
            }
        }
    }
    // Cycle or missing edges: append leftovers in original order.
    for (size_t i = 0; i < vars.size(); ++i) {
        if (!seen[i]) {
            ordered.push_back(vars[i]);
        }
    }
    return ordered;
}
} // namespace

void GlobalVarInitializer::CreatePackageInit(const AST::Package& curPackage, const InitOrder& initOrder)
{
    auto [packageInit, curBlock] = PreparePackageInit(curPackage);
    InsertAnnoFactoryInitsIntoPackageInit(*packageInit);

    const bool multiParentMerge = trans.mergingSpecific && opts.commonPartChirs.size() > 1;
    if (multiParentMerge) {
        // Diamond merge can list the same common file once per parent. File-level initOrder is
        // then wrong (e.g. c before b). Translate file inits, then re-insert Applies in an order
        // derived from per-var dependencies (specific wins).
        std::unordered_map<std::string, Ptr<const AST::VarDecl>> byMangle;
        std::vector<Ptr<const AST::VarDecl>> uniqueVars;
        std::unordered_map<const AST::File*, Function*> fileToInit;
        for (auto& fileAndVars : initOrder) {
            if (fileAndVars.second.empty() && !enableIncre) {
                continue;
            }
            const AST::File* file = fileAndVars.first.get();
            // Parent source-set files are often marked IMPORTED; still belong to this package.
            if (file == nullptr || file->curPackage == nullptr ||
                file->curPackage->fullPackageName != curPackage.fullPackageName) {
                continue;
            }
            auto fileInit = TranslateFileInitializer(*file, fileAndVars.second);
            if (fileInit) {
                fileToInit[file] = fileInit;
            }
            for (auto decl : fileAndVars.second) {
                auto* vd = DynamicCast<const AST::VarDecl*>(decl);
                if (vd == nullptr) {
                    continue;
                }
                // Use the specific decl when the common one was matched.
                if (vd->specificImplementation) {
                    vd = DynamicCast<const AST::VarDecl*>(vd->specificImplementation.get());
                    if (vd == nullptr) {
                        continue;
                    }
                }
                auto [it, inserted] = byMangle.emplace(vd->mangledName, vd);
                if (inserted) {
                    uniqueVars.push_back(vd);
                    continue;
                }
                // Parent AST often keeps COMMON on both sides after cjo merge; prefer the
                // decl whose file-init actually Applies this var's initializer.
                auto fileInitOwnsVar = [](Function* fi, const AST::VarDecl& varDecl) {
                    if (fi == nullptr || fi->GetBody() == nullptr) {
                        return false;
                    }
                    const std::string initName = "@" + GetVarInitName(varDecl);
                    for (auto* block : fi->GetBody()->GetBlocks()) {
                        for (auto* expr : block->GetExpressions()) {
                            auto* apply = DynamicCast<Apply>(expr);
                            if (apply == nullptr || apply->GetCallee() == nullptr) {
                                continue;
                            }
                            if (apply->GetCallee()->GetIdentifier() == initName) {
                                return true;
                            }
                        }
                    }
                    return false;
                };
                Function* newFI = nullptr;
                Function* oldFI = nullptr;
                if (vd->curFile) {
                    auto fIt = fileToInit.find(vd->curFile.get());
                    if (fIt != fileToInit.end()) {
                        newFI = fIt->second;
                    }
                }
                if (it->second->curFile) {
                    auto fIt = fileToInit.find(it->second->curFile.get());
                    if (fIt != fileToInit.end()) {
                        oldFI = fIt->second;
                    }
                }
                const bool newBetter =
                    (vd->TestAttr(AST::Attribute::SPECIFIC) &&
                        !it->second->TestAttr(AST::Attribute::SPECIFIC)) ||
                    (!vd->TestAttr(AST::Attribute::COMMON) &&
                        it->second->TestAttr(AST::Attribute::COMMON)) ||
                    (fileInitOwnsVar(newFI, *vd) && !fileInitOwnsVar(oldFI, *it->second));
                if (newBetter) {
                    for (auto& slot : uniqueVars) {
                        if (slot == it->second) {
                            slot = vd;
                            break;
                        }
                    }
                    it->second = vd;
                }
            }
        }
        auto orderedVars = OrderVarsByDependencies(uniqueVars);
        std::vector<Function*> orderedFileInits;
        std::unordered_set<Function*> seenFileInits;
        for (auto vd : orderedVars) {
            auto it = fileToInit.find(vd->curFile.get());
            if (it == fileToInit.end() || it->second == nullptr) {
                continue;
            }
            if (seenFileInits.insert(it->second).second) {
                orderedFileInits.push_back(it->second);
            }
        }
        // Drop stale file-init Applies (wrong order / empty common remnants), then re-append
        // in dependency order via InsertInitializerIntoPackageInitializer (move-to-end).
        Ptr<Block> block = GetBlockWithInitializers(*packageInit->GetBody());
        {
            auto exprs = block->GetExpressions();
            for (auto* expr : exprs) {
                auto* apply = DynamicCast<Apply>(expr);
                if (apply == nullptr) {
                    continue;
                }
                auto* callee = DynamicCast<Function>(apply->GetCallee());
                if (callee && IsCurrentPackageFileInit(*callee, curPackage.fullPackageName)) {
                    apply->RemoveSelfFromBlock();
                }
            }
        }
        for (auto* fileInit : orderedFileInits) {
            InsertInitializerIntoPackageInitializer(*fileInit, *packageInit);
        }
    } else {
        for (auto& fileAndVars : initOrder) {
            // maybe there isn't global var decl in one file, in this case, we don't generate file init func
            if (!fileAndVars.second.empty() || enableIncre) {
                if (!fileAndVars.second.empty() && IsSymbolImportedDecl(*fileAndVars.second[0], opts)) {
                    continue;
                }
                // only generate file init for this package, not imported files
                if (auto fileInit = TranslateFileInitializer(*fileAndVars.first, fileAndVars.second)) {
                    InsertInitializerIntoPackageInitializer(*fileInit, *packageInit);
                }
            }
        }
    }

    trans.CreateAndAppendTerminator<Exit>(curBlock);
}

inline std::pair<Function*, Block*> GlobalVarInitializer::PreparePackageLiteralInit(const AST::Package& curPackage)
{
    // create/use deserialized base of package initializer function.
    Function* packageLiteralInit = builder.GetCurPackage()->GetPackageLiteralInitFunc();
    Block* curBlock;
    if (packageLiteralInit) {
        curBlock = DropExitNodeOfInitializer(*packageLiteralInit);
        auto importsInitFunc = GetImportsInitFunc(curPackage, "_literal");
        UpdateImportsInit(curPackage, *importsInitFunc, "_literal");
        trans.SetCurrentBlock(*curBlock);
    } else {
        // create base of package init literal function.
        packageLiteralInit = GeneratePackageInitBase(curPackage, "_literal");
        builder.GetCurPackage()->SetPackageLiteralInitFunc(packageLiteralInit);
        curBlock = trans.GetCurrentBlock();
        curBlock->EnableAttr(Attribute::INITIALIZER);
        auto importsInitFunc = CreateImportsInitFunc(curPackage, "_literal");
        InsertInitializerIntoPackageInitializer(*importsInitFunc, *packageLiteralInit);
        UpdateImportsInit(curPackage, *importsInitFunc, "_literal");
        trans.SetCurrentBlock(*curBlock);
    }

    return std::make_pair(packageLiteralInit, curBlock);
}

/// Generate package literal values initializer
void GlobalVarInitializer::CreatePackageLiteralInit(const AST::Package& curPackage, const InitOrder& initOrder)
{
    // NOTE: this function is only called by CodeGen and only used for macro expand situation.
    auto [packageLiteralInit, curBlock] = PreparePackageLiteralInit(curPackage);

    for (auto& fileAndVars : initOrder) {
        // maybe there isn't global var decl in one file, in this case, we don't generate file init func
        if (!fileAndVars.second.empty()) {
            if (fileAndVars.second[0]->TestAttr(AST::Attribute::IMPORTED)) {
                for (auto var : fileAndVars.second) {
                    if (var->IsCommonOrSpecific()) {
                        // They are always initialized via init functions in corresponding file init,
                        // because e.g. it can be literal in common and not literal in specific,
                        // and therefore they are not inlined in importing package.
                        continue;
                    }

                    if (NeedVarLiteralInitFunc(*var)) {
                        auto vd = StaticCast<AST::VarDecl>(var);
                        auto globalVar = StaticCast<GlobalVar>(GetGlobalVariable(*vd));
                        auto initNode = Translator::TranslateASTNode(*vd->initializer, trans);
                        initNode->SetDebugLocation(INVALID_LOCATION);
                        trans.CreateAndAppendWrappedStore(*initNode, *globalVar, *curBlock);
                    }
                }
                continue;
            }
            if (auto fileLiteralInit = TranslateFileLiteralInitializer(*fileAndVars.first, fileAndVars.second)) {
                InsertInitializerIntoPackageInitializer(*fileLiteralInit, *packageLiteralInit);
            }
        }
    }

    trans.CreateAndAppendTerminator<Exit>(curBlock);
}

void GlobalVarInitializer::Run(const AST::Package& pkg, const InitOrder& initOrder)
{
    CreatePackageInit(pkg, initOrder);
    CreatePackageLiteralInit(pkg, initOrder);
}
