# Build Cangjie Compiler

Use this skill to build the Cangjie compiler and its dependencies (runtime, stdlib, stdx).

## Directory Structure

Assume workspace root contains:
- `cangjie_compiler/` - Compiler source
- `llvm-project/` - LLVM source
- `cangjie_runtime/runtime/` - Runtime source
- `cangjie_runtime/stdlib/` - Standard library source
- `cangjie_stdx/` - stdx source

Use relative paths from workspace root (e.g., `../cangjie_runtime` from `cangjie_compiler/`).

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

```bash
cd cangjie_runtime/runtime
python3 build.py clean
python3 build.py build -t release -v 1.1.0-beta.999
python3 build.py install
```

Copy output to compiler:
```bash
cp -R output/common/linux_release_x86_64/{lib,runtime} ../../cangjie_compiler/output/
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

Set environment:
```bash
export CANGJIE_STDX_PATH=$(pwd)/target/linux_x86_64_cjnative/static/stdx
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

## Full Build Script

```bash
# 1) Prepare LLVM
cd cangjie_compiler
cp -R ../llvm-project third_party/

# 2) Build compiler
python3 build.py -t release --no-tests -v 1.1.0.beta.999

# 3) Build runtime
cd ../cangjie_runtime/runtime
python3 build.py clean
python3 build.py build -t release -v 1.1.0-beta.999
python3 build.py install
cp -R output/common/linux_release_x86_64/{lib,runtime} ../../cangjie_compiler/output/

# 4) Build stdlib
source ../../cangjie_compiler/output/envsetup.sh
cd ../stdlib
python3 build.py clean
python3 build.py build -t release --target-lib=../runtime/output
python3 build.py install
cp -R output/* ../../cangjie_compiler/output/

# 5) Build stdx
cd ../../cangjie_stdx
python3 build.py clean
python3 build.py build -t release
python3 build.py install
export CANGJIE_STDX_PATH=$(pwd)/target/linux_x86_64_cjnative/static/stdx
export LD_LIBRARY_PATH=$CANGJIE_STDX_PATH:$LD_LIBRARY_PATH

# 6) Verify
cd /tmp
echo 'main() { println("Hello, Cangjie") }' > hello.cj
cjc hello.cj -o hello && ./hello
```
