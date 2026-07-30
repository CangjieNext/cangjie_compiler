// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_CHIR_MATCHTRAVERSALINFO_H
#define CANGJIE_CHIR_MATCHTRAVERSALINFO_H

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace Cangjie::CHIR {

class Block;
class GoTo;

class MatchTraversalInfo {
public:
    void MarkPatternBlock(Block& block)
    {
        patternBlocks.emplace(&block);
    }

    void MarkPatternGoTo(GoTo& goTo)
    {
        patternGoTos.emplace(&goTo);
    }

    void SetCaseId(Block& block, size_t caseId)
    {
        caseIds.insert_or_assign(&block, caseId);
    }

    bool IsPatternBlock(const Block& block) const
    {
        return patternBlocks.find(&block) != patternBlocks.end();
    }

    bool IsPatternGoTo(const GoTo& goTo) const
    {
        return patternGoTos.find(&goTo) != patternGoTos.end();
    }

    std::optional<size_t> GetCaseId(const Block& block) const
    {
        auto found = caseIds.find(&block);
        return found == caseIds.end() ? std::nullopt : std::optional<size_t>{found->second};
    }

    void Merge(MatchTraversalInfo&& other)
    {
        patternBlocks.merge(other.patternBlocks);
        patternGoTos.merge(other.patternGoTos);
        caseIds.merge(other.caseIds);
    }

    void Clear()
    {
        patternBlocks.clear();
        patternGoTos.clear();
        caseIds.clear();
    }

private:
    std::unordered_set<const Block*> patternBlocks;
    std::unordered_set<const GoTo*> patternGoTos;
    std::unordered_map<const Block*, size_t> caseIds;
};

} // namespace Cangjie::CHIR

#endif
