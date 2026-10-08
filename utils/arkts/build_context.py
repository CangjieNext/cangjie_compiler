#!/usr/bin/env python3
# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# Licensed under Apache-2.0 with Runtime Library Exception.
"""Build an application context provider with a Windows-hosted OHOS SDK.

The application build coordinator invokes this once before compiling @ArkTS
consumers, propagates the output CJO/link paths, and packages the sole provider.
This driver does not install or replace any SDK file.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from generate_context import PACKAGE


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app-id", required=True)
    parser.add_argument("--target", required=True,
                        choices=("x86_64-linux-ohos", "aarch64-linux-ohos"))
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--native-root", type=Path, required=True)
    parser.add_argument("--cjc", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    sdk, native, output = args.sdk_root.resolve(), args.native_root.resolve(), args.output_dir.resolve()
    compiler = args.cjc.resolve()
    arch = args.target.split("-", 1)[0]
    api = sdk / "api/lib" / f"linux_ohos_{arch}_cjnative"
    interop = api / "ohos/libohos.ark_interop.so"
    readelf = native / "llvm/bin/llvm-readelf.exe"
    required = (compiler, readelf, interop)
    for path in required:
        if not path.is_file():
            parser.error(f"missing required input: {path}")
    clang_paths = sorted((native / "llvm/lib/clang").glob(f"*/lib/{args.target}"))
    if len(clang_paths) != 1:
        parser.error("expected exactly one target Clang runtime directory")
    # Validation precedes all output writes; retain failed builds for diagnosis.
    generator = Path(__file__).with_name("generate_context.py")
    if output.exists():
        parser.error("output directory must be fresh")
    subprocess.run([sys.executable, str(generator), "--app-id", args.app_id,
                    "--target", args.target, "--output-dir", str(output)], check=True)
    source = output / "context.cj"
    manifest_path = output / "context-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["build_driver_sha256"] = sha256(Path(__file__))
    input_hashes = {str(path): sha256(path) for path in required}
    command = [str(compiler), str(source), f"--target={args.target}",
               f"--import-path={api}", "--output-type=dylib", "--dy-std",
               "--output-dir", str(output), "-B", str(native / f"sysroot/usr/lib/{args.target}")]
    for directory in (native / f"sysroot/usr/lib/{args.target}", clang_paths[0],
                      native / f"llvm/lib/{args.target}", api / "ohos"):
        command.extend(["-L", str(directory)])
    command.extend(["-l:libohos.ark_interop.so", "--sysroot", str(native / "sysroot"),
                    "-o", manifest["provider_filename"], "--diagnostic-format=noColor"])
    env = os.environ.copy()
    env["CANGJIE_HOME"] = str(sdk / "build-tools")
    env["PATH"] = os.pathsep.join([str(native / "llvm/bin"), str(compiler.parent),
                                   str(sdk / "build-tools/third_party/llvm/bin"), env.get("PATH", "")])
    version = subprocess.run([str(compiler), "-v"], env=env, capture_output=True, check=True)
    (output / "compiler-version.txt").write_bytes(version.stdout + version.stderr)
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (output / "compile.log").write_bytes(result.stdout)
    manifest.update({"compiler_inputs": input_hashes, "build_command": command,
                     "build_exit_code": result.returncode, "build_status": "CompileFailed"})
    if result.returncode == 0:
        products = (output / f"{PACKAGE}.cjo", output / manifest["provider_filename"])
        dynamic = subprocess.run([str(readelf), "-d", str(products[1])], capture_output=True, check=True)
        (output / "dynamic.txt").write_bytes(dynamic.stdout)
        if b"libohos.ark_interop.so" not in dynamic.stdout:
            raise RuntimeError("provider is missing its existing interop dynamic dependency")
        if sha256(source) != manifest["source_sha256"] or any(sha256(Path(p)) != h for p, h in input_hashes.items()):
            raise RuntimeError("build inputs changed during compilation")
        manifest["products"] = [{"file": p.name, "sha256": sha256(p), "bytes": p.stat().st_size}
                                for p in products]
        manifest["build_status"] = "Compiled"
    manifest_path.write_bytes((json.dumps(manifest, indent=2) + "\n").encode("utf-8"))
    if result.returncode:
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
