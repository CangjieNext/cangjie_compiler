// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file declares all terminators of CHIR, including Terminator itself and its sub class
 */

#ifndef CANGJIE_CHIR_TERMINATOR_H
#define CANGJIE_CHIR_TERMINATOR_H

#include "cangjie/CHIR/IR/Expression/Expression.h"

namespace Cangjie::CHIR {
class CHIRBuilder;
class ExprTypeConverter;

enum class SourceExpr : uint8_t {
    IF_EXPR,
    WHILE_EXPR,
    DO_WHILE_EXPR,
    MATCH_EXPR,
    IF_LET_OR_WHILE_LET,
    QUEST,
    BINARY,
    FOR_IN_EXPR,
    OTHER
};

/**
 * @brief Terminator class in CHIR.
 * Terminator is also an expression, but it can only be at the end of one block.
 */
class Terminator : public Expression {
    friend class Block;

protected:
    explicit Terminator(
        ExprKind kind, const std::vector<Value*>& operands, const std::vector<Block*>& successors, Block* parent);
    ~Terminator() override = default;
};

/**
 * @brief Jump from current block to another one
 */
class GoTo : public Terminator {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    Block* GetDestination() const;

private:
    explicit GoTo(Block* destBlock, Block* parent);
    ~GoTo() override = default;

    GoTo* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `if`, `for-in`, `while`, `do-while` and `match` can be translated to `Branch`
 */
class Branch : public Terminator {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /** @brief Get the condition of this Branch Expression */
    Value* GetCondition() const;

    /** @brief Get the true block of this Branch Expression */
    Block* GetTrueBlock() const;

    /** @brief Get the false block of this Branch Expression */
    Block* GetFalseBlock() const;

    /** @brief Set the source expr, mark where this branch is from */
    void SetSourceExpr(SourceExpr srcExpr);

    /** @brief Get the source expr */
    SourceExpr GetSourceExpr() const;

protected:
    std::string AddExtraComment() const override;

private:
    explicit Branch(Value* cond, Block* trueBlock, Block* falseBlock, Block* parent);
    ~Branch() override = default;

    SourceExpr sourceExpr{SourceExpr::OTHER};

    Branch* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `match` can be translated to `MultiBranch` in O2
 */
class MultiBranch : public Terminator {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /** @brief Get the condition of this MultiBranch Expression */
    Value* GetCondition() const;

    /** @brief Get the case values of this MultiBranch Expression */
    const std::vector<uint64_t>& GetCaseVals() const;

    /** @brief Get the case value by index of this MultiBranch Expression */
    uint64_t GetCaseValByIndex(size_t index) const;

    /** @brief Get the case block by index of this MultiBranch Expression */
    Block* GetCaseBlockByIndex(size_t index) const;

    /** @brief Get the default block of this MultiBranch Expression */
    Block* GetDefaultBlock() const;

    std::vector<Block*> GetNormalBlocks() const;

protected:
    std::string OperandsToString() const override;

private:
    explicit MultiBranch(Value* cond, Block* defaultBlock, const std::vector<uint64_t>& vals,
        const std::vector<Block*>& succs, Block* parent);
    ~MultiBranch() override = default;

    MultiBranch* Clone(CHIRBuilder& builder, Block& parent) const override;

    /**
     * @brief The specific case values used to match.
     * Note that default Block does not have the case val.
     */
    std::vector<uint64_t> caseVals;
};

/**
 * @brief Exit current function.
 */
class Exit : public Terminator {
    friend class CHIRContext;
    friend class CHIRBuilder;
private:
    explicit Exit(Block* parent);
    ~Exit() override = default;

    Exit* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief Throw an exception.
 */
class RaiseException : public Terminator {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /** @brief Get the exception value of this RaiseException Expression */
    Value* GetExceptionValue() const;

    /**
     * @brief Get the exception block of this RaiseException Expression.
     *
     *  Return exception block if exist,
     *  nullptr,  otherwise.
     */
    Block* GetExceptionBlock() const;

private:
    explicit RaiseException(Value* value, Block* parent);
    explicit RaiseException(Value* value, Block* successor, Block* parent);
    ~RaiseException() override = default;

    RaiseException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

class ExpressionWithException : public Terminator {
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

protected:
    explicit ExpressionWithException(ExprKind kind, Block* parent);
    explicit ExpressionWithException(
        ExprKind kind, const std::vector<Value*>& operands, const std::vector<Block*>& successors, Block* parent);
};

/**
 * @brief `Apply` expression written in `try` block
 */
class ApplyWithException : public ApplyBase {
    friend class ExprTypeConverter;
    friend class TypeConverterForCC;
    friend class CHIRSerializer;
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit ApplyWithException(
        Value* callee, const FuncCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent);
    ~ApplyWithException() override = default;

    ApplyWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `Invoke` expression written in `try` block
 */
class InvokeWithException : public InvokeBase {
    friend class ExprTypeConverter;
    friend class TypeConverterForCC;
    friend class PrivateTypeConverterNoInvokeOriginal;
    friend class CHIRSerializer;
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit InvokeWithException(
        const InvokeCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent);

    ~InvokeWithException() override = default;

    InvokeWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `InvokeStatic` expression written in `try` block
 */
class InvokeStaticWithException : public InvokeStaticBase {
    friend class ExprTypeConverter;
    friend class TypeConverterForCC;
    friend class PrivateTypeConverterNoInvokeOriginal;
    friend class CHIRSerializer;
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit InvokeStaticWithException(
        const InvokeCallContext& callContext, Block* sucBlock, Block* errBlock, Block* parent);

    ~InvokeStaticWithException() override = default;

    InvokeStaticWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `Unary` expression written in `try` block (Neg only).
 */
class UnaryExpressionWithException : public UnaryExpressionBase {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit UnaryExpressionWithException(
        UnaryExprKind unaryKind, Value* operand, Block* normal, Block* exception, Block* parent);
    ~UnaryExpressionWithException() override = default;

    UnaryExpressionWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `Binary` expression written in `try` block.
 */
class BinaryExpressionWithException : public BinaryExpressionBase {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit BinaryExpressionWithException(BinaryExprKind binaryKind, Value* lhs, Value* rhs, OverflowStrategy ofs,
        Block* normal, Block* exception, Block* parent);
    ~BinaryExpressionWithException() override = default;

    BinaryExpressionWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `NumericCast` expression written in `try` block
 */
class NumericCastWithException : public NumericCastBase {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit NumericCastWithException(Value* operand, Block* normal, Block* exception, Block* parent);
    ~NumericCastWithException() override = default;

    NumericCastWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `Intrinsic` expression wroten in `try` block
 */
class IntrinsicWithException : public ExpressionWithException {
    friend class ExprTypeConverter;
    friend class TypeConverterForCC;
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the intrinsic kind.
     *
     * @return The intrinsic kind.
     */
    CHIR::IntrinsicKind GetIntrinsicKind() const;

    /**
     * @brief Retrieves the generic type information.
     *
     * @return A vector of pointers to the generic types.
     */
    const std::vector<Type*>& GetInstantiatedTypeArgs() const;

    /**
     * @brief Retrieves the arguments of the intrinsic operation.
     *
     * @return A vector of pointers to the arguments.
     */
    const std::vector<Value*> GetArgs() const;

protected:
    std::string OperandsToString() const override;

private:
    explicit IntrinsicWithException(
        const IntrisicCallContext& callContext, Block* normal, Block* exception, Block* parent);
    ~IntrinsicWithException() override = default;

    IntrinsicWithException* Clone(CHIRBuilder& builder, Block& parent) const override;

    CHIR::IntrinsicKind intrinsicKind;
    std::vector<Type*> instantiatedTypeArgs;
};

/**
 * @brief `Allocate` expression written in `try` block
 */
class AllocateWithException : public AllocateBase {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit AllocateWithException(Type* ty, Block* normal, Block* exception, Block* parent);
    ~AllocateWithException() override = default;

    AllocateWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `RawArrayAllocate` expression written in `try` block
 */
class RawArrayAllocateWithException : public RawArrayAllocateBase {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit RawArrayAllocateWithException(Type* eleTy, Value* size, Block* normal, Block* exception, Block* parent);
    ~RawArrayAllocateWithException() override = default;

    RawArrayAllocateWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};

/**
 * @brief `Spawn` expression written in `try` block
 */
class SpawnWithException : public SpawnBase {
    friend class CHIRContext;
    friend class CHIRBuilder;
public:
    // ===--------------------------------------------------------------------===//
    // Base Information
    // ===--------------------------------------------------------------------===//
    /**
     * @brief Retrieves the success block.
     *
     * @return The success block.
     */
    Block* GetSuccessBlock() const;

    /**
     * @brief Retrieves the error block.
     *
     * @return The error block.
     */
    Block* GetErrorBlock() const;

private:
    explicit SpawnWithException(
        Value* val, Value* arg, Block* normal, Block* exception, Block* parent);
    explicit SpawnWithException(
        Value* val, Block* normal, Block* exception, Block* parent);
    ~SpawnWithException() override = default;

    SpawnWithException* Clone(CHIRBuilder& builder, Block& parent) const override;
};
} // namespace Cangjie::CHIR
#endif // CANGJIE_CHIR_EXPRESSION_H
