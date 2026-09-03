// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

/**
 * @file
 *
 * This file defines the version of the serialized CJO format.
 *
 * The CJO version protects the serialized contract between a CJO producer and a
 * CJO reader. The contract includes the serialized layout and any serialized
 * meaning that a reader must understand in order to load declarations safely.
 * Keep the version change in the same change as the format change. A format
 * change without the corresponding version change can make an incompatible
 * CJO appear compatible to an older compiler.
 *
 * Version components are bumped according to the following policy:
 *
 * | Component | Bump when | Reader compatibility |
 * |-----------|-----------|----------------------|
 * | Major | Reserved for a serialized change that is incompatible in both directions. The current LTS line does not allow major-version changes. | The major component must match exactly. |
 * | Minor | A reviewed serialized change is backward-compatible for newer readers but is not forward-compatible for older readers. | An older minor from the same major is accepted; a newer minor is rejected. |
 * | Patch | Any change is made to the CJO schema or to the compiler algorithm that serializes CJO files, when the change does not require a major or minor bump. | The patch component is not checked by the current compatibility gate. |
 *
 * During the current LTS line, CJO files are required to remain backward
 * compatible: newer compilers must be able to process older CJO files. A
 * non-forward-compatible change may be introduced only after review and
 * registration, and it requires a minor bump. Every change to the CJO schema
 * or to the CJO serialization algorithm requires at least a patch bump.
 *
 * The following table is the detailed bump matrix for changes to the CJO
 * schema. A check mark means that the indicated component is incremented, a
 * zero means that the component is reset to zero, and a dash means that the
 * component is not changed. When several rows apply to one change, use the
 * version update required by the most restrictive row.
 *
 * | Data type | Change | Major | Minor | Patch |
 * |-----------|--------|-------|-------|-------|
 * | table | Insert, delete, reorder, or change the type of a field | bump | reset | reset |
 * | table | Change a default so that a new reader cannot construct the old value correctly | bump | reset | reset |
 * | table | Change a default when a new reader can branch on the CJO version, and old readers would change compilation behavior on the new default | - | bump | reset |
 * | table | Change field semantics so that a new reader cannot construct the old value correctly | bump | reset | reset |
 * | table | Change field semantics when a new reader can branch on the CJO version, and old readers cannot process the new semantics | - | bump | reset |
 * | table | Append a field when a new reader cannot construct a valid object without reading it | bump | reset | reset |
 * | table | Append a field with forward and backward compatibility | - | - | bump |
 * | table | Deprecate a field, stop writing it, and use versioning to read the new or old field when old readers cannot handle its absence | - | bump | reset |
 * | table | Deprecate a field while continuing to write it | - | - | bump |
 * | table | Add a table type | - | - | bump |
 * | table | Rename a field only | - | - | bump |
 * | struct | Insert, delete, reorder, or change the type of a field | bump | reset | reset |
 * | struct | Change field semantics so that a new reader cannot construct the old value correctly | bump | reset | reset |
 * | struct | Change field semantics when a new reader can branch on the CJO version, and old readers cannot process the new semantics | - | bump | reset |
 * | struct | Add a struct type | - | - | bump |
 * | struct | Rename a field only | - | - | bump |
 * | enum | Insert, delete, reorder, or change an enum value when existing CJO files have no explicit value | bump | reset | reset |
 * | enum | Change enum-value semantics so that a new reader cannot construct the old semantics correctly | bump | reset | reset |
 * | enum | Change enum-value semantics when a new reader can branch on the CJO version, and old readers cannot process the new semantics | - | bump | reset |
 * | enum | Append a member when a new reader cannot handle its absence | bump | reset | reset |
 * | enum | Append a member when a new reader can branch around its absence, but old readers cannot handle the new value; use the compatibility allowlist | - | bump | reset |
 * | enum | Append a member when old readers handle the new value and new readers handle its absence; use the compatibility allowlist | - | - | bump |
 * | enum | Add an enum type | - | - | bump |
 * | enum | Rename a field only | - | - | bump |
 * | union | Insert, delete, or reorder a member when existing CJO files have no discriminant | bump | reset | reset |
 * | union | Change field semantics so that a new reader cannot construct the old semantics correctly | bump | reset | reset |
 * | union | Change field semantics when a new reader can branch on the CJO version, and old readers cannot process the new semantics | - | bump | reset |
 * | union | Append a member when a new reader cannot handle its absence | bump | reset | reset |
 * | union | Append a member when a new reader can branch around its absence, but old readers cannot handle the new value | - | bump | reset |
 * | union | Append a member when old readers handle the new value and new readers handle its absence; all current CJO union fields must be compatible | - | - | bump |
 * | union | Add a union type | - | - | bump |
 * | union | Rename a field only | - | - | bump |
 * | vector/string/scalar | Change element type or scalar field width | bump | reset | reset |
 * | vector/string/scalar | Change a scalar default so that a new reader cannot construct the old value correctly | bump | reset | reset |
 * | vector/string/scalar | Change a scalar default when a new reader can branch on the CJO version, and old readers cannot process the new semantics | - | bump | reset |
 * | vector/string/scalar | Change field semantics so that a new reader cannot construct the old semantics correctly | bump | reset | reset |
 * | vector/string/scalar | Change field semantics when a new reader can branch on the CJO version, and old readers cannot process the new semantics | - | bump | reset |
 * | other | Change the file identifier | bump | reset | reset |
 * | other | Change the root type | bump | reset | reset |
 * | other | Rename the namespace only | - | - | bump |
 *
 * Enum additions require special care. Appending a member is patch-compatible
 * only for enums whose readers are explicitly known to tolerate unknown
 * values. The current patch-compatible allowlist is TypeKind, DeclKind,
 * ExprKind, and PackageKind. Appended members of AnnoKind, OptimizationLevel,
 * PatternKind, AccessLevel, AccessModifier, OverflowPolicy, OperatorKind,
 * CallKind, LitConstKind, StringKind, ForInKind, and BuiltInType require a
 * minor bump because an older reader may fail when it encounters the new
 * value.
 *
 * Major and minor bumps must be reviewed during development. They must not be
 * introduced as ordinary fixes during the LTS line or after release
 * preparation. A major bump resets minor and patch to zero. A minor bump
 * resets patch to zero. Record the compatibility impact and any required
 * recompilation in the release notes.
 */
#ifndef CANGJIE_MODULES_CJO_VERSION_H
#define CANGJIE_MODULES_CJO_VERSION_H

#include <cstdint>

namespace Cangjie {
constexpr uint8_t CJO_MAJOR_VERSION = 0;
constexpr uint8_t CJO_MINOR_VERSION = 1;
constexpr uint8_t CJO_PATCH_VERSION = 0;
} // namespace Cangjie

#endif // CANGJIE_MODULES_CJO_VERSION_H
