#!/usr/bin/env python3
"""Configure and build MultiProcessShell (out-of-source)."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def which_or_die(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"error: '{name}' not found on PATH")
    return path


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd)


def _default_ninja() -> str | None:
    candidates = [
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe",
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe",
    ]
    for c in candidates:
        if c.is_file():
            return str(c)
    return shutil.which("ninja")


def _find_cl() -> str | None:
    cl = shutil.which("cl") or shutil.which("cl.exe")
    if cl:
        return cl
    vswhere = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if not vswhere.is_file():
        return None
    try:
        root = subprocess.check_output(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    if not root:
        return None
    msvc = Path(root) / "VC" / "Tools" / "MSVC"
    if not msvc.is_dir():
        return None
    versions = sorted([p for p in msvc.iterdir() if p.is_dir()], reverse=True)
    for ver in versions:
        candidate = ver / "bin" / "Hostx64" / "x64" / "cl.exe"
        if candidate.is_file():
            return str(candidate)
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=ROOT / "build",
        help="Out-of-source build directory (default: ./build)",
    )
    parser.add_argument(
        "--config",
        default=os.environ.get("MPS_BUILD_CONFIG", "Release"),
        help="Build type / MSVC config (default: Release)",
    )
    parser.add_argument("-G", "--generator", default="Ninja", help="CMake generator")
    parser.add_argument(
        "--fresh",
        action="store_true",
        help="Pass --fresh to cmake configure",
    )
    parser.add_argument(
        "-D",
        dest="defs",
        action="append",
        default=[],
        help="Extra -D CMake definitions (repeatable)",
    )
    parser.add_argument(
        "--no-tests",
        action="store_true",
        help="Configure with MPS_BUILD_TESTS=OFF",
    )
    parser.add_argument(
        "--no-demos",
        action="store_true",
        help="Configure with MPS_BUILD_DEMOS=OFF",
    )
    parser.add_argument(
        "--configure-only",
        action="store_true",
        help="Only run cmake configure",
    )
    parser.add_argument(
        "--test",
        action="store_true",
        help="Run ctest after build",
    )
    args = parser.parse_args()

    which_or_die("cmake")
    qtdir = os.environ.get("QTDIR")

    build_dir: Path = args.build_dir
    build_dir.mkdir(parents=True, exist_ok=True)

    cmake_cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(build_dir),
        "-G",
        args.generator,
        f"-DCMAKE_BUILD_TYPE={args.config}",
        "-DMPS_FETCH_PROTOBUF=ON",
        f"-DMPS_BUILD_TESTS={'OFF' if args.no_tests else 'ON'}",
        f"-DMPS_BUILD_DEMOS={'OFF' if args.no_demos else 'ON'}",
        f"-DMPS_BUILD_SRC={'OFF' if args.no_demos else 'ON'}",
    ]

    if args.generator.lower() == "ninja":
        ninja = _default_ninja()
        if not ninja:
            raise SystemExit("error: ninja not found (required for -G Ninja)")
        cmake_cmd.append(f"-DCMAKE_MAKE_PROGRAM={ninja}")

    if os.name == "nt":
        cl = _find_cl()
        if not cl:
            raise SystemExit(
                "error: MSVC cl.exe not found. Run from an x64 Native Tools / vcvars shell."
            )
        cmake_cmd.extend(
            [
                f"-DCMAKE_C_COMPILER={cl}",
                f"-DCMAKE_CXX_COMPILER={cl}",
            ]
        )
        # Ensure Ninja/CMake compile tests see the same toolchain env.
        cl_dir = str(Path(cl).parent)
        os.environ["PATH"] = cl_dir + os.pathsep + os.environ.get("PATH", "")
        os.environ["CC"] = cl
        os.environ["CXX"] = cl

    if qtdir:
        cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={qtdir}")
    elif not args.no_demos:
        raise SystemExit("error: QTDIR is required to build demos (or pass --no-demos)")
    if args.fresh:
        cmake_cmd.append("--fresh")
    for d in args.defs:
        cmake_cmd.append(f"-D{d}")

    run(cmake_cmd)
    if args.configure_only:
        return 0

    build_cmd = ["cmake", "--build", str(build_dir), "--config", args.config]
    run(build_cmd)

    if args.test and not args.no_tests:
        which_or_die("ctest")
        run(
            [
                "ctest",
                "--test-dir",
                str(build_dir),
                "--output-on-failure",
                "-C",
                args.config,
            ]
        )

    print("OK: build finished", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
