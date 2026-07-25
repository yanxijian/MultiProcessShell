"""M4b helpers: frame codec + local-socket connect to Qt QLocalServer."""

from .frame import FrameDecoder, encode_frame
from .local_pipe import connect_local

__all__ = ["FrameDecoder", "encode_frame", "connect_local"]
