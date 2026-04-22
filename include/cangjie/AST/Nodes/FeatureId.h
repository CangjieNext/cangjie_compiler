// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_AST_NODES_FEATUREID_H
#define CANGJIE_AST_NODES_FEATUREID_H

#include "cangjie/AST/Nodes/Node.h"
#include "cangjie/AST/Identifier.h"

namespace Cangjie::AST {
struct Annotation;
/**
 * featureId
 *     : (Ident | ContextIdent) (DOT (Ident | ContextIdent))*
 *     ;
 */
struct FeatureId : Node {
    std::vector<Identifier> identifiers;
    std::vector<Position> dotPoses;
    FeatureId();
    std::string ToString() const override;
};

/**
 * FeaturesSet
 *     : LCURL NL*
 *     FeatureId NL* (COMMA NL* FeatureId NL*)*
 *     RCURL
 *     ;
 */
struct FeaturesSet : Node {
    Position lCurlPos;
    std::vector<FeatureId> content;
    std::vector<Position> commaPoses;
    Position rCurlPos;
    FeaturesSet();
};

/**
 * featuresDirective
 *     : annotationList? FEATURES NL*
 *     FeaturesSet
 *     end+
 *     ;
 */
struct FeaturesDirective : Node {
    std::vector<OwnedPtr<Annotation>> annotations;
    OwnedPtr<FeaturesSet> featuresSet;
    Position featuresPos;
    FeaturesDirective();
    ~FeaturesDirective();
};
} // namespace Cangjie::AST

#endif // CANGJIE_AST_NODES_FEATUREID_H
