// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Base/CGTypes/CGVArrayType.h"

#include "CGContext.h"
#include "CGModule.h"
#include "CGPkgContext.h"

namespace Cangjie::CodeGen {
void CGVArrayType::CalculateSizeAndAlign()
{
    if (auto arrayType = llvm::dyn_cast<llvm::ArrayType>(llvmType)) {
        auto layOut = cgMod.GetLLVMModule()->getDataLayout();
        size = layOut.getTypeAllocSize(arrayType);
        align = layOut.getABITypeAlignment(arrayType);
    }
}

llvm::Type* CGVArrayType::GenLLVMType()
{
    if (llvmType) {
        return llvmType;
    }

    auto& varrType = StaticCast<const CHIR::VArrayType&>(chirType);
    auto varrElementType = varrType.GetElementType();
    auto elemLLVMType = CGType::GetOrCreate(cgMod, varrElementType)->GetLLVMType();
    uint64_t numOfElements = varrType.GetSize();

    // A VArray is a stack-allocated value whose total byte size is numOfElements * elementByteSize.
    // The 2GB stacksize guard in the LLVM backend rejects oversized VArrays, but only when the
    // byte-size computation does not silently wrap to a small value. When numOfElements *
    // elementByteSize overflows uint64 and lands below 2GB, that guard is bypassed and cjc
    // emits a broken binary that crashes at runtime. Detect this wraparound here and reject it
    // as a compile error. (Overflows that still land above 2GB, and legitimately large VArrays,
    // keep being handled by the existing 2GB stacksize check downstream.) CodeGen runs on a
    // thread pool, so we must not exit()/abort() from here; record the failure on the package
    // context and let the driver exit cleanly with a non-zero status after joining.
    auto& dataLayout = cgMod.GetLLVMModule()->getDataLayout();
    uint64_t elementByteSize = dataLayout.getTypeAllocSize(elemLLVMType);
    uint64_t totalByteSize = 0;
    constexpr uint64_t maxStackSize = 2147483648ULL; // 2GB, matches the LLVM backend guard
    bool overflowed = __builtin_mul_overflow(numOfElements, elementByteSize, &totalByteSize);
    if (overflowed && totalByteSize < maxStackSize) {
        cgCtx.GetCGPkgContext().RecordFatalError(
            "the total size of 'VArray<$" + std::to_string(numOfElements) + ", ...>' (" +
            std::to_string(numOfElements) + " * " + std::to_string(elementByteSize) +
            " bytes) wraps around and exceeds cangjie max stacksize(2GB).");
        // Emit a placeholder type so CodeGen keeps producing a self-consistent (but never linked)
        // module; the driver drops it after seeing the recorded fatal error.
        llvmType = llvm::ArrayType::get(elemLLVMType, 1U);
    } else {
        llvmType = llvm::ArrayType::get(elemLLVMType, numOfElements);
    }

    auto layoutName = "VArray." + std::to_string(varrType.GetSize()) + MangleType(*varrElementType);
    layoutType = llvm::StructType::getTypeByName(cgCtx.GetLLVMContext(), layoutName);
    if (layoutType && cgCtx.IsGeneratedStructType(layoutName)) {
        return llvmType;
    } else if (!layoutType) {
        layoutType = llvm::StructType::create(cgCtx.GetLLVMContext(), layoutName);
    }
    cgCtx.AddGeneratedStructType(layoutName);

    // VArrayLayout: [sizeOfVArray x T], T is element type. if Array is ref Array, T is i8 addrspece(1) *.
    SetStructTypeBody(layoutType, std::vector<llvm::Type*>{llvmType});

    return llvmType;
}

void CGVArrayType::GenContainedCGTypes()
{
    auto& varrayType = StaticCast<const CHIR::VArrayType&>(chirType);
    (void)containedCGTypes.emplace_back(CGType::GetOrCreate(cgMod, varrayType.GetElementType()));
}

llvm::Constant* CGVArrayType::GenFieldsNumOfTypeInfo()
{
    auto fieldsNum = StaticCast<const CHIR::VArrayType&>(chirType).GetSize();
    return llvm::ConstantInt::get(llvm::Type::getInt16Ty(cgMod.GetLLVMContext()), fieldsNum);
}

llvm::Constant* CGVArrayType::GenFieldsOfTypeInfo()
{
    return CGType::GenFieldsOfTypeInfo();
}

llvm::Constant* CGVArrayType::GenSourceGenericOfTypeInfo()
{
    return CGType::GenSourceGenericOfTypeInfo();
}

llvm::Constant* CGVArrayType::GenTypeArgsNumOfTypeInfo()
{
    return CGType::GenTypeArgsNumOfTypeInfo();
}

llvm::Constant* CGVArrayType::GenOffsetsOfTypeInfo()
{
    return CGType::GenOffsetsOfTypeInfo();
}

llvm::Constant* CGVArrayType::GenTypeArgsOfTypeInfo()
{
   return CGType::GenTypeArgsOfTypeInfo();
}

llvm::Constant* CGVArrayType::GenSuperOfTypeInfo()
{
    auto varrayElementType = DeRef(*StaticCast<const CHIR::VArrayType&>(chirType).GetElementType());
    auto elemCGType = CGType::GetOrCreate(cgMod, varrayElementType);
    auto ti = CGType::GetOrCreate(cgMod, varrayElementType)->GetOrCreateTypeInfo();
    if (elemCGType->IsStaticGI()) {
        cgCtx.AddDependentPartialOrderOfTypes(ti, this->typeInfo);
    }
    return llvm::ConstantExpr::getBitCast(ti, CGType::GetOrCreateTypeInfoPtrType(cgMod.GetLLVMContext()));
}
} // namespace Cangjie::CodeGen
