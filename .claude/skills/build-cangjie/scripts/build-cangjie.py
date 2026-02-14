#!/usr/bin/env python3
"""Build script for Cangjie compiler and its dependencies.

Supports platforms: linux_x86_64, linux_aarch64, mac_x86_64, mac_aarch64

Usage:
    python3 build-cangjie.py [--platform PLATFORM] [--build-type TYPE] [--version VERSION]
                             [--component COMPONENT] [--no-tests] [--incremental]

Examples:
    python3 build-cangjie.py --platform linux_x86_64 --build-type release
    python3 build-cangjie.py --platform mac_aarch64 --component compiler
    python3 build-cangjie.py --incremental --component cjc
"""

import argparse
import os
import subprocess
import sys

SUPPORTED_PLATFORMS = ["linux_x86_64", "linux_aarch64", "mac_x86_64", "mac_aarch64"]
COMPONENTS = ["all", "compiler", "cjc", "runtime", "stdlib", "stdx"]

PLATFORM_CONFIG = {
    "linux_x86_64": {
        "runtime_output": "common/linux_release_x86_64",
        "stdx_target": "linux_x86_64_cjnative",
    },
    "linux_aarch64": {
        "runtime_output": "common/linux_release_aarch64",
        "stdx_target": "linux_aarch64_cjnative",
    },
    "mac_x86_64": {
        "runtime_output": "common/darwin_release_x86_64",
        "stdx_target": "darwin_x86_64_cjnative",
    },
    "mac_aarch64": {
        "runtime_output": "common/darwin_release_aarch64",
        "stdx_target": "darwin_aarch64_cjnative",
    },
}


