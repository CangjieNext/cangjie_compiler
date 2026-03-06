# Cangjie Compiler Skills Index

Available skills for working with the Cangjie compiler project.

## Skills

| Skill | Description |
|-------|-------------|
| [build-cangjie](./build-cangjie/SKILL.md) | Build the Cangjie compiler and its dependencies (runtime, stdlib, stdx) |
| [run-cangjie-tests](./run-cangjie-tests/SKILL.md) | Run test cases in the `cangjie_test` repository (HLT, LLT, cjcov, LSP) |
| [use-knowledge-base](./use-knowledge-base/SKILL.md) | Navigate the Cangjie compiler knowledge base using the 6-layer index system |

## Structure

Each skill follows a standard layout:

```
.claude/skills/
├── SKILL.md                          # This index file
├── build-cangjie/
│   ├── SKILL.md                      # Build instructions (multi-platform)
│   ├── REFERENCE.md                  # Platform configs, arguments, env vars
│   ├── LICENSE
│   └── scripts/
│       └── build-cangjie.py          # Automated build script
├── run-cangjie-tests/
│   ├── SKILL.md                      # Test running instructions
│   ├── REFERENCE.md                  # Test framework params, directives
│   ├── LICENSE
│   └── scripts/
│       └── run-cangjie-tests.py      # Test runner helper script
└── use-knowledge-base/
    ├── SKILL.md                      # Knowledge base usage guide
    ├── REFERENCE.md                  # Index system, domains, query types
    ├── LICENSE
    └── scripts/
        └── use-knowledge-base.py     # Knowledge base navigation script
```

## Supported Platforms

All build and test skills support:
- `linux_x86_64`
- `linux_aarch64`
- `mac_x86_64`
- `mac_aarch64`
