// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file implements codegen Wrappers for CHIR Expression.
 */
#ifndef CANGJIE_CHIREXPRWRAPPER_H
#define CANGJIE_CHIREXPRWRAPPER_H

#include "cangjie/Basic/Print.h"
#include "cangjie/CHIR/Utils/CHIRCasting.h"
#include "cangjie/CHIR/IR/Expression/Terminator.h"
#include "cangjie/CHIR/Utils/Utils.h"

#include <llvm/IR/BasicBlock.h>

namespace Cangjie {
namespace CodeGen {
class CHIRExprWrapper {
public:
    explicit CHIRExprWrapper(const CHIR::Expression& chirExpr) : chirExpr(chirExpr)
    {
    }

    virtual ~CHIRExprWrapper() = default;

    const CHIR::Expression& GetChirExpr() const
    {
        return chirExpr;
    }

    CHIR::ExprMajorKind GetExprMajorKind() const
    {
        return chirExpr.GetExprMajorKind();
    }

    CHIR::ExprKind GetExprKind() const
    {
        return chirExpr.GetExprKind();
    }

    bool IsConstant() const
    {
        return Is<CHIR::Constant>(chirExpr);
    }

    bool IsConstantNull() const
    {
        return chirExpr.IsConstantNull();
    }

    bool IsConstantInt() const
    {
        return chirExpr.IsConstantInt();
    }

    bool IsConstantString() const
    {
        return chirExpr.IsConstantString();
    }

    std::string GetExprKindName() const
    {
        return chirExpr.GetExprKindName();
    }

    CHIR::Block* GetParentBlock() const
    {
        return chirExpr.GetParentBlock();
    }

    CHIR::Function* GetTopLevelFunc() const
    {
        return chirExpr.GetTopLevelFunc();
    }

    unsigned GetNumOfNonSuccessorOperands() const
    {
        return chirExpr.GetNumOfNonSuccessorOperands();
    }

    CHIR::Value* GetOperand(unsigned idx) const
    {
        return chirExpr.GetOperand(idx);
    }

    CHIR::LocalVar* GetResult() const
    {
        return chirExpr.GetResult();
    }

    std::string ToString(unsigned indent = 0) const
    {
        return chirExpr.ToString(indent);
    }

    bool IsTerminator() const
    {
        return chirExpr.IsTerminator();
    }

    // Get the value of the annotation T associated to this node
    template <typename T> typename std::invoke_result<decltype(T::Extract), const T*>::type Get() const
    {
        return chirExpr.Get<T>();
    }

    const CHIR::DebugLocation& GetDebugLocation() const
    {
        return chirExpr.GetDebugLocation();
    }

    void Dump() const
    {
        chirExpr.Dump();
    }

protected:
    const CHIR::Expression& chirExpr;
};

class CHIRCallExpr : public CHIRExprWrapper {
public:
    explicit CHIRCallExpr(const CHIR::Expression& chirExpr) : CHIRExprWrapper(chirExpr)
    {
    }
    virtual CHIR::Type* GetThisType(bool fromObj = true) const = 0;
    virtual std::vector<CHIR::Type*> GetInstantiatedTypeArgs() const = 0;
    virtual bool IsCalleeMethod() const = 0;
    virtual bool IsCalleeStructInstanceMethod() const = 0;
    virtual bool IsCalleeStatic() const = 0;
    virtual const CHIR::Type* GetOuterType([[maybe_unused]] CHIR::CHIRBuilder& builder) const = 0;
    virtual const CHIR::Value* GetThisParam() const = 0;
    virtual std::vector<CHIR::Value*> GetArgs() const = 0;
};

class CHIRApplyWrapper : public CHIRCallExpr {
public:
    explicit CHIRApplyWrapper(const CHIR::ApplyBase& apply) : CHIRCallExpr(apply)
    {
        if (GetInstantiatedTypeArgs().size() != GetCalleeTypeArgsNum()) {
#ifndef NDEBUG
            Errorln(chirExpr.ToString(0) + "\n");
#endif
            CJC_ASSERT_WITH_MSG(false, "Incorrect ApplyExpr from CHIR, type arguments are missing.");
        }
    }

