#!/usr/bin/env python3
"""Check that all relative-path references inside knowledge-base JSON files
resolve to existing files or directories.

Inspects the following fields (at any nesting depth):
  path, metadataFile, documentPath, documentation, metadata, indexFile

Paths that are already absolute are resolved as-is; relative paths are
resolved relative to the directory containing the JSON file.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Fields whose values are treated as file-system path references.
PATH_FIELDS = {"path", "metadataFile", "documentPath", "documentation",
               "metadata", "indexFile"}


# ---------------------------------------------------------------------------
# Recursive extraction of path references
# ---------------------------------------------------------------------------

def _extract_paths(obj, field_trail: str = "") -> list[tuple[str, str]]:
    """Walk *obj* and yield (field_name, value) for every PATH_FIELDS hit."""
    results: list[tuple[str, str]] = []
    if isinstance(obj, dict):
        for key, value in obj.items():
            current = f"{field_trail}.{key}" if field_trail else key
            if key in PATH_FIELDS and isinstance(value, str):
                results.append((current, value))
            else:
                results.extend(_extract_paths(value, current))
    elif isinstance(obj, list):
        for idx, item in enumerate(obj):
            results.extend(_extract_paths(item, f"{field_trail}[{idx}]"))
    return results


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check path references in knowledge-base JSON files."
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
    tools_dir = base_dir / "tools"
    json_files = [f for f in json_files if not str(f).startswith(str(tools_dir))]

    total_links = 0
    broken: list[tuple[Path, str, str, Path]] = []  # (source, field, raw, resolved)

    for jf in json_files:
        try:
            with open(jf, encoding="utf-8") as fh:
                data = json.load(fh)
        except (json.JSONDecodeError, OSError):
            # Parsing errors are reported by validate_json.py; skip here.
            continue

        refs = _extract_paths(data)
        for field_name, raw_path in refs:
            total_links += 1
            target = Path(raw_path)
            if not target.is_absolute():
                target = (jf.parent / target).resolve()
            else:
                target = target.resolve()

            if not target.exists():
                broken.append((jf, field_name, raw_path, target))

    # Summary
    print(f"Checked {total_links} path reference(s) across "
          f"{len(json_files)} JSON file(s) under {base_dir}\n")

    if broken:
        print(f"FAIL: {len(broken)} broken link(s) found:\n")
        for source, field, raw, resolved in broken:
            rel_source = source.relative_to(base_dir)
            print(f"  {rel_source}")
            print(f"    field   : {field}")
            print(f"    target  : {raw}")
            print(f"    resolved: {resolved}")
            print()
        return 1

    print("PASS: all path references are valid.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
