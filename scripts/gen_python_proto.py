#!/usr/bin/env python3
"""Regenerate clients/python/shell/ipc/v1/ipc_pb2.py from proto/shell/ipc/v1/ipc.proto."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    proto = repo / "proto" / "shell" / "ipc" / "v1" / "ipc.proto"
    out = repo / "clients" / "python"
    parser = argparse.ArgumentParser()
    parser.add_argument("--protoc", default=None, help="Path to protoc (else PATH / common build dirs)")
    args = parser.parse_args()

    protoc = args.protoc
    if not protoc:
        protoc = shutil.which("protoc")
    if not protoc:
        for candidate in (repo / "build-x64-check" / "_deps").rglob("protoc.exe"):
            protoc = str(candidate)
            break
        if not protoc:
            for candidate in (repo / "build" / "_deps").rglob("protoc*"):
                if candidate.is_file() and "protoc" in candidate.name:
                    protoc = str(candidate)
                    break
    if not protoc:
        print("protoc not found; pass --protoc", file=sys.stderr)
        return 1

    cmd = [
        protoc,
        f"--python_out={out}",
        f"-I{repo / 'proto'}",
        str(proto),
    ]
    print(" ".join(cmd))
    subprocess.check_call(cmd)
    print(f"Wrote {out / 'shell' / 'ipc' / 'v1' / 'ipc_pb2.py'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
