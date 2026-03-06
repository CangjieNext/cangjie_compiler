# Profile Cangjie - Reference

## Overview

This skill enables performance profiling for the Cangjie compiler by:
1. Enabling `enableTimer` and `enableMemoryCollect` flags in `Option.h`
2. Building the compiler with profiling instrumentation
3. Analyzing generated `.mem.prof` files after compilation

## Key Files

- `include/cangjie/Option/Option.h` - Contains profiling flags (lines 596-598)
- `*.mem.prof` - Generated profiling data files

## Profiling Flags

```cpp
bool enableTimer = false;          // Line 596 - Enable compilation time tracking
bool enableMemoryCollect = false;  // Line 598 - Enable memory usage tracking
```

## Workflow

1. **Enable Profiling**: Modify Option.h to set both flags to `true`
2. **Build**: Compile the compiler with profiling enabled
3. **Test**: Run compilations with `--profile-compile-memory` flag to generate profiling data
   ```bash
   cjc --profile-compile-memory test.cj -o test 2>&1
   ```
4. **Analyze**: Examine generated `.mem.prof` files
   ```bash
   find . -name "*.mem.prof"
   cat test.cj.mem.prof
   ```
5. **Restore**: Set flags back to `false` and rebuild for production

## Important Notes

- Profiling adds significant overhead - only use for performance analysis
- Always backup Option.h before modifications
- Remember to restore original settings before committing
- `.mem.prof` files are generated when using `--profile-compile-memory` option
- The profiling flags in Option.h must be enabled for `--profile-compile-memory` to work
- Profiling data is written to `<source_file>.mem.prof` in the current directory

## Script Usage

```bash
# Full profiling build
python3 .claude/skills/profile-cangjie/scripts/profile-cangjie.py

# Analyze existing results
python3 .claude/skills/profile-cangjie/scripts/profile-cangjie.py --analyze-only

# Restore original settings
python3 .claude/skills/profile-cangjie/scripts/profile-cangjie.py --restore
```
