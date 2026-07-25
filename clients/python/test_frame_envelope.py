"""Offline frame / Envelope unit tests (no Host)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

_ROOT = Path(__file__).resolve().parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from mps_ipc.frame import FrameDecoder, PayloadTooLarge, encode_frame
from shell.ipc.v1 import ipc_pb2


class FrameTests(unittest.TestCase):
    def test_round_trip(self) -> None:
        payload = b"hello-payload"
        frame = encode_frame(payload)
        self.assertEqual(frame[:4], (len(payload)).to_bytes(4, "big"))
        dec = FrameDecoder()
        dec.append(frame[:2])
        self.assertIsNone(dec.try_pop())
        dec.append(frame[2:])
        self.assertEqual(dec.try_pop(), payload)

    def test_two_frames(self) -> None:
        a, b = b"aa", b"bbbb"
        blob = encode_frame(a) + encode_frame(b)
        dec = FrameDecoder()
        dec.append(blob)
        self.assertEqual(dec.try_pop(), a)
        self.assertEqual(dec.try_pop(), b)

    def test_oversized(self) -> None:
        with self.assertRaises(PayloadTooLarge):
            encode_frame(b"x" * (16 * 1024 * 1024 + 1))


class EnvelopeTests(unittest.TestCase):
    def test_hello_framed_round_trip(self) -> None:
        env = ipc_pb2.Envelope()
        env.protocol = 1
        env.id = "corr-py"
        env.dir = ipc_pb2.DIR_EVT
        env.ts_ms = 1710000000000
        env.hello.min_protocol = 1
        env.hello.max_protocol = 1
        env.hello.pid = 4242
        env.hello.app_name = "python_m4b"
        env.hello.caps.embed = ipc_pb2.EMBED_NONE

        frame = encode_frame(env.SerializeToString())
        dec = FrameDecoder()
        dec.append(frame)
        payload = dec.try_pop()
        assert payload is not None
        parsed = ipc_pb2.Envelope()
        self.assertTrue(parsed.ParseFromString(payload))
        self.assertTrue(parsed.HasField("hello"))
        self.assertEqual(parsed.hello.app_name, "python_m4b")
        self.assertEqual(parsed.hello.caps.embed, ipc_pb2.EMBED_NONE)

    def test_hello_ack_parse(self) -> None:
        env = ipc_pb2.Envelope()
        env.protocol = 1
        env.id = "ack-1"
        env.dir = ipc_pb2.DIR_EVT
        env.hello_ack.protocol = 1
        env.hello_ack.session_id = "7"
        env.hello_ack.host_caps.embed = ipc_pb2.EMBED_HWND
        raw = env.SerializeToString()
        again = ipc_pb2.Envelope()
        self.assertTrue(again.ParseFromString(raw))
        self.assertEqual(again.hello_ack.session_id, "7")


if __name__ == "__main__":
    unittest.main()
