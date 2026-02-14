# Build Cangjie Compiler

Use this skill to build the Cangjie compiler and its dependencies (runtime, stdlib, stdx).

## Supported Platforms

| Platform | Runtime Output Path | stdx Target |
|----------|-------------------|-------------|
| `linux_x86_64` | `common/linux_release_x86_64` | `linux_x86_64_cjnative` |
| `linux_aarch64` | `common/linux_release_aarch64` | `linux_aarch64_cjnative` |
| `mac_x86_64` | `common/darwin_release_x86_64` | `darwin_x86_64_cjnative` |
| `mac_aarch64` | `common/darwin_release_aarch64` | `darwin_aarch64_cjnative` |

## Directory Structure

Assume workspace root contains:
- `cangjie_compiler/` - Compiler source
- `llvm-project/` - LLVM source
- `cangjie_runtime/runtime/` - Runtime source
- `cangjie_runtime/stdlib/` - Standard library source
- `cangjie_stdx/` - stdx source

Use relative paths from workspace root (e.g., `../cangjie_runtime` from `cangjie_compiler/`).

## Quick Start (Script)

```bash
python3 .claude/skills/build-cangjie/scripts/build-cangjie.py --platform linux_x86_64
python3 .claude/skills/build-cangjie/scripts/build-cangjie.py --platform mac_aarch64 --component compiler
python3 .claude/skills/build-cangjie/scripts/build-cangjie.py --incremental --component cjc
```

## Build Compiler (Full)

### Prepare LLVM
```bash
cd cangjie_compiler
cp -R ../llvm-project third_party/
```

### Build Compiler
```bash
cd cangjie_compiler
python3 build.py -t release -v 1.1.0.beta.999
```

Options:
- `-t release` or `-t debug`: Build type
- `--no-tests`: Skip unittest compilation
- `-v 1.1.0.beta.999`: Version number

## Incremental Build (cjc only)

After modifying source files:
```bash
cd cangjie_compiler/build/build
ninja cjc
```

Note: Incremental cjc build alone cannot compile Cangjie projects. Runtime and stdlib are also required.

## Build Runtime

Replace `PLATFORM` with your target (e.g., `linux_release_x86_64`, `linux_release_aarch64`, `darwin_release_x86_64`, `darwin_release_aarch64`):

```bash
cd cangjie_runtime/runtime
python3 build.py clean
python3 build.py build -t release -v 1.1.0-beta.999
python3 build.py install
```

Copy output to compiler:
```bash
cp -R output/common/PLATFORM/{lib,runtime} ../../cangjie_compiler/output/
```

Platform-specific examples:
```bash
# linux_x86_64
cp -R output/common/linux_release_x86_64/{lib,runtime} ../../cangjie_compiler/output/
# linux_aarch64
cp -R output/common/linux_release_aarch64/{lib,runtime} ../../cangjie_compiler/output/
# mac_x86_64
cp -R output/common/darwin_release_x86_64/{lib,runtime} ../../cangjie_compiler/output/
# mac_aarch64
cp -R output/common/darwin_release_aarch64/{lib,runtime} ../../cangjie_compiler/output/
```

## Build Standard Library

Prerequisites: Compiler and runtime built.

```bash
source cangjie_compiler/output/envsetup.sh

cd cangjie_runtime/stdlib
python3 build.py clean
python3 build.py build -t release --target-lib=../runtime/output
python3 build.py install
cp -R output/* ../../cangjie_compiler/output/
```

## Build stdx

Prerequisites: Compiler and stdlib built.

```bash
cd cangjie_stdx
python3 build.py clean
python3 build.py build -t release
python3 build.py install
```

Set environment (replace `TARGET` with your platform target):
```bash
# linux_x86_64
export CANGJIE_STDX_PATH=$(pwd)/target/linux_x86_64_cjnative/static/stdx
# linux_aarch64
export CANGJIE_STDX_PATH=$(pwd)/target/linux_aarch64_cjnative/static/stdx
# mac_x86_64
export CANGJIE_STDX_PATH=$(pwd)/target/darwin_x86_64_cjnative/static/stdx
# mac_aarch64
export CANGJIE_STDX_PATH=$(pwd)/target/darwin_aarch64_cjnative/static/stdx

export LD_LIBRARY_PATH=$CANGJIE_STDX_PATH:$LD_LIBRARY_PATH
```

## Verify Build

```bash
cd /tmp
echo 'main() { println("Hello, Cangjie") }' > hello.cj
source <workspace>/cangjie_compiler/output/envsetup.sh
cjc hello.cj -o hello && ./hello
```

Expected output: `Hello, Cangjie`