def run(cmd, cwd=None):
    """Run a shell command and exit on failure."""
    print(f"[build] {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd)
    if result.returncode != 0:
        print(f"[build] FAILED: {cmd}", file=sys.stderr)
        sys.exit(result.returncode)


def detect_platform():
    """Auto-detect the current platform."""
    import platform as plat
    machine = plat.machine().lower()
    system = plat.system().lower()

    if system == "linux":
        os_name = "linux"
    elif system == "darwin":
        os_name = "mac"
    else:
        print(f"Unsupported OS: {system}", file=sys.stderr)
        sys.exit(1)

    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "aarch64"
    else:
        print(f"Unsupported architecture: {machine}", file=sys.stderr)
        sys.exit(1)

    return f"{os_name}_{arch}"


def find_workspace_root():
    """Find workspace root (parent of cangjie_compiler)."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # scripts/ -> build-cangjie/ -> skills/ -> .claude/ -> cangjie_compiler/ -> workspace
    compiler_root = os.path.abspath(os.path.join(script_dir, "..", "..", "..", ".."))
    workspace_root = os.path.dirname(compiler_root)
    return workspace_root, compiler_root


def prepare_llvm(workspace_root, compiler_root):
    """Copy LLVM source into compiler third_party."""
    llvm_src = os.path.join(workspace_root, "llvm-project")
    llvm_dst = os.path.join(compiler_root, "third_party", "llvm-project")
    if not os.path.exists(llvm_src):
        print(f"[build] LLVM source not found at {llvm_src}", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(llvm_dst):
        run(f"cp -R {llvm_src} {os.path.join(compiler_root, 'third_party/')}")
    else:
        print("[build] LLVM already in third_party, skipping copy")


def build_compiler(compiler_root, build_type, version, no_tests):
    """Build the Cangjie compiler."""
    cmd = f"python3 build.py -t {build_type} -v {version}"
    if no_tests:
        cmd += " --no-tests"
    run(cmd, cwd=compiler_root)


def build_cjc_incremental(compiler_root):
    """Incremental build of cjc only."""
    build_dir = os.path.join(compiler_root, "build", "build")
    run("ninja cjc", cwd=build_dir)


def build_runtime(workspace_root, compiler_root, platform, build_type, version):
    """Build the Cangjie runtime."""
    cfg = PLATFORM_CONFIG[platform]
    runtime_dir = os.path.join(workspace_root, "cangjie_runtime", "runtime")
    run("python3 build.py clean", cwd=runtime_dir)
    run(f"python3 build.py build -t {build_type} -v {version}", cwd=runtime_dir)
    run("python3 build.py install", cwd=runtime_dir)
    output_path = os.path.join(runtime_dir, "output", cfg["runtime_output"])
    dest = os.path.join(compiler_root, "output")
    run(f"cp -R {output_path}/lib {output_path}/runtime {dest}/")


def build_stdlib(workspace_root, compiler_root, build_type):
    """Build the standard library."""
    envsetup = os.path.join(compiler_root, "output", "envsetup.sh")
    stdlib_dir = os.path.join(workspace_root, "cangjie_runtime", "stdlib")
    runtime_output = os.path.join(workspace_root, "cangjie_runtime", "runtime", "output")
    run(f"bash -c 'source {envsetup} && cd {stdlib_dir} && python3 build.py clean'")
    run(f"bash -c 'source {envsetup} && cd {stdlib_dir} && python3 build.py build -t {build_type} --target-lib={runtime_output}'")
    run(f"bash -c 'source {envsetup} && cd {stdlib_dir} && python3 build.py install'")
    run(f"cp -R {os.path.join(stdlib_dir, 'output')}/* {os.path.join(compiler_root, 'output')}/")


def build_stdx(workspace_root, compiler_root, platform, build_type):
    """Build stdx."""
    cfg = PLATFORM_CONFIG[platform]
    stdx_dir = os.path.join(workspace_root, "cangjie_stdx")
    envsetup = os.path.join(compiler_root, "output", "envsetup.sh")
    run(f"bash -c 'source {envsetup} && cd {stdx_dir} && python3 build.py clean'")
    run(f"bash -c 'source {envsetup} && cd {stdx_dir} && python3 build.py build -t {build_type}'")
    run(f"bash -c 'source {envsetup} && cd {stdx_dir} && python3 build.py install'")
    stdx_path = os.path.join(stdx_dir, "target", cfg["stdx_target"], "static", "stdx")
    print(f"\n[build] Set environment variables:")
    print(f"  export CANGJIE_STDX_PATH={stdx_path}")
    print(f"  export LD_LIBRARY_PATH={stdx_path}:$LD_LIBRARY_PATH")


def verify_build(compiler_root):
    """Verify the build by compiling and running a hello world program."""
    envsetup = os.path.join(compiler_root, "output", "envsetup.sh")
    test_code = 'main() { println("Hello, Cangjie") }'
    run(f"bash -c 'source {envsetup} && cd /tmp && echo \\'{test_code}\\' > hello.cj && cjc hello.cj -o hello && ./hello'")


def main():
    parser = argparse.ArgumentParser(description="Build Cangjie compiler and dependencies")
    parser.add_argument("--platform", choices=SUPPORTED_PLATFORMS, default=None,
                        help="Target platform (auto-detected if omitted)")
    parser.add_argument("--build-type", choices=["release", "debug"], default="release",
                        help="Build type (default: release)")
    parser.add_argument("--version", default="1.1.0.beta.999",
                        help="Version string (default: 1.1.0.beta.999)")
    parser.add_argument("--component", choices=COMPONENTS, default="all",
                        help="Component to build (default: all)")
    parser.add_argument("--no-tests", action="store_true",
                        help="Skip unittest compilation")
    parser.add_argument("--incremental", action="store_true",
                        help="Incremental build (only for cjc component)")
    parser.add_argument("--verify", action="store_true",
                        help="Run verification after build")
    args = parser.parse_args()

    platform = args.platform or detect_platform()
    print(f"[build] Platform: {platform}")
    print(f"[build] Build type: {args.build_type}")
    print(f"[build] Component: {args.component}")

    workspace_root, compiler_root = find_workspace_root()

    if args.incremental and args.component == "cjc":
        build_cjc_incremental(compiler_root)
        return

    if args.component in ("all", "compiler"):
        prepare_llvm(workspace_root, compiler_root)
        build_compiler(compiler_root, args.build_type, args.version, args.no_tests)

    if args.component in ("all", "runtime"):
        # Use dash version for runtime
        rt_version = args.version.replace(".beta.", "-beta.")
        build_runtime(workspace_root, compiler_root, platform, args.build_type, rt_version)

    if args.component in ("all", "stdlib"):
        build_stdlib(workspace_root, compiler_root, args.build_type)

    if args.component in ("all", "stdx"):
        build_stdx(workspace_root, compiler_root, platform, args.build_type)

    if args.verify or args.component == "all":
        verify_build(compiler_root)

    print("\n[build] Done.")


if __name__ == "__main__":
    main()


