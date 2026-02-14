# Using the Cangjie Compiler Knowledge Base

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Overview

This document provides a comprehensive guide for AI assistants on how to effectively use the Cangjie compiler knowledge base. The knowledge base is organized as a 6-layer index system designed for efficient information retrieval.

## The 6-Layer Index System

The knowledge base is structured in six layers, from high-level navigation to detailed content:

### Layer 1: Root Index
**File:** `.claude/knowledge-base/index.md`

The entry point for all queries. Provides:
- Overview of all knowledge domains
- Quick navigation to major sections
- High-level architecture summary
- Common query routing

**When to use:** Start here for unfamiliar queries or broad topics.

### Layer 2: Domain Indexes
**Files:**
- `.claude/knowledge-base/modules/index.md` - Module catalog
- `.claude/knowledge-base/concepts/index.md` - Concept directory
- `.claude/knowledge-base/architecture/index.md` - Architecture overview
- `.claude/knowledge-base/workflows/index.md` - Workflow guides
- `.claude/knowledge-base/code-navigation/index.md` - Code navigation aids

**When to use:** When you know the general domain but need to find specific topics.

### Layer 3: Topic Indexes
**Examples:**
- Module-specific indexes (e.g., AST module, Lex module)
- Concept category indexes
- Workflow category indexes

**When to use:** When navigating within a specific domain to find detailed documents.

### Layer 4: Content Documents
**Examples:**
- `.claude/knowledge-base/concepts/ast.md` - AST concepts
- `.claude/knowledge-base/modules/lex.md` - Lex module details
- `.claude/knowledge-base/workflows/compilation-pipeline.md` - Compilation workflow

**When to use:** For detailed information on specific topics.

### Layer 5: Cross-References
**File:** `.claude/knowledge-base/cross-references.json`

Links related concepts, modules, and workflows. Enables:
- Finding related information
- Understanding dependencies
- Discovering connections between topics

**When to use:** When exploring related topics or understanding relationships.

### Layer 6: Search Index
**File:** `.claude/knowledge-base/search-index.json`

Keyword-based search index for quick lookups. Contains:
- Keywords mapped to documents
- Synonyms and aliases
- Common search terms

**When to use:** For keyword-based searches or when other methods don't yield results.

## Retrieval Strategy

Follow this step-by-step approach for efficient information retrieval:

### Step 1: Classify the Query

Determine the query type:

1. **Module Query** - About a specific compiler module
   - Examples: "How does the lexer work?", "What's in the AST module?"
   - Entry point: `.claude/knowledge-base/modules/index.md`

2. **Concept Query** - About a compiler concept or technique
   - Examples: "What is type checking?", "How does name mangling work?"
   - Entry point: `.claude/knowledge-base/concepts/index.md`

3. **Architecture Query** - About system design or structure
   - Examples: "What's the compiler pipeline?", "How do modules interact?"
   - Entry point: `.claude/knowledge-base/architecture/index.md`

4. **Workflow Query** - About processes or procedures
   - Examples: "How to add a new feature?", "What's the build process?"
   - Entry point: `.claude/knowledge-base/workflows/index.md`

5. **Code Navigation Query** - About finding code or understanding structure
   - Examples: "Where is the parser code?", "What files implement lexing?"
   - Entry point: `.claude/knowledge-base/code-navigation/index.md`

6. **Cross-Cutting Query** - Spans multiple domains
   - Examples: "How does parsing interact with semantic analysis?"
   - Entry point: `.claude/knowledge-base/index.md` or cross-references

### Step 2: Navigate to Entry Point

Based on query classification, start at the appropriate index:

```
Module Query → modules/index.md → specific module doc
Concept Query → concepts/index.md → specific concept doc
Architecture Query → architecture/index.md → architecture doc
Workflow Query → workflows/index.md → specific workflow doc
Code Navigation → code-navigation/index.md → navigation doc
Cross-Cutting → index.md → multiple paths
```

### Step 3: Follow the Index Chain

Navigate through the index hierarchy:

1. Read the index document
2. Identify relevant subsections or links
3. Follow links to more specific indexes or content
4. Repeat until you reach content documents

### Step 4: Extract Information

From content documents:

1. Read the overview/summary section
2. Scan headings for relevant sections
3. Extract specific information needed
4. Note related documents for follow-up

### Step 5: Use Cross-References

If more context is needed:

1. Check the document's metadata (JSON file) for related docs
2. Consult `.claude/knowledge-base/cross-references.json`
3. Follow links to related concepts, modules, or workflows
4. Build a complete understanding

### Step 6: Verify with Search Index

If information seems incomplete:

1. Check `.claude/knowledge-base/search-index.json`
2. Search for keywords related to the query
3. Discover additional relevant documents
4. Fill gaps in understanding

## Common Query Patterns

### Pattern 1: "How does [module] work?"

**Retrieval Path:**
1. Start: `.claude/knowledge-base/modules/index.md`
2. Find module in catalog
3. Navigate to module document (e.g., `modules/lex.md`)
4. Read overview, architecture, and key components
5. Check cross-references for related concepts
6. Optional: Navigate to code files via `code-navigation/key-files.md`

