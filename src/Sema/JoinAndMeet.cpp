// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements utility functions for JoinAndMeet.
 */

#include "JoinAndMeet.h"

#include "TypeCheckUtil.h"

#include "cangjie/AST/Nodes/Node.h"
#include "cangjie/AST/Types.h"
#include "cangjie/Utils/CheckUtils.h"

using namespace Cangjie;
using namespace AST;
using namespace TypeCheckUtil;
using ErrMsg = std::stack<std::string>;
using ErrOrTy = std::variant<ErrMsg, AST::ModalTy>;

namespace {
enum class Uniformity {
    UNIFORMED,
    MIXED,
    ALL_IRRELEVANT
};

Uniformity CheckFuncUniformity(const std::set<ModalTy>& tys)
{
    size_t paramCnt = 0;
    bool anyFuncTy = false;
    bool anyNonFuncTy = false;
    for (auto& ty : tys) {
        if (auto funcTy = DynamicCast<FuncTy>(ty)) {
            size_t curCnt = funcTy->paramTys.size();
            if (!anyFuncTy) {
                paramCnt = curCnt;
            } else if (paramCnt != curCnt) {
                return Uniformity::MIXED;
            }
            anyFuncTy = true;
        } else {
            anyNonFuncTy = true;
        }
        if (anyFuncTy && anyNonFuncTy) {
            return Uniformity::MIXED;
        }
    }
    if (!anyFuncTy) {
        return Uniformity::ALL_IRRELEVANT;
    }
    return Uniformity::UNIFORMED;
}

Uniformity CheckTupleUniformity(const std::set<ModalTy>& tys)
{
    size_t argCnt = 0;
    bool anyTupleTy = false;
    bool anyNonTupleTy = false;
    for (auto& ty : tys) {
        if (auto tupleTy = DynamicCast<TupleTy>(ty)) {
            size_t curCnt = tupleTy->typeArgs.size();
            if (!anyTupleTy) {
                argCnt = curCnt;
            } else if (argCnt != curCnt) {
                return Uniformity::MIXED;
            }
            anyTupleTy = true;
        } else {
            anyNonTupleTy = true;
        }
        if (anyTupleTy && anyNonTupleTy) {
            return Uniformity::MIXED;
        }
    }
    if (!anyTupleTy) {
        return Uniformity::ALL_IRRELEVANT;
    }
    return Uniformity::UNIFORMED;
}
} // namespace

ErrOrTy JoinAndMeet::Join(bool sprsErr)
{
    // Hot fix.
    if (!IsInputValid()) {
        return {TypeManager::GetInvalidTy()};
    }
    auto jTy = BatchJoin(tySet);
    if (sprsErr || errMsg.empty()) {
        return {jTy};
    } else {
        return {errMsg};
    }
}

ErrOrTy JoinAndMeet::JoinAsVisibleTy()
{
    // Hot fix.
    if (!IsInputValid()) {
        return {TypeManager::GetInvalidTy()};
    }
    auto jTy = BatchJoin(tySet);
    this->isForcedToUserVisible = true;
    jTy = ToUserVisibleTy(jTy);
    AddFinalErrMsgs(*jTy, true);
    if (errMsg.empty()) {
        return {jTy};
    } else {
        return {errMsg};
    }
}

