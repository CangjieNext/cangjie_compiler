# Use Knowledge Base Reference

## Script Arguments

| Argument | Values | Default | Description |
|----------|--------|---------|-------------|
| `--action` | `list-domains`, `search`, `show` | `list-domains` | Action to perform |
| `--query` | string | (none) | Search query or document path |

## 6-Layer Index System

| Layer | Entry Point | Purpose |
|-------|------------|---------|
| 1. Root Index | `knowledge-base/index.md` | Top-level navigation |
| 2. Domain Indexes | `knowledge-base/<domain>/index.md` | Domain-specific navigation |
| 3. Topic Indexes | Module/concept-specific indexes | Topic-level navigation |
| 4. Content Documents | Individual `.md` files | Detailed information |
| 5. Cross-References | `cross-references.json` | Relationship mapping |
| 6. Search Index | `search-index.json` | Keyword-based lookup |

## Knowledge Domains

| Domain | Description |
|--------|-------------|
| `modules` | Compiler module documentation |
| `concepts` | Compiler concepts and techniques |
| `architecture` | System design and structure |
| `workflows` | Processes and procedures |
| `code-navigation` | Code location and structure aids |

## Query Classification

| Query Type | Entry Point | Example |
|-----------|-------------|---------|
| Module | `modules/index.md` | "How does the lexer work?" |
| Concept | `concepts/index.md` | "What is type checking?" |
| Architecture | `architecture/index.md` | "What's the compiler pipeline?" |
| Workflow | `workflows/index.md` | "How to add a new feature?" |
| Code Navigation | `code-navigation/index.md` | "Where is the parser code?" |
| Cross-Cutting | `index.md` | "How does parsing interact with sema?" |

## Retrieval Strategy

1. Classify the query type
2. Navigate to the appropriate entry point
3. Follow the index chain to content documents
4. Extract needed information
5. Use cross-references for additional context
6. Verify with search index if incomplete