**Example:** "How does the lexer work?"
- modules/index.md → modules/lex.md
- Read: Purpose, architecture, key classes
- Cross-ref: concepts/tokenization.md, code-navigation/key-files.md
- Result: Complete understanding of lexer functionality

### Pattern 2: "What is [concept]?"

**Retrieval Path:**
1. Start: `.claude/knowledge-base/concepts/index.md`
2. Find concept in directory
3. Navigate to concept document (e.g., `concepts/ast.md`)
4. Read definition, explanation, and examples
5. Check which modules implement the concept
6. Optional: Review related concepts

**Example:** "What is an AST?"
- concepts/index.md → concepts/ast.md
- Read: Definition, structure, node types
- Cross-ref: modules/ast.md, modules/parse.md
- Result: Understanding of AST concept and implementation

### Pattern 3: "Where is [code/file]?"

**Retrieval Path:**
1. Start: `.claude/knowledge-base/code-navigation/index.md`
2. Choose navigation aid:
   - `directory-structure.md` for module locations
   - `key-files.md` for important files
   - `naming-conventions.md` for file naming patterns
3. Locate the code
4. Optional: Read module doc for context

**Example:** "Where is the parser code?"
- code-navigation/index.md → code-navigation/directory-structure.md
- Find: Parse module → src/Parse/, include/cangjie/Parse/
- Check: code-navigation/key-files.md for specific files
- Result: src/Parse/Parser.cpp, include/cangjie/Parse/Parser.h

### Pattern 4: "How do I [task]?"

**Retrieval Path:**
1. Start: `.claude/knowledge-base/workflows/index.md`
2. Find relevant workflow
3. Navigate to workflow document
4. Follow step-by-step instructions
5. Check prerequisites and related workflows

**Example:** "How do I add a new token type?"
- workflows/index.md → workflows/adding-tokens.md
- Read: Steps, files to modify, testing procedures
- Cross-ref: modules/lex.md, concepts/tokenization.md
- Result: Complete procedure for adding tokens

### Pattern 5: "How does [A] interact with [B]?"

**Retrieval Path:**
1. Start: `.claude/knowledge-base/index.md` or cross-references
2. Read both module/concept documents
3. Check architecture documents for interaction patterns
4. Use cross-references to find connection points
5. Synthesize understanding from multiple sources

**Example:** "How does parsing interact with semantic analysis?"
- modules/parse.md + modules/sema.md
- architecture/compilation-pipeline.md
- cross-references.json → find connections
- Result: Understanding of parser-sema interface

### Pattern 6: "What files implement [feature]?"

**Retrieval Path:**
1. Start: Identify which module implements the feature
2. Navigate: modules/index.md → specific module doc
3. Check: "Key Files" section in module doc
4. Verify: code-navigation/key-files.md for file details
5. Optional: code-navigation/directory-structure.md for location

**Example:** "What files implement type checking?"
- modules/index.md → modules/sema.md
- Key files: Sema.h, TypeChecker.h, SemaType.cpp
- Location: src/Sema/, include/cangjie/Sema/
- Result: Complete list of type checking files

## Decision Tree for Entry Points

Use this decision tree to quickly choose the right starting point:

```
Query about...
├─ Specific module? → modules/index.md
├─ Compiler concept? → concepts/index.md
├─ System design? → architecture/index.md
├─ How to do something? → workflows/index.md
├─ Where to find code? → code-navigation/index.md
├─ Multiple topics? → index.md (root)
└─ Keyword search? → search-index.json
```

## Tips for Efficient Retrieval

### 1. Start Broad, Then Narrow
- Begin with index documents
- Don't jump directly to content unless you're certain
- Use indexes to understand the landscape

### 2. Use Metadata
- Check JSON metadata files for quick summaries
- Review tags and related docs
- Use version and date info to assess currency

### 3. Follow Cross-References
- Related docs often provide crucial context
- Cross-references reveal non-obvious connections
- Build a web of understanding, not isolated facts

### 4. Leverage Code Navigation
- Use directory-structure.md to understand organization
- Use key-files.md to find important code quickly
- Use naming-conventions.md to predict file names

### 5. Combine Multiple Sources
- Module docs explain "what" and "why"
- Architecture docs explain "how it fits together"
- Workflow docs explain "how to do it"
- Code navigation docs explain "where to find it"

### 6. Cache Common Paths
- Frequently accessed paths: modules/index.md, code-navigation/key-files.md
- Keep mental map of index structure
- Remember common cross-references

### 7. Use Search as Last Resort
- Indexes are more structured and reliable
- Search is useful for keywords and edge cases
- Search can reveal unexpected connections

### 8. Verify Understanding
- Cross-check information across multiple documents
- Ensure consistency between module, concept, and architecture docs
- Use code navigation to verify claims about implementation

## Advanced Retrieval Techniques

