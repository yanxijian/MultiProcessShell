#!/usr/bin/env python3
"""Install QThemeEngine → QFluentRibbon → MultiProcessShell into a local prefix.

Default prefix: D:/Codes/prefix (override with --prefix or MPS_PREFIX).
Requires QTDIR (or --qt) and an x64 MSVC environment (vcvars) on Windows.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


CODES = Path(__file__).resolve().parents[2]
DEFAULT_PREFIX = Path(os.environ.get("MPS_PREFIX", str(CODES / "prefix")))
DEFAULT_QT = Path(os.environ.get("QTDIR", str(CODES / "Qt6.8.4")))


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd)


def which_or_die(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"error: '{name}' not found on PATH (run from vcvars x64 shell)")
    return path


def configure_build_install(
    *,
    source: Path,
    build: Path,
    prefix: Path,
    qt: Path,
    extra_defs: list[str],
) -> None:
    which_or_die("cmake")
    build.mkdir(parents=True, exist_ok=True)
    prefix_path = f"{prefix.as_posix()};{qt.as_posix()}"
    cmd = [
        "cmake",
        "-S",
        str(source),
        "-B",
        str(build),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={prefix.as_posix()}",
        f"-DCMAKE_PREFIX_PATH={prefix_path}",
        *extra_defs,
    ]
    run(cmd)
    run(["cmake", "--build", str(build)])
    run(["cmake", "--install", str(build)])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--prefix",
        type=Path,
        default=DEFAULT_PREFIX,
        help=f"Install prefix (default: {DEFAULT_PREFIX})",
    )
    parser.add_argument(
        "--qt",
        type=Path,
        default=DEFAULT_QT,
        help=f"Qt prefix (default: QTDIR or {DEFAULT_QT})",
    )
    parser.add_argument(
        "--skip-qte",
        action="store_true",
        help="Skip QThemeEngine (assume already installed into prefix)",
    )
    parser.add_argument(
        "--skip-qfr",
        action="store_true",
        help="Skip QFluentRibbon",
    )
    parser.add_argument(
        "--skip-mps",
        action="store_true",
        help="Skip MultiProcessShell",
    )
    parser.add_argument(
        "--mps-tests",
        action="store_true",
        help="Build MPS tests (default OFF for faster stack install)",
    )
    args = parser.parse_args()

    prefix: Path = args.prefix
    qt: Path = args.qt
    if not qt.is_dir():
        raise SystemExit(f"error: Qt prefix not found: {qt}")

    # Avoid Strawberry Perl's bin shadowing link.exe on Windows.
    path_parts = [p for p in os.environ.get("PATH", "").split(os.pathsep) if p and "Strawberry" not in p]
    os.environ["PATH"] = os.pathsep.join(path_parts)
    os.environ["QTDIR"] = str(qt)

    qte = CODES / "QThemeEngine"
    qfr = CODES / "QFluentRibbon"
    mps = CODES / "MultiProcessShell"

    if not args.skip_qte:
        if not (qte / "CMakeLists.txt").is_file():
            raise SystemExit(f"error: missing {qte}")
        configure_build_install(
            source=qte,
            build=qte / "build-shared",
            prefix=prefix,
            qt=qt,
            extra_defs=[
                "-DQTE_BUILD_SHARED=ON",
                "-DQTE_INSTALL=ON",
                "-DQTE_BUILD_TESTS=OFF",
                "-DQTE_BUILD_EXAMPLES=ON",
            ],
        )

    if not args.skip_qfr:
        if not (qfr / "CMakeLists.txt").is_file():
            raise SystemExit(f"error: missing {qfr}")
        configure_build_install(
            source=qfr,
            build=qfr / "build-shared",
            prefix=prefix,
            qt=qt,
            extra_defs=[
                "-DQFR_BUILD_SHARED=ON",
                "-DQFR_INSTALL=ON",
                "-DQFR_DEV_EMBED_QTE=OFF",
                "-DQFR_BUILD_TESTS=OFF",
                "-DQFR_BUILD_EXAMPLES=ON",
            ],
        )

    if not args.skip_mps:
        if not (mps / "CMakeLists.txt").is_file():
            raise SystemExit(f"error: missing {mps}")
        configure_build_install(
            source=mps,
            build=mps / "build-shared",
            prefix=prefix,
            qt=qt,
            extra_defs=[
                "-DMPS_BUILD_SHARED=ON",
                "-DMPS_INSTALL=ON",
                "-DMPS_DEV_EMBED_QTE=OFF",
                "-DMPS_DEV_EMBED_QFR=OFF",
                f"-DMPS_BUILD_TESTS={'ON' if args.mps_tests else 'OFF'}",
                "-DMPS_BUILD_DEMOS=ON",
                "-DMPS_BUILD_SRC=ON",
                "-DMPS_FETCH_PROTOBUF=ON",
                "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON",
            ],
        )

    print(f"OK: stack installed to {prefix}", flush=True)
    print(f"  Demo: {mps / 'build-shared' / 'demos' / 'mps_demo_host.exe'}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
