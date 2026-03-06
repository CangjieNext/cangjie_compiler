#!/usr/bin/env python3
"""Validate the document structure of the Cangjie compiler knowledge base.

Checks performed:
  1. Markdown files contain required sections based on their category:
     - modules/   : "## 概述" or "## Overview", "## 关键组件" or "## Key Components"
     - concepts/  : "## 定义" or "## Definition"
     - workflows/ : "## 步骤" or "## Steps" or "## 前置条件" or "## Prerequisites"
     - architecture/ : at least 2 level-2 headings (##)
  2. Every module .json has a matching .md and vice versa (excluding
     index files and *-components.json files).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Heading extraction
# ---------------------------------------------------------------------------

_H2_RE = re.compile(r"^##\s+(.+)$", re.MULTILINE)


def _h2_headings(text: str) -> list[str]:
    """Return all level-2 heading texts found in *text*."""
    return _H2_RE.findall(text)


# ---------------------------------------------------------------------------
# Section requirement definitions
# ---------------------------------------------------------------------------

# Each entry is a list of *alternative* headings — at least one must appear.
MODULE_REQUIRED = [
    ["概述", "Overview", "Purpose"],
    ["关键组件", "Key Components"],
]

CONCEPT_REQUIRED = [
    ["定义", "Definition"],
]

WORKFLOW_REQUIRED = [
    ["步骤", "Steps", "前置条件", "Prerequisites"],
]

ARCH_MIN_H2 = 2  # architecture docs need at least this many ## headings


# ---------------------------------------------------------------------------
# Per-category checkers
# ---------------------------------------------------------------------------

def _check_required_sections(
    headings: list[str],
    requirements: list[list[str]],
) -> list[str]:
    """Return descriptions of missing required sections."""
    issues: list[str] = []
    for alternatives in requirements:
        if not any(alt in headings for alt in alternatives):
            label = " / ".join(f'"## {a}"' for a in alternatives)
            issues.append(f"missing required section (one of {label})")
    return issues


def _check_md(path: Path, category: str) -> list[str]:
    """Validate a single markdown file. Returns list of issue strings."""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return [f"cannot read file: {exc}"]

    headings = _h2_headings(text)

    if category == "modules":
        return _check_required_sections(headings, MODULE_REQUIRED)
    if category == "concepts":
        return _check_required_sections(headings, CONCEPT_REQUIRED)
    if category == "workflows":
        return _check_required_sections(headings, WORKFLOW_REQUIRED)
    if category == "architecture":
        if len(headings) < ARCH_MIN_H2:
            return [
                f"expected at least {ARCH_MIN_H2} level-2 headings, "
                f"found {len(headings)}"
            ]
    return []


# ---------------------------------------------------------------------------
# JSON / MD pairing check
# ---------------------------------------------------------------------------

def _check_pairing(category_dir: Path) -> list[str]:
    """Ensure every non-index .json has a matching .md and vice versa.

    Skips index.json and *-components.json files.
    """
    issues: list[str] = []
    md_stems = {
        f.stem for f in category_dir.glob("*.md") if f.name != "index.md"
    }
    json_stems = {
        f.stem
        for f in category_dir.glob("*.json")
        if f.name != "index.json" and not f.name.endswith("-components.json")
    }

    for stem in json_stems - md_stems:
        issues.append(f"{category_dir.name}/{stem}.json has no matching .md")
    for stem in md_stems - json_stems:
        issues.append(f"{category_dir.name}/{stem}.md has no matching .json")
    return issues


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

CATEGORIES_WITH_MD_CHECKS = ["modules", "concepts", "workflows", "architecture",
                              "code-navigation"]
CATEGORIES_WITH_PAIRING = ["modules", "concepts", "workflows", "architecture",
                            "code-navigation"]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate document structure in the knowledge base."
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

    errors: list[tuple[str, str]] = []  # (location, message)
    md_count = 0

    # 1. Check markdown section requirements
    for category in CATEGORIES_WITH_MD_CHECKS:
        cat_dir = base_dir / category
        if not cat_dir.is_dir():
            continue
        for md_file in sorted(cat_dir.glob("*.md")):
            if md_file.name == "index.md":
                continue
            md_count += 1
            issues = _check_md(md_file, category)
            for msg in issues:
                rel = md_file.relative_to(base_dir)
                errors.append((str(rel), msg))

    # 2. Check JSON/MD pairing
    for category in CATEGORIES_WITH_PAIRING:
        cat_dir = base_dir / category
        if not cat_dir.is_dir():
            continue
        for msg in _check_pairing(cat_dir):
            errors.append((category, msg))

    # Summary
    print(f"Checked {md_count} markdown file(s) under {base_dir}\n")
    if errors:
        print(f"FAIL: {len(errors)} issue(s) found:\n")
        for location, msg in errors:
            print(f"  {location}  ->  {msg}")
        print()
        return 1

    print("PASS: all documents have valid structure.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