### Technique 1: Bidirectional Navigation
- Start from both ends: high-level concept and specific code
- Meet in the middle with module documentation
- Ensures both theoretical and practical understanding

### Technique 2: Dependency Tracing
- Use cross-references to trace dependencies
- Follow module dependencies in architecture docs
- Understand data flow through the system

### Technique 3: Pattern Recognition
- Identify common patterns in module structure
- Apply patterns to predict organization of unfamiliar modules
- Use naming conventions to infer file purposes

### Technique 4: Contextual Expansion
- Start with specific query answer
- Expand to related concepts for context
- Build comprehensive understanding of the area

### Technique 5: Workflow-Driven Learning
- Use workflows as learning paths
- Follow workflows to understand module interactions
- Practical tasks reveal system design

## Common Pitfalls to Avoid

1. **Skipping Indexes** - Don't jump directly to content; indexes provide essential context
2. **Ignoring Metadata** - JSON files contain valuable navigation hints
3. **Single-Source Reliance** - Always cross-reference information
4. **Overlooking Cross-References** - Missing connections leads to incomplete understanding
5. **Neglecting Code Navigation** - Theory without code location is incomplete
6. **Linear Reading** - Use hyperlinks and cross-references for efficient navigation
7. **Forgetting Updates** - Check last_updated dates for currency

## Query Examples with Full Retrieval Paths

### Example 1: "Explain the compilation pipeline"

**Path:**
1. `.claude/knowledge-base/index.md` - Identify as architecture query
2. `.claude/knowledge-base/architecture/index.md` - Find pipeline doc
3. `.claude/knowledge-base/architecture/compilation-pipeline.md` - Read details
4. Cross-ref: modules/lex.md, modules/parse.md, modules/sema.md, modules/codegen.md
5. Result: Complete pipeline understanding with module details

### Example 2: "How do I modify the parser?"

**Path:**
1. `.claude/knowledge-base/workflows/index.md` - Find parser modification workflow
2. `.claude/knowledge-base/workflows/modifying-parser.md` - Read procedure
3. `.claude/knowledge-base/modules/parse.md` - Understand parser architecture
4. `.claude/knowledge-base/code-navigation/key-files.md` - Locate parser files
5. Result: Step-by-step guide with code locations

### Example 3: "What's the relationship between AST and CHIR?"

**Path:**
1. `.claude/knowledge-base/index.md` - Identify as cross-cutting query
2. `.claude/knowledge-base/modules/ast.md` - Understand AST
3. `.claude/knowledge-base/modules/chir.md` - Understand CHIR
4. `.claude/knowledge-base/architecture/ir-design.md` - Understand transformation
5. `.claude/knowledge-base/cross-references.json` - Find connections
6. Result: Complete understanding of AST-to-CHIR lowering

### Example 4: "Where is token definition code?"

**Path:**
1. `.claude/knowledge-base/code-navigation/index.md` - Code location query
2. `.claude/knowledge-base/code-navigation/key-files.md` - Find Lex module files
3. Result: include/cangjie/Basic/TokenKinds.h, include/cangjie/Basic/Tokens.inc
4. Optional: `.claude/knowledge-base/modules/lex.md` - Context on token system

### Example 5: "How does name mangling work?"

**Path:**
1. `.claude/knowledge-base/concepts/index.md` - Concept query
2. `.claude/knowledge-base/concepts/name-mangling.md` - Read concept
3. `.claude/knowledge-base/modules/mangle.md` - Implementation details
4. `.claude/knowledge-base/code-navigation/key-files.md` - Find mangle code
5. Result: Concept understanding + implementation details + code location

## Integration with Code Exploration

The knowledge base complements direct code exploration:

### When to Use Knowledge Base
- Understanding high-level architecture
- Learning module purposes and interactions
- Finding entry points for code exploration
- Understanding design decisions
- Learning workflows and procedures

### When to Use Direct Code Exploration
- Examining specific implementations
- Debugging issues
- Understanding detailed algorithms
- Reviewing recent changes
- Analyzing performance characteristics

### Combined Approach
1. Start with knowledge base for context
2. Identify key files and entry points
3. Explore code with understanding of purpose
4. Return to knowledge base for clarification
5. Update mental model with code insights

## Maintenance and Updates

The knowledge base is versioned and dated:

- Check `last_updated` fields for currency
- Version numbers indicate major changes
- Cross-references are maintained for consistency
- New documents are added as the compiler evolves

When information seems outdated:
1. Check document version and date
2. Verify with actual code if necessary
3. Note discrepancies for knowledge base updates

## Summary

Effective use of the Cangjie compiler knowledge base requires:

1. **Understanding the structure** - 6-layer index system
2. **Classifying queries** - Choose the right entry point
3. **Following the chain** - Navigate through indexes to content
4. **Using cross-references** - Build comprehensive understanding
5. **Combining sources** - Module + concept + architecture + code
6. **Verifying information** - Cross-check across documents

Master these techniques to efficiently retrieve information and build deep understanding of the Cangjie compiler.
