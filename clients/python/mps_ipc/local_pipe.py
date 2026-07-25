# Connect to a Qt QLocalServer endpoint (Windows named pipe / Unix domain socket).

from __future__ import annotations

import socket
import sys
from typing import Protocol


class ByteStream(Protocol):
    def read(self, n: int) -> bytes: ...
    def write(self, data: bytes) -> None: ...
    def close(self) -> None: ...


class _SocketStream:
    def __init__(self, sock: socket.socket) -> None:
        self._sock = sock

    def read(self, n: int) -> bytes:
        chunks: list[bytes] = []
        got = 0
        while got < n:
            part = self._sock.recv(n - got)
            if not part:
                break
            chunks.append(part)
            got += len(part)
        return b"".join(chunks)

    def write(self, data: bytes) -> None:
        self._sock.sendall(data)

    def close(self) -> None:
        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self._sock.close()


class _WinPipeStream:
    def __init__(self, handle: int) -> None:
        import ctypes
        from ctypes import wintypes

        self._k32 = ctypes.windll.kernel32
        self._handle = handle
        self._DWORD = wintypes.DWORD

    def read(self, n: int) -> bytes:
        import ctypes

        buf = ctypes.create_string_buffer(n)
        read = self._DWORD(0)
        ok = self._k32.ReadFile(self._handle, buf, n, ctypes.byref(read), None)
        if not ok:
            raise OSError(f"ReadFile failed: {self._k32.GetLastError()}")
        return buf.raw[: read.value]

    def write(self, data: bytes) -> None:
        import ctypes

        written = self._DWORD(0)
        ok = self._k32.WriteFile(self._handle, data, len(data), ctypes.byref(written), None)
        if not ok or written.value != len(data):
            raise OSError(f"WriteFile failed: {self._k32.GetLastError()}")

    def close(self) -> None:
        self._k32.CloseHandle(self._handle)


def connect_local(endpoint: str, server_path: str | None = None) -> ByteStream:
    """
    Connect to QLocalServer.

    Prefer --server-path (QLocalServer::fullServerName). Else use --endpoint
    (listen name): Windows \\\\.\\pipe\\<name>, Unix /tmp/<name> fallback.
    """
    if sys.platform == "win32":
        path = server_path or endpoint
        if not path.startswith("\\\\"):
            path = rf"\\.\pipe\{path}"
        return _connect_win_pipe(path)

    path = server_path or endpoint
    if not path.startswith("/") and not path.startswith("\0"):
        # Common Qt Unix layout when only the listen name is known.
        candidates = [path, f"/tmp/{path}"]
    else:
        candidates = [path]

    last_err: OSError | None = None
    for candidate in candidates:
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(candidate)
            return _SocketStream(sock)
        except OSError as exc:
            last_err = exc
    assert last_err is not None
    raise last_err


def _connect_win_pipe(path: str) -> _WinPipeStream:
    import ctypes
    from ctypes import wintypes

    k32 = ctypes.windll.kernel32
    GENERIC_READ = 0x80000000
    GENERIC_WRITE = 0x40000000
    OPEN_EXISTING = 3
    INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value

    handle = k32.CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None)
    if handle in (0, INVALID_HANDLE_VALUE, -1):
        raise OSError(f"CreateFileW({path!r}) failed: {k32.GetLastError()}")
    return _WinPipeStream(int(handle))
