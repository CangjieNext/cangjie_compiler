# Skill: Build Cangjie Compiler

## Overview

构建仓颉编译器（cangjie_compiler）及其依赖（runtime、stdlib）的完整流程。

## 目录约定

- `WORKSPACE=/usr1/ace`（cangjie_compiler 的上一层目录）
- 编译器源码：`$WORKSPACE/cangjie_compiler`
- LLVM 源码：`$WORKSPACE/llvm-project`
- Runtime 源码：`$WORKSPACE/cangjie_runtime/runtime`
- 标准库源码：`$WORKSPACE/cangjie_runtime/stdlib`
- stdx 源码：`$WORKSPACE/cangjie_stdx`

---

## 1. 初次全量构建编译器

### 1.1 准备 LLVM

```bash
cp -R $WORKSPACE/llvm-project $WORKSPACE/cangjie_compiler/third_party/
```

### 1.2 构建编译器

```bash
cd $WORKSPACE/cangjie_compiler/compiler
python3 build.py -t release -v 1.1.0.beta.999
```

参数说明：
- `-t release` 或 `-t debug`：构建类型
- `--no-tests`：跳过 unittest 编译（可选）
- `-v 1.1.0.beta.999`：版本号

## 2. 增量编译（仅 cjc）

修改少量文件后，无需全量重编，直接用 ninja：

```bash
cd $WORKSPACE/cangjie_compiler/build/build
ninja cjc
```

> 注意：单独编译 cjc 不能用来编译仓颉项目，还需要编译 runtime 和标准库。

## 3. 编译 Runtime

```bash
cd $WORKSPACE/cangjie_runtime/runtime
python3 build.py clean
python3 build.py build -t release -v 1.1.0-beta.999
python3 build.py install
```

编译完成后，将产物复制到编译器输出目录：

```bash
cp -R $WORKSPACE/cangjie_runtime/runtime/output/common/linux_release_x86_64/{lib,runtime} \
  $WORKSPACE/cangjie_compiler/output/
```

## 4. 编译标准库

前置条件：编译器和 runtime 均已编译完成。

```bash
source $WORKSPACE/cangjie_compiler/output/envsetup.sh

cd $WORKSPACE/cangjie_runtime/stdlib
python3 build.py clean
python3 build.py build -t release --target-lib=$WORKSPACE/cangjie_runtime/runtime/output
python3 build.py install
cp -R output/* $WORKSPACE/cangjie_compiler/output/
```

## 5. 编译 stdx

前置条件：编译器和标准库均已编译完成。

```bash
cd $WORKSPACE/cangjie_stdx
python3 build.py clean
python3 build.py build -t release
python3 build.py install
```

使用前设置环境变量：

```bash
export CANGJIE_STDX_PATH=$WORKSPACE/cangjie_stdx/target/linux_x86_64_cjnative/static/stdx
export LD_LIBRARY_PATH=$CANGJIE_STDX_PATH:$LD_LIBRARY_PATH
```

## 6. 验证

```bash
cd /tmp
echo 'main() { println("Hello, Cangjie") }' > hello.cj
source $WORKSPACE/cangjie_compiler/output/envsetup.sh
cjc hello.cj -o hello && ./hello
# 预期输出: Hello, Cangjie
```

## 7. 完整构建一键流程（从零开始）

```bash
export WORKSPACE=/usr1/ace

# 1) 准备 LLVM
cp -R $WORKSPACE/llvm-project $WORKSPACE/cangjie_compiler/third_party/

# 2) 编译器
cd $WORKSPACE/cangjie_compiler/compiler
python3 build.py -t release --no-tests -v 1.1.0.beta.999

# 3) Runtime
cd $WORKSPACE/cangjie_runtime/runtime
python3 build.py clean
python3 build.py build -t release -v 1.1.0-beta.999
python3 build.py install
cp -R output/common/linux_release_x86_64/{lib,runtime} $WORKSPACE/cangjie_compiler/output/

# 4) 标准库
source $WORKSPACE/cangjie_compiler/output/envsetup.sh
cd $WORKSPACE/cangjie_runtime/stdlib
python3 build.py clean
python3 build.py build -t release --target-lib=$WORKSPACE/cangjie_runtime/runtime/output
python3 build.py install
cp -R output/* $WORKSPACE/cangjie_compiler/output/

# 5) stdx
cd $WORKSPACE/cangjie_stdx
python3 build.py clean
python3 build.py build -t release
python3 build.py install
export CANGJIE_STDX_PATH=$WORKSPACE/cangjie_stdx/target/linux_x86_64_cjnative/static/stdx
export LD_LIBRARY_PATH=$CANGJIE_STDX_PATH:$LD_LIBRARY_PATH

# 6) 验证
cd /tmp
echo 'main() { println("Hello, Cangjie") }' > hello.cj
cjc hello.cj -o hello && ./hello
```
