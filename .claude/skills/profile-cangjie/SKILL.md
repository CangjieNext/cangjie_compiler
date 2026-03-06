# Profile Cangjie Compiler

Use this skill to build the Cangjie compiler with performance profiling enabled (compilation time and memory usage).

This skill enables `enableTimer` and `enableMemoryCollect` options in Option.h, builds the compiler, and provides tools to analyze the generated `.mem.prof` files.

## Quick Start (Script)

```bash
python3 .claude/skills/profile-cangjie/scripts/profile-cangjie.py --platform linux_x86_64
python3 .claude/skills/profile-cangjie/scripts/profile-cangjie.py --analyze-only
```

## Manual Steps

### 1. Enable Profiling Options

Edit `include/cangjie/Option/Option.h` and set:
```cpp
bool enableTimer = true;
bool enableMemoryCollect = true;
```

### 2. Build Compiler with Profiling

```bash
cd cangjie_compiler
python3 build.py -t release -v 1.1.0.beta.999
```

### 3. Run Compilation with Profiling

After building, compile Cangjie code with `--profile-compile-memory` option to generate profiling data:
```bash
source output/envsetup.sh

# Example: Profile a simple test
cd /tmp
echo 'main() { println("test") }' > test_mem.cj
cjc --profile-compile-memory test_mem.cj -o test_mem 2>&1

# Profile your own code
cjc --profile-compile-memory your_file.cj -o output 2>&1
```

The `--profile-compile-memory` option generates `.mem.prof` files containing:
- Compilation time statistics per phase
- Memory usage statistics per phase
- Peak memory consumption

### 4. Analyze Profiling Results

Find and analyze `.mem.prof` files:
```bash
# Find all profiling files
find . -name "*.mem.prof"

# View profiling data
cat *.mem.prof

# Example output location
ls -la test_mem.cj.mem.prof
```

### 5. Restore Normal Build

After profiling, restore the original settings:
```cpp
bool enableTimer = false;
bool enableMemoryCollect = false;
```

Then rebuild:
```bash
cd cangjie_compiler
python3 build.py -t release -v 1.1.0.beta.999
```

## Profiling Data Format

The `.mem.prof` files are generated when using `--profile-compile-memory` option and contain:
- Time spent in each compilation phase (Lex, Parse, Sema, CHIR, CodeGen, etc.)
- Memory allocated/deallocated per phase
- Peak memory consumption
- Memory leaks (if any)

Example profiling command:
```bash
cjc --profile-compile-memory test.cj -o test 2>&1
cat test.cj.mem.prof
```

## Notes

- Profiling adds overhead to compilation, so build times will be slower
- The profiling data is most useful for identifying performance bottlenecks
- Use this for development and optimization, not for production builds
- Remember to disable profiling options before committing code