AST::ModalTy JoinAndMeet::BatchJoin(const std::set<ModalTy>& tys)
{
    auto isSubtype = [this](ModalTy ty1, ModalTy ty2) { return tyMgr.IsSubtype(ty1, ty2); };
    auto isSupertype = [this](ModalTy ty1, ModalTy ty2) { return tyMgr.IsSubtype(ty2, ty1); };
    auto doBatchJoin = [this](const std::set<AST::ModalTy>& tys) {return BatchJoin(tys);};
    auto doBatchMeet = [this](const std::set<AST::ModalTy>& tys) {return BatchMeet(tys);};
    DualMode joinMode = {.bound = tyMgr.GetAnyTy(),
        .coFunc = doBatchJoin,
        .contraFunc = doBatchMeet,
        .coSubtyFunc = isSubtype};
    std::set<ModalTy> realTys;
    std::function<void(ModalTy)> insertRealTy = [this, &realTys, &insertRealTy](ModalTy ty) {
        if (auto tyVar = DynamicCast<TyVar*>(ty); (tyVar && Utils::In(tyVar, ignoredTyVars)) || ty->IsNothing()) {
            return;
        }
        if (auto unionTy = DynamicCast<UnionTy>(ty)) {
            for (auto uty : unionTy->tys) {
                insertRealTy(uty);
            }
        } else {
            realTys.insert(ty);
        }
    };
    for (auto ty : tys) {
        insertRealTy(ty);
    }
    PData::CommitScope cs(tyMgr.constraints);
    if (auto ret = FindSmallestTy(realTys, isSupertype); ret && !ret->IsInvalid()) {
        return ret;
    }
    PData::Reset(tyMgr.constraints);
    if (auto funcTyJoin = JoinOrMeetFuncTy(joinMode, realTys)) {
        return funcTyJoin;
    }
    PData::Reset(tyMgr.constraints);
    if (auto tupleTyJoin = JoinOrMeetTupleTy(joinMode, realTys)) {
        return tupleTyJoin;
    }
    PData::Reset(tyMgr.constraints);
    auto common = tyMgr.GetAllCommonSuperTys(std::unordered_set<ModalTy>(realTys.begin(), realTys.end()));
    if (curFile) {
        Utils::EraseIf(common, [this](ModalTy ty) { return !impMgr->IsTyAccessible(*curFile, *ty); });
    }
    if (common.empty()) {
        return tyMgr.GetAnyTy();
    }
    auto ret = FindSmallestTy(std::set<ModalTy>(common.begin(), common.end()), isSubtype);
    // reset unnecessary constaints from finding possible supertypes (e.g. those claimed by conditional extensions),
    // and re-enforce necessary constraints by judging the common supertype again
    PData::Reset(tyMgr.constraints);
    if (ret->IsInvalid() || !LessThanAll(ret, realTys, isSupertype)) {
        PData::Reset(tyMgr.constraints);
    }
    return ret;
}

/**
 * returns:
 *      - the joined/met FuncTy if all tys are FuncTy and the LUB/GLB exists
 *      - AnyTy/Nothing if there exists any FuncTy but the LUB/GLB doesn't exist
 *      - nullptr if there is no FuncTy in tys
 */
AST::ModalTy JoinAndMeet::JoinOrMeetFuncTy(const DualMode& mode, const std::set<ModalTy>& tys)
{
    auto uniformity = CheckFuncUniformity(tys);
    switch (uniformity) {
        case Uniformity::ALL_IRRELEVANT:
            return nullptr;
        case Uniformity::MIXED:
            return mode.bound;
        default:
            break;
    }
    size_t paramCnt = RawStaticCast<FuncTy*>(*tys.begin())->paramTys.size();
    std::vector<ModalTy> paramTys(paramCnt);
    for (size_t i = 0; i < paramCnt; i++) {
        std::set<ModalTy> operandParamTys;
        for (auto ty : tys) {
            operandParamTys.insert(RawStaticCast<FuncTy*>(ty)->paramTys[i]);
        }
        paramTys[i] = mode.contraFunc(operandParamTys);
    }
    std::set<ModalTy> operandRetTys;
    for (auto ty : tys) {
        operandRetTys.insert(RawStaticCast<FuncTy*>(ty)->retTy);
    }
    auto retTy = mode.coFunc(operandRetTys);
    if (Ty::AreTysCorrect(paramTys) && Ty::IsTyCorrect(retTy)) {
        auto resultTy = tyMgr.GetFunctionTy(paramTys, retTy);
        CJC_NULLPTR_CHECK(resultTy);
        for (auto ty : tys) {
            if (!mode.coSubtyFunc(ty, resultTy)) {
                return mode.bound;
            }
        }
        return resultTy;
    }
    return mode.bound;
}

/**
 * returns:
 *      - the joined/met TupleTy if all tys are TupleTy and the LUB/GLB exists
 *      - AnyTy/Nothing if there exists any TupleTy but the LUB/GLB doesn't exist
 *      - nullptr if there is no TupleTy in tys
 */
