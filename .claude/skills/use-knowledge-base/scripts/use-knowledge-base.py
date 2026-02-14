#!/usr/bin/env python3
"""Navigate and query the Cangjie compiler knowledge base.

Reads structured JSON metadata from the 6-layer index system for quick
lookups of modules, concepts, architecture, workflows, and code navigation.

Usage:
    python3 use-knowledge-base.py [action] [options]

Actions:
    overview              Show knowledge base overview from root index
    modules               List all modules grouped by subcategory
    module <id>           Show module metadata and components
    search <query>        Search keywords in search-index.json (supports CN/EN)
    xref <module_id>      Show cross-references for a module
    source <file_path>    Look up which module/component owns a source file
    concepts              List all concepts
    workflows             List all workflows

Examples:
    python3 use-knowledge-base.py overview
    python3 use-knowledge-base.py modules
    python3 use-knowledge-base.py module lex
    python3 use-knowledge-base.py search "语义分析"
    python3 use-knowledge-base.py search parser
    python3 use-knowledge-base.py xref sema
    python3 use-knowledge-base.py source src/Parse/Parser.cpp
    python3 use-knowledge-base.py concepts
    python3 use-knowledge-base.py workflows
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

KB_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent / ".claude" / "knowledge-base"


# ---------------------------------------------------------------------------
# JSON helpers
# ---------------------------------------------------------------------------

def _load_json(rel_path: str) -> dict:
    """Load a JSON file relative to KB_ROOT."""
    path = KB_ROOT / rel_path
    if not path.exists():
        print(f"File not found: {path.relative_to(KB_ROOT.parent.parent)}", file=sys.stderr)
        sys.exit(1)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------

def cmd_overview() -> int:
    """Show knowledge base overview from root index.json."""
    data = _load_json("index.json")
    print(f"Cangjie Compiler Knowledge Base  v{data['version']}  ({data['lastUpdated']})")
    print("=" * 60)
    for cat in data["categories"]:
        print(f"  [{cat['id']:20s}] {cat['description']}  ({cat['documentCount']} docs)")
    print()
    hints = data.get("searchHints", {})
    if hints:
        print("Search hints:")
        for key, val in hints.items():
            print(f"  {val}")
    return 0


def cmd_modules() -> int:
    """List all modules grouped by subcategory."""
    data = _load_json("modules/index.json")
    print("Compiler Modules")
    print("=" * 60)
    for sub in data.get("subcategories", []):
        print(f"\n  [{sub['subcategory']}]")
        for mod in sub["modules"]:
            deps = ", ".join(mod.get("dependencies", [])) or "(none)"
            src = ", ".join(mod.get("sourceLocations", []))
            print(f"    {mod['id']:12s}  {mod['description']}")
            print(f"    {'':12s}  deps: {deps}  |  src: {src}")
    return 0


def cmd_module(module_id: str) -> int:
    """Show detailed metadata and components for a module."""
    # Module metadata
    meta_path = f"modules/{module_id}.json"
    meta = _load_json(meta_path)
    print(f"Module: {module_id}")
    print("=" * 60)

    # Handle both flat and nested formats
    mod_info = meta.get("module", meta)
    for key in ("name", "description", "purpose"):
        if key in mod_info:
            print(f"  {key}: {mod_info[key]}")

    src_dirs = mod_info.get("sourceDirectories", meta.get("sourceDirectories", []))
    if src_dirs:
        print(f"  source dirs: {', '.join(src_dirs)}")

    # Components
    comp_path = KB_ROOT / "modules" / f"{module_id}-components.json"
    if comp_path.exists():
        comp_data = json.loads(comp_path.read_text(encoding="utf-8"))
        components = comp_data.get("components", {})

        # Handle nested format: components.classes, components.enums, etc.
        if isinstance(components, dict):
            for section_name, items in components.items():
                if isinstance(items, list) and items:
                    print(f"\n  {section_name} ({len(items)}):")
                    for c in items:
                        if isinstance(c, dict):
                            name = c.get("name", c.get("className", "?"))
                            desc = c.get("description", c.get("purpose", ""))
                            loc = c.get("location", "")
                            print(f"    - {name}: {desc}")
                            if loc:
                                print(f"      location: {loc}")
        # Handle flat list format
        elif isinstance(components, list):
            print(f"\n  Components ({len(components)}):")
            for c in components:
                if isinstance(c, dict):
                    name = c.get("name", "?")
                    desc = c.get("description", "")
                    print(f"    - {name}: {desc}")
    return 0


def cmd_search(query: str) -> int:
    """Search keywords in search-index.json (supports CN/EN)."""
    data = _load_json("search-index.json")
    keywords = data.get("index", {}).get("keywords", {})
    aliases = data.get("aliases", {})

    query_lower = query.lower()
    results: list[tuple[str, list]] = []

    # Search keywords
    for kw, info in keywords.items():
        if query_lower in kw.lower():
            docs = info.get("documents", [])
            results.append((kw, docs))

    # Check aliases
    resolved_alias = None
    for alias, target in aliases.items():
        if query_lower in alias.lower():
            resolved_alias = (alias, target)
            break

    if not results and not resolved_alias:
        print(f"No results for '{query}'")
        return 1

    if resolved_alias:
        print(f"Alias: '{resolved_alias[0]}' -> '{resolved_alias[1]}'")
        print()

    seen = set()
    if results:
        print(f"Search results for '{query}':")
        print("-" * 50)
        for kw, docs in results:
            print(f"\n  keyword: {kw}")
            for doc in sorted(docs, key=lambda d: -d.get("relevance", 0)):
                key = (doc["type"], doc["id"])
                if key in seen:
                    continue
                seen.add(key)
                rel = doc.get("relevance", "?")
                print(f"    [{doc['type']:12s}] {doc['id']:20s}  relevance={rel}  path={doc['path']}")
    return 0


def cmd_xref(module_id: str) -> int:
    """Show cross-references for a module."""
    data = _load_json("cross-references.json")
    refs = data.get("references", {})

    # Search in modules first, then concepts, workflows
    for domain in ("modules", "concepts", "workflows"):
        domain_refs = refs.get(domain, {})
        if module_id in domain_refs:
            entry = domain_refs[module_id]
            print(f"Cross-references for {domain}/{module_id}")
            print("=" * 60)

            referenced_by = entry.get("referencedBy", {})
            if referenced_by:
                print("\n  Referenced by:")
                for cat, ids in referenced_by.items():
                    print(f"    {cat}: {', '.join(ids)}")

            references = entry.get("references", {})
            if references:
                print("\n  References:")
                for cat, ids in references.items():
                    print(f"    {cat}: {', '.join(ids)}")
            return 0

    print(f"No cross-references found for '{module_id}'")
    return 1


def cmd_source(file_path: str) -> int:
    """Look up which module/component owns a source file."""
    data = _load_json("search-index.json")
    source_files = data.get("index", {}).get("sourceFiles", {})

    # Exact match
    if file_path in source_files:
        info = source_files[file_path]
        print(f"Source: {file_path}")
        print(f"  module:        {info.get('module', '?')}")
        print(f"  component:     {info.get('component', '?')}")
        print(f"  documentation: {info.get('documentation', '?')}")
        print(f"  metadata:      {info.get('metadata', '?')}")
        return 0

    # Prefix match (for directories or partial paths)
    matches = []
    query_lower = file_path.lower()
    for src, info in source_files.items():
        if query_lower in src.lower():
            matches.append((src, info))

    if matches:
        print(f"Matches for '{file_path}':")
        print("-" * 50)
        for src, info in sorted(matches):
            print(f"  {src}")
            print(f"    module: {info.get('module', '?')}  component: {info.get('component', '?')}")
        return 0

    print(f"No source file mapping found for '{file_path}'")
    return 1


def cmd_concepts() -> int:
    """List all concepts from concepts/index.json."""
    data = _load_json("concepts/index.json")
    print("Concepts")
    print("=" * 60)
    for doc in data.get("documents", data.get("subcategories", [])):
        if isinstance(doc, dict):
            cid = doc.get("id", doc.get("name", "?"))
            desc = doc.get("description", "")
            print(f"  {cid:30s}  {desc}")
    # Also show from search-index concepts section
    si = _load_json("search-index.json")
    concepts = si.get("index", {}).get("concepts", {})
    if concepts:
        print(f"\nConcept → Module mappings:")
        for cname, info in concepts.items():
            related = ", ".join(info.get("relatedModules", []))
            print(f"  {cname:30s}  modules: {related}")
    return 0


def cmd_workflows() -> int:
    """List all workflows from workflows/index.json."""
    data = _load_json("workflows/index.json")
    print("Workflows")
    print("=" * 60)
    for doc in data.get("documents", data.get("subcategories", [])):
        if isinstance(doc, dict):
            wid = doc.get("id", doc.get("name", "?"))
            desc = doc.get("description", "")
            print(f"  {wid:20s}  {desc}")
    return 0


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Navigate and query the Cangjie compiler knowledge base."
    )
    parser.add_argument(
        "--base-dir", type=Path, default=None,
        help="Override knowledge base root directory",
    )
    sub = parser.add_subparsers(dest="action")

    sub.add_parser("overview", help="Show knowledge base overview")
    sub.add_parser("modules", help="List all modules")

    p_mod = sub.add_parser("module", help="Show module details")
    p_mod.add_argument("module_id", help="Module ID (e.g., lex, parse, sema)")

    p_search = sub.add_parser("search", help="Search keywords")
    p_search.add_argument("query", help="Search query (CN/EN)")

    p_xref = sub.add_parser("xref", help="Show cross-references")
    p_xref.add_argument("module_id", help="Module/concept ID")

    p_src = sub.add_parser("source", help="Look up source file ownership")
    p_src.add_argument("file_path", help="Source file path")

    sub.add_parser("concepts", help="List all concepts")
    sub.add_parser("workflows", help="List all workflows")

    args = parser.parse_args()

    if args.base_dir:
        global KB_ROOT
        KB_ROOT = args.base_dir.resolve()

    if not KB_ROOT.is_dir():
        print(f"Knowledge base not found at {KB_ROOT}", file=sys.stderr)
        return 1

    actions = {
        "overview": lambda: cmd_overview(),
        "modules": lambda: cmd_modules(),
        "module": lambda: cmd_module(args.module_id),
        "search": lambda: cmd_search(args.query),
        "xref": lambda: cmd_xref(args.module_id),
        "source": lambda: cmd_source(args.file_path),
        "concepts": lambda: cmd_concepts(),
        "workflows": lambda: cmd_workflows(),
    }

    if not args.action:
        parser.print_help()
        return 0

    return actions[args.action]()


if __name__ == "__main__":
    sys.exit(main())




