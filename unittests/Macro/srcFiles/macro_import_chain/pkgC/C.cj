// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

// pkgC is a normal (non-macro) package: the downstream package. It imports pkgB
// only (NOT pkgA). After rule 3.3 + transitive closure, pkgC must obtain:
//   - pkgB's symbols (normal import pkgB.*)
//   - pkgA's macro GenMacroA (pkgB declares `macro import pkgA`; pkgB does NOT
//     call GenMacroA itself, its expansion *emits* @GenMacroA. Via the macro-
//     import declaration chain, pkgA is lifted onto pkgB -> pkgC)
//   - ArrayList (pkgA declares `macro import std.collection.ArrayList`; lifted
//     onto pkgA -> pkgB -> pkgC via the same declaration chain)
// So @GenMacroB -> @GenMacroA -> ArrayList(...) all resolve in pkgC, even though
// pkgC only imports pkgB and never names pkgA or std.collection.

package pkgC

import std.ast.*
import pkgB.*

@GenMacroB
class Marker {
}

main(): Int64 {
    return 0
}