AST::ModalTy JoinAndMeet::JoinOrMeetTupleTy(const DualMode& mode, const std::set<ModalTy>& tys)
{
    auto uniformity = CheckTupleUniformity(tys);
    switch (uniformity) {
        case Uniformity::ALL_IRRELEVANT:
            return nullptr;
        case Uniformity::MIXED:
            return mode.bound;
        default:
            break;
    }
    size_t argCnt = RawStaticCast<TupleTy*>(*tys.begin())->typeArgs.size();
    std::vector<ModalTy> typeArgs(argCnt);
    for (size_t i = 0; i < argCnt; i++) {
        std::set<ModalTy> operandTyArgs;
        for (auto& ty : tys) {
            operandTyArgs.insert(RawStaticCast<TupleTy*>(ty)->typeArgs[i]);
        }
        typeArgs[i] = mode.coFunc(operandTyArgs);
    }
    if (Ty::AreTysCorrect(typeArgs)) {
        auto resultTy = tyMgr.GetTupleTy(typeArgs);
        for (auto ty : tys) {
            if (!mode.coSubtyFunc(ty, resultTy)) {
                return mode.bound;
            }
        }
        return resultTy;
    }
    return mode.bound;
}

ErrOrTy JoinAndMeet::Meet(bool sprsErr)
{
    // Hot fix.
    if (!IsInputValid()) {
        return {TypeManager::GetInvalidTy()};
    }
    auto mTy = BatchMeet(tySet);
    if (sprsErr || errMsg.empty()) {
        return {mTy};
    } else {
        return {errMsg};
    }
}

ErrOrTy JoinAndMeet::MeetAsVisibleTy()
{
    // Hot fix.
    if (!IsInputValid()) {
        return {TypeManager::GetInvalidTy()};
    }
    auto mTy = BatchMeet(tySet);
    this->isForcedToUserVisible = true;
    mTy = ToUserVisibleTy(mTy);
    AddFinalErrMsgs(*mTy, false);
    if (errMsg.empty()) {
        return {mTy};
    } else {
        return {errMsg};
    }
}

AST::ModalTy JoinAndMeet::BatchMeet(const std::set<ModalTy>& tys)
{
    auto isSubtype = [this](ModalTy ty1, ModalTy ty2) { return tyMgr.IsSubtype(ty1, ty2); };
    auto isSupertype = [this](ModalTy ty1, ModalTy ty2) { return tyMgr.IsSubtype(ty2, ty1); };
    auto doBatchJoin = [this](const std::set<AST::ModalTy>& tys) {return BatchJoin(tys);};
    auto doBatchMeet = [this](const std::set<AST::ModalTy>& tys) {return BatchMeet(tys);};
    DualMode meetMode = {.bound = TypeManager::GetInvalidTy(),
        .coFunc = doBatchMeet,
        .contraFunc = doBatchJoin,
        .coSubtyFunc = isSupertype};
    std::set<ModalTy> realTys;
    std::function<void(ModalTy)> insertRealTy = [this, &realTys, &insertRealTy](ModalTy ty) {
        if (auto tyVar = DynamicCast<TyVar*>(ty); tyVar && Utils::In(tyVar, ignoredTyVars)) {
            return;
        }
        if (auto unionTy = DynamicCast<UnionTy>(ty)) {
            insertRealTy(BatchJoin(unionTy->tys));
        }
        if (auto itsTy = DynamicCast<IntersectionTy>(ty)) {
            for (auto ity : itsTy->tys) {
                insertRealTy(ity);
            }
        } else {
            realTys.insert(ty);
        }
    };
    for (auto ty : tys) {
        insertRealTy(ty);
    }
    PData::CommitScope cs(tyMgr.constraints);
    if (auto ret = FindSmallestTy(realTys, isSubtype); ret && !ret->IsInvalid()) {
        return ret;
    }
    PData::Reset(tyMgr.constraints);
    if (auto funcTyJoin = JoinOrMeetFuncTy(meetMode, realTys)) {
        return funcTyJoin;
    }
    PData::Reset(tyMgr.constraints);
    if (auto tupleTyJoin = JoinOrMeetTupleTy(meetMode, realTys)) {
        return tupleTyJoin;
    }
    PData::Reset(tyMgr.constraints);
    return TypeManager::GetInvalidTy();
}

std::string JoinAndMeet::CombineErrMsg(ErrMsg& msgs)
{
    std::string res{};
    res.append("Traces:\n");

    while (!msgs.empty()) {
        res.append(msgs.top() + "\n");
        msgs.pop();
    }
    return res;
}

