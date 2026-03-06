# Building Workflow - 构建编译器

**Last Updated:** 2026-02-12
**Version:** 1.0.0

## Objective

构建仓颉编译器及其依赖，包括 LLVM、编译器本身、Runtime 和标准库。

## Prerequisites

- LLVM source code at `../llvm-project`
- Python 3
- CMake
- Ninja build system
- Sufficient disk space (at least 50GB recommended)
- GCC or Clang compiler toolchain

## Steps

### 1. 准备 LLVM

Copy LLVM source code to the third_party directory:

```bash
cp -R ../llvm-project third_party/
```

**Involved Modules:** Build system, LLVM integration

### 2. 编译器全量构建

Build the compiler with specified configuration:

```bash
cd compiler
python3 build.py -t release -v 1.1.0.beta.999
```

**Options:**
- `-t release` or `-t debug` - Build type
- `--no-tests` - Skip building tests
- `-v version` - Specify version number

**Involved Modules:** Driver, Frontend, Lex, Parse, AST, Sema, CHIR, CodeGen

### 3. 增量编译

For faster incremental builds after making changes:

```bash
cd build/build
ninja cjc
```

**Involved Modules:** Build system, modified modules only

### 4. 编译 Runtime

Build and install the Cangjie runtime:

```bash
cd ../cangjie_runtime/runtime
python3 build.py clean
python3 build.py build -t release -v 1.1.0-beta.999
python3 build.py install
cp -R output/common/linux_release_x86_64/{lib,runtime} ../../cangjie_compiler/output/
```

**Involved Modules:** Runtime system, memory management, GC

### 5. 编译标准库

Build and install the standard library:

```bash
source output/envsetup.sh
cd ../cangjie_runtime/stdlib
python3 build.py clean
python3 build.py build -t release --target-lib=$WORKSPACE/cangjie_runtime/runtime/output
python3 build.py install
cp -R output/* $WORKSPACE/cangjie_compiler/output/
```

**Involved Modules:** Standard library, runtime integration

### 6. 验证

Verify the build by compiling and running a simple program:

```bash
echo 'main() { println("Hello, Cangjie") }' > hello.cj
cjc hello.cj -o hello
./hello
```

Expected output: `Hello, Cangjie`

**Involved Modules:** Complete toolchain

## Involved Modules

- **Build System:** CMake, Ninja, Python build scripts
- **LLVM:** Third-party dependency for code generation
- **Compiler:** All compiler modules (Lex, Parse, AST, Sema, CHIR, CodeGen)
- **Runtime:** Memory management, garbage collection, runtime support
- **Standard Library:** Core language libraries and APIs
- **Driver:** Compiler driver and toolchain orchestration

## Troubleshooting

### LLVM not found

**Symptoms:** Build fails with "LLVM not found" or similar error

**Solutions:**
- Verify LLVM source exists at `../llvm-project`
- Check that LLVM was copied correctly to `third_party/`
- Ensure LLVM version is compatible with the compiler

### Build failures

**Symptoms:** Compilation errors during build process

**Solutions:**
- Check that all prerequisites are installed
- Verify Python 3 version is compatible (3.7+ recommended)
- Ensure sufficient disk space is available
- Try cleaning build directory: `python3 build.py clean`
- Check compiler toolchain (GCC/Clang) version

### Missing dependencies

**Symptoms:** Linker errors or missing library errors

**Solutions:**
- Install required system packages (cmake, ninja-build, python3-dev)
- Verify all build steps completed successfully
- Check that runtime and stdlib were installed to correct locations
- Ensure environment variables are set correctly (run `envsetup.sh`)

### Incremental build issues

**Symptoms:** Changes not reflected after ninja build

**Solutions:**
- Perform a full clean rebuild: `python3 build.py clean && python3 build.py -t release`
- Check that you're building the correct target
- Verify CMake cache is up to date

### Runtime or stdlib not found

**Symptoms:** Compiled programs fail to run with library errors

**Solutions:**
- Verify runtime and stdlib were copied to `output/` directory
- Check that `envsetup.sh` was sourced
- Ensure library paths are correct in environment variables
- Rebuild runtime and stdlib if necessary

## Reference

See also: `build-cangjie.md` skill for automated build procedures.
