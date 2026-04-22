// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_ANNOTATIONKIND_H
#define CANGJIE_AST_NODES_ANNOTATIONKIND_H

namespace Cangjie::AST {
enum class AnnotationKind {
    JAVA,
    CALLING_CONV,
    C,
    JAVA_MIRROR,
    JAVA_IMPL,
    JAVA_HAS_DEFAULT,
    OBJ_C_MIRROR,
    OBJ_C_IMPL,
    OBJ_C_INIT, // Indicate a method as Objective-C init.
    OBJ_C_OPTIONAL,
    FOREIGN_NAME,
    FOREIGN_GETTER_NAME,
    FOREIGN_SETTER_NAME,
    ATTRIBUTE,
    /**
     * NOTE: 'OVERFLOW' is a macro in early versions of glibc (VERSION < 2.27).
     * To be compatiable with old versions of glibc we should avoid using 'OVERFLOW' as an identifier.
     * For details, please check 'https://man7.org/linux/man-pages/man3/matherr.3.html'.
     */
    NUMERIC_OVERFLOW,
    INTRINSIC,
    WHEN,
    FASTNATIVE,
    ANNOTATION, // Indicate @Annotation.
    CUSTOM,     // Indicate a Custom Annotation, such as @JsonName.
    CONSTSAFE,
    DEPRECATED,
    FROZEN,
    ENSURE_PREPARED_TO_MOCK,
    NON_PRODUCT,
    MAKE_COPY,
    UNKNOWN
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_ANNOTATIONKIND_H