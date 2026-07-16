#!/usr/bin/env python3
"""
Helper to configure/build a minimal shared Qt (qtbase) into QTDIR.

Does not vendor Qt sources inside MultiProcessShell. Pass --source to an
existing qt-everywhere / qtbase checkout.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def which_or_die(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"error: '{name}' not found on PATH")
    return path


def run(cmd: list[str], cwd: Path | None = None, env: dict | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd, env=env)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        required=True,
        help="Path to qt-everywhere-src-* (or qtbase top-level with configure)",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        required=True,
        help="Out-of-source Qt build directory",
    )
    parser.add_argument(
        "--prefix",
        type=Path,
        default=None,
        help="Install prefix (default: env QTDIR)",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=max(os.cpu_count() or 4, 2),
        help="Parallel build jobs",
    )
    parser.add_argument(
        "--configure-only",
        action="store_true",
    )
    parser.add_argument(
        "--skip-install",
        action="store_true",
    )
    args = parser.parse_args()

    prefix = args.prefix or Path(os.environ.get("QTDIR", ""))
    if not prefix:
        raise SystemExit("error: set --prefix or QTDIR")

    source: Path = args.source.resolve()
    configure = source / "configure.bat"
    if not configure.is_file():
        configure = source / "qtbase" / "configure.bat"
    if not configure.is_file():
        # *nix
        configure = source / "configure"
        if not configure.is_file():
            raise SystemExit(f"error: configure not found under {source}")

    build_dir: Path = args.build_dir.resolve()
    build_dir.mkdir(parents=True, exist_ok=True)
    prefix.mkdir(parents=True, exist_ok=True)

    which_or_die("cmake")

    is_windows = os.name == "nt"
    if is_windows:
        # Expect caller to run from an MSVC developer environment (vcvars).
        cmd = [
            "cmd",
            "/c",
            str(configure),
            "-prefix",
            str(prefix),
            "-release",
            "-shared",
            "-nomake",
            "examples",
            "-nomake",
            "tests",
            "-submodules",
            "qtbase",
            "-opensource",
            "-confirm-license",
            "-platform",
            "win32-msvc",
        ]
        run(cmd, cwd=build_dir)
    else:
        cmd = [
            str(configure),
            "-prefix",
            str(prefix),
            "-release",
            "-shared",
            "-nomake",
            "examples",
            "-nomake",
            "tests",
            "-submodules",
            "qtbase",
            "-opensource",
            "-confirm-license",
        ]
        run(cmd, cwd=build_dir)

    if args.configure_only:
        return 0

    run(["cmake", "--build", str(build_dir), "--parallel", str(args.jobs)])
    if not args.skip_install:
        run(["cmake", "--install", str(build_dir)])

    print(f"OK: Qt installed to {prefix}", flush=True)
    print("Set QTDIR to this prefix and add QTDIR/bin to PATH.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
