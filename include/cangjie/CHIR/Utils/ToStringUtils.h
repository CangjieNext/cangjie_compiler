// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_TOSTRING_UTILS_H
#define CANGJIE_CHIR_TOSTRING_UTILS_H

#include "cangjie/CHIR/IR/IntrinsicKind.h"
#include "cangjie/CHIR/IR/Package.h"
#include "cangjie/CHIR/Utils/StringWrapper.h"
#include "cangjie/CHIR/IR/Type/Type.h"
#include "cangjie/CHIR/IR/Value/Value.h"
#include "cangjie/Utils/SafePointer.h"

#include <functional>
#include <iostream>
#include <set>
#include <vector>

namespace Cangjie {
enum class OverflowStrategy : uint8_t;
}

namespace Cangjie::CHIR {
/**
 * Map of IntrinsicKind to string value.
 */
extern const std::unordered_map<CHIR::IntrinsicKind, std::string> INTRINSIC_KIND_TO_STRING_MAP;

const std::unordered_map<CHIR::Package::AccessLevel, std::string> PACKAGE_ACCESS_LEVEL_TO_STRING_MAP = {
    {CHIR::Package::AccessLevel::INTERNAL, "internal"},
    {CHIR::Package::AccessLevel::PROTECTED, "protected"},
    {CHIR::Package::AccessLevel::PUBLIC, "public"},
};

/**
 * @brief Prints indentation to the specified stream.
 *
 * @param stream The output stream to print to.
 * @param indent The number of indentation levels.
 */
void PrintIndent(std::ostream& stream, size_t indent = 1);

/**
 * @brief Generates a string representation of generic constraints.
 *
 * @param genericTypeParams The list of generic type parameters.
 * @return A string representing the generic constraints.
 */
std::string GetGenericConstaintsStr(const std::vector<GenericType*>& genericTypeParams);

/**
 * @brief Generates a string representation of a block group.
 *
 * @param blockGroup The block group to represent.
 * @param indent The number of indentation levels.
 * @return A string representing the block group.
 */
std::string GetBlockGroupStr(const BlockGroup& blockGroup, size_t indent = 0);

/**
 * @brief Generates a string representation of a block.
 *
 * @param block The block to represent.
 * @param indent The number of indentation levels.
 * @return A string representing the block.
 */
std::string GetBlockStr(const Block& block, size_t indent = 0);

/**
 * @brief Generates a string representation of a function.
 *
 * @param func The function to represent.
 * @param indent The number of indentation levels.
 * @return A string representing the function.
 */
std::string GetFuncStr(const Function& func, size_t indent = 0);

std::string FuncSymbolStr(const Function& func);

/**
 * @brief Generates a string representation of a lambda expression.
 *
 * @param lambda The lambda expression to represent.
 * @param indent The number of indentation levels.
 * @return A string representing the lambda expression.
 */
std::string GetLambdaStr(const Lambda& lambda, size_t indent = 0);

/**
 * @brief Generates a string representation of an imported var.
 *
 * @param value The imported var to represent.
 * @return A string representing the imported value.
 */
std::string GetImportedVarStr(const GlobalVar& value);

/**
 * @brief Generates a string representation of an imported function.
 *
 * @param value The imported function to represent.
 * @return A string representing the imported function.
 */
std::string GetImportedFuncStr(const Function& value);

/**
 * @brief Generates a string representation of exceptions.
 *
 * @param exceptions The list of exception classes.
 * @return A string representing the exceptions.
 */
std::string GetExceptionsStr(const std::vector<ClassType*>& exceptions);

/**
 * @brief Generates a string representation of generic type parameters.
 *
 * @param genericTypeParams The list of generic type parameters.
 * @return A string representing the generic type parameters.
 */
std::string GetGenericTypeParamsStr(const std::vector<GenericType*>& genericTypeParams);

/**
 * @brief Generates a string representation of generic type parameter constraints.
 *
 * @param genericTypeParams The list of generic type parameters.
 * @return A string representing the generic type parameter constraints.
 */
std::string GetGenericTypeParamsConstraintsStr(const std::vector<GenericType*>& genericTypeParams);

/**
 * @brief Adds a comma or not to the string stream based on a condition.
 *
 * @param ss The string stream to modify.
 */
void AddCommaOrNot(std::stringstream& ss);

/**
 * @brief Converts a package access level to a string.
 *
 * @param level The package access level to convert.
 * @return A string representing the package access level.
 */
std::string PackageAccessLevelToString(const Package::AccessLevel& level);

/**
 * @brief Converts a custom type kind to a string.
 *
 * @param def The custom type definition to convert.
 * @return A string representing the custom type kind.
 */
std::string CustomTypeKindToString(const CustomTypeDef& def);

/**
 * @brief Converts a boolean value to a string.
 *
 * @param flag The boolean value to convert.
 * @return A string representing the boolean value.
 */
std::string BoolToString(bool flag);

StringWrapper ThisTypeToString(const Type* thisType);
std::string InstTypeArgsToString(const std::vector<Type*>& instTypeArgs);
std::string ExprOperandsToString(const std::vector<Value*>& args);
std::string ExprWithExceptionOperandsToString(const std::vector<Value*>& args, const std::vector<Block*>& successors);
std::string OverflowToString(Cangjie::OverflowStrategy ofStrategy);
} // namespace Cangjie::CHIR

#endif // CANGJIE_CHIR_TOSTRING_UTILS_H