    ~CHIRApplyWrapper() override = default;

    CHIR::Value* GetCallee() const
    {
        return GetApply().GetCallee();
    }

    std::vector<CHIR::Value*> GetArgs() const override
    {
        return GetApply().GetArgs();
    }

    std::vector<CHIR::Type*> GetInstantiatedTypeArgs() const override
    {
        return GetApply().GetInstantiatedTypeArgs();
    }

    CHIR::Type* GetThisType(bool fromObj = true) const override
    {
        (void)fromObj;
        return GetApply().GetThisType();
    }

    bool IsCalleeMethod() const override
    {
        bool isCallee = false;
        if (auto func = DynamicCast<CHIR::Function*>(GetCallee())) {
            isCallee = func->IsMemberFunc();
        }
        return isCallee;
    }

    bool IsCalleeStatic() const override
    {
        return GetCallee()->TestAttr(CHIR::Attribute::STATIC);
    }

    bool IsCalleeStructInstanceMethod() const override
    {
        if (!IsCalleeMethod() || IsCalleeStatic()) {
            return false;
        }

        auto outer = StaticCast<CHIR::Function*>(GetCallee())->GetOuterDeclaredOrExtendedDef();
        return outer && outer->IsStruct();
    }

    bool IsCalleeStructMutOrCtorMethod() const
    {
        return IsCalleeStructInstanceMethod() &&
            (GetCallee()->TestAttr(CHIR::Attribute::MUT) || CHIR::IsConstructor(*GetCallee()));
    }

    const CHIR::Type* GetOuterType(CHIR::CHIRBuilder& builder) const override
    {
        CHIR::Type* res = GetApply().GetInstParentCustomTyOfCallee(builder);
        if (!res) {
#ifndef NDEBUG
            Errorln("Should not get a nullptr:\n", chirExpr.ToString(0));
#endif
            CJC_ASSERT(false);
        }
        return res;
    }

    const CHIR::Value* GetThisParam() const override
    {
        return IsCalleeMethod() && !IsCalleeStatic() ? GetArgs()[0] : nullptr;
    }

private:
    const CHIR::ApplyBase& GetApply() const
    {
        return StaticCast<const CHIR::ApplyBase&>(chirExpr);
    }

    size_t GetCalleeTypeArgsNum() const
    {
        if (GetCallee()->IsFunc()) {
            return StaticCast<CHIR::Function*>(GetCallee())->GetGenericTypeParams().size();
        }
        return 0;
    }
};

class CHIRInvokeExpr : public CHIRCallExpr {
public:
    explicit CHIRInvokeExpr(const CHIR::Expression& chirExpr) : CHIRCallExpr(chirExpr)
    {
    }

    virtual std::size_t GetVirtualMethodOffset() const = 0;

    void SetPrepForVirtualCallBB(llvm::BasicBlock* prepForVirtualCallBB)
    {
        this->prepForVirtualCallBB = prepForVirtualCallBB;
    }

    llvm::BasicBlock* GetPrepForVirtualCallBB() const
    {
        return prepForVirtualCallBB;
    }

private:
    llvm::BasicBlock* prepForVirtualCallBB = nullptr;
};

class CHIRInvokeWrapper : public CHIRInvokeExpr {
public:
    explicit CHIRInvokeWrapper(const CHIR::InvokeBase& invoke) : CHIRInvokeExpr(invoke)
    {
    }

    ~CHIRInvokeWrapper() override = default;

    CHIR::Value* GetObject() const
    {
        return GetInvoke().GetObject();
    }

    std::string GetMethodName() const
    {
        return GetInvoke().GetMethodName();
    }

    CHIR::FuncType* GetMethodType() const
    {
        return GetInvoke().GetMethodType();
    }

    std::vector<CHIR::Value*> GetArgs() const override
    {
        return GetInvoke().GetArgs();
    }

