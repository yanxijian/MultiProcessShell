# Patch protoc-generated *.pb.h so default_instance_ data symbols use
# dllexport/dllimport with the mps_ipc shared library (MSVC).
# Usage: cmake -P or: python patch_pb_dll_exports.py <ipc.pb.h>

import re
import sys
from pathlib import Path

MARKER = "/* mps_pb_dll_exports */"


def patch(text: str) -> str:
    if MARKER in text:
        return text

    guard = f"""{MARKER}
#if defined(_WIN32) && defined(MPS_IPC_SHARED)
#  ifdef mps_ipc_EXPORTS
#    define MPS_PB_API __declspec(dllexport)
#  else
#    define MPS_PB_API __declspec(dllimport)
#  endif
#else
#  define MPS_PB_API
#endif
"""

    # Insert after the last include guard / first #include block opener.
    # Prefer right after `#define ..._IPC_PB_H` style include guard body start.
    m = re.search(r"(#define\s+\w+_IPC_PB_H\b[^\n]*\n)", text)
    if m:
        text = text[: m.end()] + "\n" + guard + "\n" + text[m.end() :]
    else:
        text = guard + "\n" + text

    # extern ProtoDefaultTypeInternal _Foo_default_instance_;
    text = re.sub(
        r"\bextern\s+(?!MPS_PB_API\b)([\w:]+DefaultTypeInternal\s+_+\w+_default_instance_)",
        r"extern MPS_PB_API \1",
        text,
    )
    return text


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: patch_pb_dll_exports.py <ipc.pb.h>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    original = path.read_text(encoding="utf-8")
    updated = patch(original)
    if updated != original:
        path.write_text(updated, encoding="utf-8", newline="\n")
        print(f"patched {path}")
    else:
        print(f"unchanged {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
