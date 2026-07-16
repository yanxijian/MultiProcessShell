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
        "--configure-only",
        action="store_true",
        help="Only run cmake configure",
    )
    args = parser.parse_args()

    which_or_die("cmake")
    qtdir = os.environ.get("QTDIR")
    if not qtdir:
        raise SystemExit("error: QTDIR is not set")
    if not Path(qtdir).is_dir():
        raise SystemExit(f"error: QTDIR does not exist: {qtdir}")

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
        f"-DCMAKE_PREFIX_PATH={qtdir}",
    ]
    if args.fresh:
        cmake_cmd.append("--fresh")
    for d in args.defs:
        cmake_cmd.append(f"-D{d}")

    run(cmake_cmd)
    if args.configure_only:
        return 0

    build_cmd = ["cmake", "--build", str(build_dir), "--config", args.config]
    run(build_cmd)
    print("OK: build finished", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