    std::vector<CHIR::Type*> GetInstantiatedTypeArgs() const override
    {
        return GetInvoke().GetInstantiatedTypeArgs();
    }

    CHIR::Type* GetThisType(bool fromObj = true) const override
    {
        return fromObj ? GetInvoke().GetObject()->GetType() : GetInvoke().GetThisType();
    }

    bool IsCalleeMethod() const override
    {
        return true;
    }

    bool IsCalleeStatic() const override
    {
        return false;
    }

    bool IsCalleeStructInstanceMethod() const override
    {
        return false;
    }

    const CHIR::Type* GetOuterType(CHIR::CHIRBuilder& builder) const override
    {
        CJC_ASSERT(!IsCalleeStatic());
        CHIR::Type* res = GetInvoke().GetInstSrcParentCustomTypeOfMethod(builder);
        if (!res) {
#ifndef NDEBUG
            Errorln("Should not get a nullptr:\n", chirExpr.ToString(0));
#endif
            CJC_ASSERT(false);
        }
        return res;
    }

    const CHIR::Value* GetThisParam() const override
    {
        CJC_ASSERT(!IsCalleeStatic());
        return GetObject();
    }

    std::size_t GetVirtualMethodOffset() const override
    {
        return GetInvoke().GetVirtualMethodOffset();
    }

    bool TestVritualMethodAttr(CHIR::Attribute attr) const
    {
        return GetInvoke().GetVirtualMethodAttr().TestAttr(attr);
    }

private:
    const CHIR::InvokeBase& GetInvoke() const
    {
        return StaticCast<const CHIR::InvokeBase&>(chirExpr);
    }
};

class CHIRInvokeStaticWrapper : public CHIRInvokeExpr {
public:
    explicit CHIRInvokeStaticWrapper(const CHIR::InvokeStaticBase& invokeStatic) : CHIRInvokeExpr(invokeStatic)
    {
    }

    ~CHIRInvokeStaticWrapper() override = default;

    std::string GetMethodName() const
    {
        return GetInvokeStatic().GetMethodName();
    }

    CHIR::FuncType* GetMethodType() const
    {
        return GetInvokeStatic().GetMethodType();
    }

    CHIR::Value* GetRTTIValue() const
    {
        return GetInvokeStatic().GetRTTIValue();
    }

    std::vector<CHIR::Value*> GetArgs() const override
    {
        return GetInvokeStatic().GetArgs();
    }

    CHIR::Type* GetThisType(bool fromObj = true) const override
    {
        (void)fromObj;
        return GetInvokeStatic().GetThisType();
    }

    std::vector<CHIR::Type*> GetInstantiatedTypeArgs() const override
    {
        return GetInvokeStatic().GetInstantiatedTypeArgs();
    }

    bool IsCalleeMethod() const override
    {
        return true;
    }

    bool IsCalleeStatic() const override
    {
        return true;
    }

    bool IsCalleeStructInstanceMethod() const override
    {
        return false;
    }

    const CHIR::Type* GetOuterType(CHIR::CHIRBuilder& builder) const override
    {
        CJC_ASSERT(IsCalleeStatic());
        CHIR::Type* res = GetInvokeStatic().GetInstSrcParentCustomTypeOfMethod(builder);
        if (!res) {
#ifndef NDEBUG
            Errorln("Should not get a nullptr:\n", chirExpr.ToString(0));
#endif
            CJC_ASSERT(false);
        }
        return res;
    }

    const CHIR::Value* GetThisParam() const override
    {
        CJC_ASSERT(false && "InvokeStatic doesn't have this param.");
        return nullptr;
    }

    std::size_t GetVirtualMethodOffset() const override
    {
        return GetInvokeStatic().GetVirtualMethodOffset();
    }

private:
    const CHIR::InvokeStaticBase& GetInvokeStatic() const
    {
        return StaticCast<const CHIR::InvokeStaticBase&>(chirExpr);
    }
};

} // namespace CodeGen
} // namespace Cangjie
#endif // CANGJIE_CHIREXPRWRAPPER_H
