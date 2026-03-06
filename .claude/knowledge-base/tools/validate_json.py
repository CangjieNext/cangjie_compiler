#!/usr/bin/env python3
"""Validate all JSON files in the Cangjie compiler knowledge base.

Checks that every JSON file parses correctly and contains the required
fields for its file type (root index, category index, module metadata,
component list, concept metadata, workflow metadata, cross-references,
and search index).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Required-field definitions per file type
# ---------------------------------------------------------------------------

ROOT_INDEX_FIELDS = ["version", "lastUpdated", "categories"]

CROSS_REFERENCES_FIELDS = ["version", "references", "dependencyGraph"]

SEARCH_INDEX_FIELDS = ["version", "index"]
SEARCH_INDEX_SUB_FIELDS = {"index": ["keywords", "sourceFiles", "concepts"]}

# Category index.json files (architecture/, modules/, concepts/, etc.)
CATEGORY_INDEX_FIELDS = ["category"]
# They must also have at least one of these list fields:
CATEGORY_INDEX_LIST_FIELDS = ["documents", "subcategories", "modules"]

# Module <name>.json (not *-components.json, not index.json)
MODULE_JSON_FIELDS_ALT = [
    # Either flat top-level keys …
    {"moduleId", "name", "description", "sourceDirectories"},
    # … or nested under "module" with documentId at top level
    {"documentId", "module"},
    # … or documentId + category (alternative agent format)
    {"documentId", "category"},
]
MODULE_NESTED_FIELDS = ["name"]

# Module *-components.json
COMPONENT_JSON_FIELDS_ALT = [
    {"moduleId", "components"},
    {"moduleId", "classes"},
]

# Concept <name>.json
CONCEPT_JSON_FIELDS_ALT = [
    {"documentId", "concept"},
    {"concept"},
]
CONCEPT_NESTED_FIELDS = ["name", "definition"]

# Workflow <name>.json
WORKFLOW_JSON_FIELDS_ALT = [
    {"documentId", "workflow"},
    {"workflowId", "workflow"},
    {"workflow"},
]
WORKFLOW_NESTED_FIELDS = ["name"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _classify(path: Path, base_dir: Path) -> str:
    """Return a type tag for the given JSON file."""
    rel = path.relative_to(base_dir)
    parts = rel.parts
    name = path.name

    if parts == ("index.json",):
        return "root-index"
    if name == "cross-references.json" and len(parts) == 1:
        return "cross-references"
    if name == "search-index.json" and len(parts) == 1:
        return "search-index"
    if name == "index.json":
        return "category-index"
    if len(parts) >= 2 and parts[0] == "modules" and name.endswith("-components.json"):
        return "module-components"
    if len(parts) >= 2 and parts[0] == "modules":
        return "module"
    if len(parts) >= 2 and parts[0] == "concepts":
        return "concept"
    if len(parts) >= 2 and parts[0] == "workflows":
        return "workflow"
    if len(parts) >= 2 and parts[0] == "architecture":
        return "architecture"
    if len(parts) >= 2 and parts[0] == "code-navigation":
        return "code-navigation"
    return "unknown"


def _check_fields(data: dict, required: list[str]) -> list[str]:
    """Return list of missing field names."""
    return [f for f in required if f not in data]


def _check_alternatives(data: dict, alternatives: list[set[str]]) -> list[str] | None:
    """Check that at least one alternative set of keys is fully present.

    Returns None on success, or a description string on failure.
    """
    for alt in alternatives:
        if alt.issubset(data.keys()):
            return None
    options = " OR ".join(str(sorted(a)) for a in alternatives)
    return f"must contain one of: {options}"


# ---------------------------------------------------------------------------
# Per-type validators
# ---------------------------------------------------------------------------

def _validate_root_index(data: dict, path: Path) -> list[str]:
    missing = _check_fields(data, ROOT_INDEX_FIELDS)
    return [f"missing required field '{f}'" for f in missing]


def _validate_cross_references(data: dict, path: Path) -> list[str]:
    missing = _check_fields(data, CROSS_REFERENCES_FIELDS)
    return [f"missing required field '{f}'" for f in missing]


def _validate_search_index(data: dict, path: Path) -> list[str]:
    errors: list[str] = []
    missing = _check_fields(data, SEARCH_INDEX_FIELDS)
    errors.extend(f"missing required field '{f}'" for f in missing)
    if "index" in data and isinstance(data["index"], dict):
        sub_missing = _check_fields(data["index"], SEARCH_INDEX_SUB_FIELDS["index"])
        errors.extend(f"missing 'index.{f}'" for f in sub_missing)
    return errors


def _validate_category_index(data: dict, path: Path) -> list[str]:
    errors: list[str] = []
    missing = _check_fields(data, CATEGORY_INDEX_FIELDS)
    errors.extend(f"missing required field '{f}'" for f in missing)
    if not any(f in data for f in CATEGORY_INDEX_LIST_FIELDS):
        errors.append(
            f"must contain at least one of: {CATEGORY_INDEX_LIST_FIELDS}"
        )
    return errors


def _validate_module_components(data: dict, path: Path) -> list[str]:
    result = _check_alternatives(data, COMPONENT_JSON_FIELDS_ALT)
    if result:
        return [result]
    return []


def _validate_module(data: dict, path: Path) -> list[str]:
    errors: list[str] = []
    result = _check_alternatives(data, MODULE_JSON_FIELDS_ALT)
    if result:
        errors.append(result)
    if "module" in data and isinstance(data["module"], dict):
        missing = _check_fields(data["module"], MODULE_NESTED_FIELDS)
        errors.extend(f"missing 'module.{f}'" for f in missing)
    return errors


def _validate_concept(data: dict, path: Path) -> list[str]:
    errors: list[str] = []
    result = _check_alternatives(data, CONCEPT_JSON_FIELDS_ALT)
    if result:
        errors.append(result)
    if "concept" in data and isinstance(data["concept"], dict):
        missing = _check_fields(data["concept"], CONCEPT_NESTED_FIELDS)
        errors.extend(f"missing 'concept.{f}'" for f in missing)
    return errors


def _validate_workflow(data: dict, path: Path) -> list[str]:
    errors: list[str] = []
    result = _check_alternatives(data, WORKFLOW_JSON_FIELDS_ALT)
    if result:
        errors.append(result)
    if "workflow" in data and isinstance(data["workflow"], dict):
        missing = _check_fields(data["workflow"], WORKFLOW_NESTED_FIELDS)
        errors.extend(f"missing 'workflow.{f}'" for f in missing)
    return errors


VALIDATORS = {
    "root-index": _validate_root_index,
    "cross-references": _validate_cross_references,
    "search-index": _validate_search_index,
    "category-index": _validate_category_index,
    "module-components": _validate_module_components,
    "module": _validate_module,
    "concept": _validate_concept,
    "workflow": _validate_workflow,
}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate JSON files in the knowledge base."
    )
    parser.add_argument(
        "--base-dir",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Root of the knowledge base (default: parent of tools/)",
    )
    args = parser.parse_args()
    base_dir: Path = args.base_dir.resolve()

    if not base_dir.is_dir():
        print(f"FAIL: base directory does not exist: {base_dir}")
        return 1

    json_files = sorted(base_dir.rglob("*.json"))
    # Exclude files inside tools/ itself
    tools_dir = base_dir / "tools"
    json_files = [f for f in json_files if not str(f).startswith(str(tools_dir))]

    total = len(json_files)
    errors: list[tuple[Path, str]] = []

    for jf in json_files:
        # 1. Parse check
        try:
            with open(jf, encoding="utf-8") as fh:
                data = json.load(fh)
        except json.JSONDecodeError as exc:
            errors.append((jf, f"invalid JSON: {exc}"))
            continue

        if not isinstance(data, dict):
            errors.append((jf, "top-level value is not a JSON object"))
            continue

        # 2. Structural check
        ftype = _classify(jf, base_dir)
        validator = VALIDATORS.get(ftype)
        if validator:
            for msg in validator(data, jf):
                errors.append((jf, msg))

    # Summary
    print(f"Checked {total} JSON file(s) under {base_dir}\n")
    if errors:
        print(f"FAIL: {len(errors)} issue(s) found:\n")
        for path, msg in errors:
            print(f"  {path.relative_to(base_dir)}  ->  {msg}")
        print()
        return 1

    print("PASS: all JSON files are valid.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