ModalTy JoinAndMeet::ToUserVisibleTy(ModalTy ty)
{
    CJC_NULLPTR_CHECK(ty);
    if (ty->IsIntersection()) {
        auto iSecTy = RawStaticCast<IntersectionTy*>(ty);
        auto isSubtype = [this](ModalTy ty1, ModalTy ty2) { return tyMgr.IsSubtype(ty1, ty2); };
        ModalTy res = ToUserVisibleTy(FindSmallestTy(iSecTy->tys, isSubtype));
        // Given C1 <: I1 & I2 and C2 <: I1 & I2, then Join(C1, C2) gives I1 & I2.
        // Meet(I1, I2) gives Nothing but the result for the original Join.
        return res->IsNothing() ? tyMgr.GetAnyTy() : res;
    } else if (auto unionTy = DynamicCast<UnionTy>(ty)) {
        std::set<ModalTy> uTys;
        for (auto& t : unionTy->tys) {
            uTys.insert(ToUserVisibleTy(t));
        }
        auto res = BatchJoin(uTys);
        // Dual of the above comments.
        return res->IsAny() ? TypeManager::GetNothingTy(res->Modal()) : res;
    } else if (ty->IsFunc()) {
        auto funcTy = RawStaticCast<FuncTy*>(ty);
        auto retTy = ToUserVisibleTy(funcTy->retTy);
        auto paramTys = funcTy->paramTys;
        std::transform(funcTy->paramTys.begin(), funcTy->paramTys.end(), paramTys.begin(),
            [this](ModalTy typ) { return ToUserVisibleTy(typ); });
        if (Ty::AreTysCorrect(paramTys) && Ty::IsTyCorrect(retTy)) {
            return tyMgr.GetFunctionTy(paramTys, retTy, {funcTy->isC, funcTy->isClosureTy, funcTy->hasVariableLenArg});
        } else {
            return TypeManager::GetInvalidTy();
        }
    } else if (ty->IsTuple()) {
        auto tupleTy = RawStaticCast<TupleTy*>(ty);
        auto elemTys = tupleTy->typeArgs;
        std::transform(tupleTy->typeArgs.begin(), tupleTy->typeArgs.end(), elemTys.begin(),
            [this](ModalTy typ) { return ToUserVisibleTy(typ); });
        if (Ty::AreTysCorrect(elemTys)) {
            return tyMgr.GetTupleTy(elemTys);
        } else {
            return TypeManager::GetInvalidTy();
        }
    } else {
        return ty;
    }
}

void JoinAndMeet::AddFinalErrMsgs(const Ty& ty, bool isJoin)
{
    auto getTysStr = [this]() {
        auto tyVec = Utils::SetToVec<ModalTy>(tySet);
        std::sort(tyVec.begin(), tyVec.end(), CompTyByNames);
        std::string tysStr;
        for (auto it = tyVec.begin(); it != std::prev(tyVec.end()); ++it) {
            tysStr += (it == tyVec.begin() ? std::string() : ", ") + "'" + Ty::ToString(*it) + "'";
        }
        if (tyVec.size() > 1) {
            CJC_NULLPTR_CHECK(tyVec.back());
            tysStr += " and '" + tyVec.back()->String() + "'";
        }
        return tysStr;
    };

    if (ty.IsInvalid()) {
        CJC_ASSERT(!tySet.empty());
        std::string newErrMsg = "The types " + getTysStr() + " do not have ";
        newErrMsg += isJoin ? "the smallest common supertype" : "the greatest common subtype";
        errMsg.push(newErrMsg);
        return;
    }
}

std::pair<std::optional<std::string>, ModalTy> JoinAndMeet::SetJoinedType(
    ModalTy ty, std::variant<std::stack<std::string>, ModalTy>& joinRes)
{
    if (std::get_if<ModalTy>(&joinRes)) {
        ty = std::get<ModalTy>(joinRes);
        return {{}, ty};
    }
    ty = TypeManager::GetInvalidTy();
    auto errMsgs = std::get<std::stack<std::string>>(joinRes);
    return {JoinAndMeet::CombineErrMsg(errMsgs), ty};
}

std::pair<std::optional<std::string>, ModalTy> JoinAndMeet::SetMetType(
    ModalTy ty, std::variant<std::stack<std::string>, ModalTy>& metRes)
{
    return SetJoinedType(ty, metRes);
}

bool JoinAndMeet::IsInputValid() const
{
    bool isValid = true;
    if (tySet.empty()) {
        isValid = false;
    }
    if (!Ty::AreTysCorrect(tySet)) {
        isValid = false;
    }
    return isValid;
}
