#!/usr/bin/env python3
"""M4b smoke: connect to Host QLocalServer, send Hello (EMBED_NONE), wait for HelloAck."""

from __future__ import annotations

import argparse
import os
import sys
import time
import uuid
from pathlib import Path

# Allow `python hello_client.py` from this directory without installing a package.
_ROOT = Path(__file__).resolve().parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from mps_ipc.frame import FrameDecoder, encode_frame  # noqa: E402
from mps_ipc.local_pipe import connect_local  # noqa: E402
from shell.ipc.v1 import ipc_pb2  # noqa: E402


def _read_exact(stream, n: int) -> bytes:
    chunks: list[bytes] = []
    got = 0
    while got < n:
        part = stream.read(n - got)
        if not part:
            raise ConnectionError("peer closed while reading")
        chunks.append(part)
        got += len(part)
    return b"".join(chunks)


def _recv_envelope(stream, decoder: FrameDecoder, deadline: float) -> ipc_pb2.Envelope:
    while True:
        if time.monotonic() > deadline:
            raise TimeoutError("timed out waiting for Envelope")
        payload = decoder.try_pop()
        if payload is not None:
            env = ipc_pb2.Envelope()
            if not env.ParseFromString(payload):
                raise ValueError("failed to parse Envelope")
            return env
        # Read at least header; more if peer wrote a full frame.
        chunk = stream.read(4096)
        if not chunk:
            time.sleep(0.01)
            continue
        decoder.append(chunk)


def build_hello(app_name: str) -> ipc_pb2.Envelope:
    env = ipc_pb2.Envelope()
    env.protocol = 1
    env.id = str(uuid.uuid4())
    env.dir = ipc_pb2.DIR_EVT
    env.ts_ms = int(time.time() * 1000)
    hello = env.hello
    hello.min_protocol = 1
    hello.max_protocol = 1
    hello.pid = os.getpid() & 0xFFFFFFFF
    hello.app_name = app_name
    hello.caps.embed = ipc_pb2.EMBED_NONE
    hello.caps.heartbeat = False
    hello.caps.invoke = False
    return env


def run(endpoint: str, server_path: str | None, timeout_s: float, app_name: str) -> int:
    stream = connect_local(endpoint, server_path)
    decoder = FrameDecoder()
    try:
        hello = build_hello(app_name)
        stream.write(encode_frame(hello.SerializeToString()))
        deadline = time.monotonic() + timeout_s
        while True:
            env = _recv_envelope(stream, decoder, deadline)
            if env.HasField("hello_ack"):
                ack = env.hello_ack
                print(
                    f"HelloAck ok protocol={ack.protocol} session_id={ack.session_id!r} "
                    f"host_embed={ack.host_caps.embed}",
                    flush=True,
                )
                return 0
            # Host Demo may send CreateSubWindow after Ack; ignore and keep waiting if Ack not yet seen.
            if env.HasField("create_sub_window"):
                continue
    finally:
        stream.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="M4b Python Hello / HelloAck smoke client")
    parser.add_argument("--endpoint", required=True, help="QLocalServer listen name")
    parser.add_argument(
        "--server-path",
        default=None,
        help="QLocalServer::fullServerName (preferred when set by Host / smoke harness)",
    )
    parser.add_argument("--timeout", type=float, default=10.0, help="seconds to wait for HelloAck")
    parser.add_argument("--app-name", default="python_m4b", help="Hello.app_name")
    args = parser.parse_args(argv)
    try:
        return run(args.endpoint, args.server_path, args.timeout, args.app_name)
    except Exception as exc:  # noqa: BLE001 — smoke CLI surfaces any failure as non-zero
        print(f"m4b hello failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
