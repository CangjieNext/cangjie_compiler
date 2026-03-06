#!/usr/bin/env python3
"""Profile script for Cangjie compiler with performance monitoring.

This script enables enableTimer and enableMemoryCollect in Option.h,
builds the compiler, and provides tools to analyze profiling results.

Usage:
    python3 profile-cangjie.py [--platform PLATFORM] [--build-type TYPE]
    python3 profile-cangjie.py --analyze-only
    python3 profile-cangjie.py --restore

Examples:
    python3 profile-cangjie.py --platform linux_x86_64
    python3 profile-cangjie.py --analyze-only
    python3 profile-cangjie.py --restore
"""

import argparse
import os
import re
import subprocess
import sys

SUPPORTED_PLATFORMS = ["linux_x86_64", "linux_aarch64", "mac_x86_64", "mac_aarch64"]


def run(cmd, cwd=None):
    """Run a shell command and exit on failure."""
    print(f"[profile] {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd)
    if result.returncode != 0:
        print(f"[profile] FAILED: {cmd}", file=sys.stderr)
        sys.exit(result.returncode)


def find_workspace_root():
    """Find workspace root (parent of cangjie_compiler)."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # scripts/ -> profile-cangjie/ -> skills/ -> .claude/ -> cangjie_compiler/ -> workspace
    compiler_root = os.path.abspath(os.path.join(script_dir, "..", "..", "..", ".."))
    workspace_root = os.path.dirname(compiler_root)
    return workspace_root, compiler_root


def backup_option_h(option_h_path):
    """Backup Option.h file."""
    backup_path = option_h_path + ".backup"
    if not os.path.exists(backup_path):
        run(f"cp {option_h_path} {backup_path}")
        print(f"[profile] Backed up Option.h to {backup_path}")
    else:
        print(f"[profile] Backup already exists at {backup_path}")


def enable_profiling(option_h_path):
    """Enable enableTimer and enableMemoryCollect in Option.h."""
    print(f"[profile] Enabling profiling options in {option_h_path}")

    with open(option_h_path, 'r') as f:
        content = f.read()

    # Replace enableTimer = false with enableTimer = true
    content = re.sub(
        r'bool enableTimer\s*=\s*false;',
        'bool enableTimer = true;',
        content
    )

    # Replace enableMemoryCollect = false with enableMemoryCollect = true
    content = re.sub(
        r'bool enableMemoryCollect\s*=\s*false;',
        'bool enableMemoryCollect = true;',
        content
    )

    with open(option_h_path, 'w') as f:
        f.write(content)

    print("[profile] Profiling options enabled")


def disable_profiling(option_h_path):
    """Disable enableTimer and enableMemoryCollect in Option.h."""
    print(f"[profile] Disabling profiling options in {option_h_path}")

    with open(option_h_path, 'r') as f:
        content = f.read()

    # Replace enableTimer = true with enableTimer = false
    content = re.sub(
        r'bool enableTimer\s*=\s*true;',
        'bool enableTimer = false;',
        content
    )

    # Replace enableMemoryCollect = true with enableMemoryCollect = false
    content = re.sub(
        r'bool enableMemoryCollect\s*=\s*true;',
        'bool enableMemoryCollect = false;',
        content
    )

    with open(option_h_path, 'w') as f:
        f.write(content)

    print("[profile] Profiling options disabled")


def restore_option_h(option_h_path):
    """Restore Option.h from backup."""
    backup_path = option_h_path + ".backup"
    if os.path.exists(backup_path):
        run(f"cp {backup_path} {option_h_path}")
        print(f"[profile] Restored Option.h from backup")
    else:
        print(f"[profile] No backup found at {backup_path}", file=sys.stderr)
        sys.exit(1)


def build_compiler(compiler_root, build_type, version):
    """Build the Cangjie compiler."""
    print(f"\n[profile] Building compiler with profiling enabled...")
    cmd = f"python3 build.py -t {build_type} -v {version}"
    run(cmd, cwd=compiler_root)


def analyze_profiling_results(compiler_root):
    """Find and display profiling results."""
    print(f"\n[profile] Searching for profiling results...")

    # Search in common locations
    search_paths = [
        os.path.join(compiler_root, "build", "build"),
        "/tmp",
        os.getcwd()
    ]

    all_prof_files = []
    for search_path in search_paths:
        if os.path.exists(search_path):
            result = subprocess.run(
                f"find {search_path} -name '*.mem.prof' 2>/dev/null",
                shell=True,
                capture_output=True,
                text=True
            )
            if result.returncode == 0 and result.stdout.strip():
                all_prof_files.extend(result.stdout.strip().split('\n'))

    if all_prof_files:
        print(f"\n[profile] Found {len(all_prof_files)} profiling file(s):")
        for f in all_prof_files:
            print(f"  - {f}")

        print(f"\n[profile] To view profiling data:")
        print(f"  cat <file.mem.prof>")
    else:
        print(f"[profile] No .mem.prof files found")
        print(f"\n[profile] To generate profiling data, compile with --profile-compile-memory:")
        print(f"  source {os.path.join(compiler_root, 'output', 'envsetup.sh')}")
        print(f"  cd /tmp")
        print(f"  echo 'main() {{ println(\"test\") }}' > test_mem.cj")
        print(f"  cjc --profile-compile-memory test_mem.cj -o test_mem 2>&1")
        print(f"  cat test_mem.cj.mem.prof")


def main():
    parser = argparse.ArgumentParser(description="Profile Cangjie compiler build")
    parser.add_argument("--platform", choices=SUPPORTED_PLATFORMS,
                        help="Target platform (auto-detected if not specified)")
    parser.add_argument("--build-type", default="release", choices=["release", "debug"],
                        help="Build type (default: release)")
    parser.add_argument("--version", default="1.1.0.beta.999",
                        help="Version number (default: 1.1.0.beta.999)")
    parser.add_argument("--analyze-only", action="store_true",
                        help="Only analyze existing profiling results without building")
    parser.add_argument("--restore", action="store_true",
                        help="Restore Option.h from backup and disable profiling")
    args = parser.parse_args()

    workspace_root, compiler_root = find_workspace_root()
    option_h_path = os.path.join(compiler_root, "include", "cangjie", "Option", "Option.h")

    if args.restore:
        restore_option_h(option_h_path)
        print("\n[profile] Option.h restored. You may want to rebuild the compiler.")
        return

    if args.analyze_only:
        analyze_profiling_results(compiler_root)
        return

    # Full profiling build
    backup_option_h(option_h_path)
    enable_profiling(option_h_path)

    try:
        build_compiler(compiler_root, args.build_type, args.version)
        print("\n[profile] Build completed successfully!")
        print("\n[profile] Next steps:")
        print("  1. Run compilations with --profile-compile-memory to generate profiling data:")
        print("     source output/envsetup.sh")
        print("     cd /tmp")
        print("     echo 'main() { println(\"test\") }' > test_mem.cj")
        print("     cjc --profile-compile-memory test_mem.cj -o test_mem 2>&1")
        print("     cat test_mem.cj.mem.prof")
        print()
        print("  2. Analyze results:")
        print(f"     python3 {__file__} --analyze-only")
        print()
        print("  3. Restore normal build:")
        print(f"     python3 {__file__} --restore")
    except Exception as e:
        print(f"\n[profile] Build failed: {e}", file=sys.stderr)
        print(f"[profile] Restoring Option.h...")
        restore_option_h(option_h_path)
        sys.exit(1)


if __name__ == "__main__":
    main()
