#!/usr/bin/env python3
"""Deploy Qt runtime next to built demo executables (Windows)."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=ROOT / "build",
        help="CMake build directory (default: ./build)",
    )
    parser.add_argument(
        "--config",
        default="Release",
        help="MSVC multi-config folder name if present (default: Release)",
    )
    args = parser.parse_args()

    qtdir = os.environ.get("QTDIR")
    if not qtdir:
        raise SystemExit("error: QTDIR is not set")

    windeployqt = Path(qtdir) / "bin" / "windeployqt.exe"
    if not windeployqt.is_file():
        windeployqt = Path(shutil.which("windeployqt") or "")
    if not windeployqt.is_file():
        raise SystemExit("error: windeployqt.exe not found")

    candidates = [
        args.build_dir / "demos" / "mps_demo_host.exe",
        args.build_dir / "demos" / args.config / "mps_demo_host.exe",
    ]
    host = next((p for p in candidates if p.is_file()), None)
    if not host:
        raise SystemExit(f"error: mps_demo_host.exe not found under {args.build_dir}/demos")

    demo_dir = host.parent
    client = demo_dir / "mps_demo_client.exe"
    if not client.is_file():
        # try build tree client then copy
        alt = [
            args.build_dir / "demos" / "mps_demo_client.exe",
            args.build_dir / "demos" / args.config / "mps_demo_client.exe",
        ]
        src = next((p for p in alt if p.is_file()), None)
        if not src:
            raise SystemExit("error: mps_demo_client.exe not found")
        shutil.copy2(src, client)

    def deploy(exe: Path) -> None:
        cmd = [
            str(windeployqt),
            "--release",
            "--no-translations",
            "--compiler-runtime",
            str(exe),
        ]
        print("+", " ".join(cmd), flush=True)
        subprocess.check_call(cmd)

    deploy(host)
    deploy(client)

    # Convenience copy for double-click outside the build tree.
    dist = ROOT / "dist" / "Demo"
    dist.mkdir(parents=True, exist_ok=True)
    # Copy entire demo_dir contents (exe + Qt deps)
    for item in demo_dir.iterdir():
        dest = dist / item.name
        if item.is_file():
            shutil.copy2(item, dest)
        elif item.is_dir():
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(item, dest)

    print(f"OK: deployed. Double-click:\n  {dist / 'mps_demo_host.exe'}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
