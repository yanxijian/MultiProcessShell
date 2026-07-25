# Length-prefixed framing (uint32 BE + payload), matching src/common/frame.cpp.

from __future__ import annotations

MAX_FRAME_PAYLOAD_BYTES = 16 * 1024 * 1024


class FrameError(Exception):
    pass


class PayloadTooLarge(FrameError):
    pass


class Incomplete(FrameError):
    pass


def encode_frame(payload: bytes) -> bytes:
    if len(payload) > MAX_FRAME_PAYLOAD_BYTES:
        raise PayloadTooLarge(f"payload {len(payload)} exceeds {MAX_FRAME_PAYLOAD_BYTES}")
    return len(payload).to_bytes(4, "big") + payload


class FrameDecoder:
    def __init__(self) -> None:
        self._buf = bytearray()
        self._failed = False

    def reset(self) -> None:
        self._buf.clear()
        self._failed = False

    @property
    def failed(self) -> bool:
        return self._failed

    def append(self, data: bytes) -> None:
        if self._failed or not data:
            return
        self._buf.extend(data)

    def try_pop(self) -> bytes | None:
        """Return next payload, None if incomplete, or raise PayloadTooLarge."""
        if self._failed:
            raise PayloadTooLarge("decoder failed")
        if len(self._buf) < 4:
            return None
        length = int.from_bytes(self._buf[:4], "big")
        if length > MAX_FRAME_PAYLOAD_BYTES:
            self._failed = True
            self._buf.clear()
            raise PayloadTooLarge(f"declared length {length}")
        need = 4 + length
        if len(self._buf) < need:
            return None
        payload = bytes(self._buf[4:need])
        del self._buf[:need]
        return payload
