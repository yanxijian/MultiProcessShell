# Patch protoc-generated *.pb.h so message globals / default_instance_ data
# symbols use dllexport/dllimport with the mps_ipc shared library (MSVC).
#
# protobuf ≤29:  extern FooDefaultTypeInternal _Foo_default_instance_;
# protobuf ≥35:  extern FooGlobalsTypeInternal Foo_globals_;
#                (+ optional Foo_class_data_)
#
# Usage: python patch_pb_dll_exports.py <ipc.pb.h>

import re
import sys
from pathlib import Path

MARKER = "/* mps_pb_dll_exports */"

_GUARD = f"""{MARKER}
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

# Any extern declaration of protobuf message global / class_data symbols.
_EXTERN_DATA = re.compile(
    r"\bextern\s+(?!MPS_PB_API\b)"
    r"("
    r"(?:const\s+)?"
    r"(?:"
    r"[\w:]+DefaultTypeInternal\s+_+\w+_default_instance_"
    r"|[\w:]+GlobalsTypeInternal\s+\w+_globals_"
    r"|::google::protobuf::internal::ClassDataFull\s+\w+_class_data_"
    r")"
    r")"
)


def patch(text: str) -> str:
    if MARKER not in text:
        m = re.search(r"(#define\s+\w+_IPC_PB_H\b[^\n]*\n)", text)
        if m:
            text = text[: m.end()] + "\n" + _GUARD + "\n" + text[m.end() :]
        else:
            text = _GUARD + "\n" + text

    text = _EXTERN_DATA.sub(r"extern MPS_PB_API \1", text)
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
