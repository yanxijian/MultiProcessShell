# src/

C++/Qt implementation (Host, Client, common, embed backends).

**Status:** placeholder only — no implementation until Demo morphology is agreed  
(see `docs/zh/demo-morphology.md` / `docs/en/demo-morphology.md`).

## Planned layout

| Path | Role |
|------|------|
| `common/` | Framing, IDs, shared types |
| `embed_helper/` | Optional C ABI for multi-language embed |
| `host/` | Shell process |
| `host/embed/win/` | Windows `SetParent` backend (phase-1) |
| `host/embed/x11/` | Linux/X11 placeholder |
| `host/embed/inproc/` | macOS in-process placeholder |
| `client/runtime/` | Client IPC + embed hooks |
| `client/alpha`, `client/beta` | Sample business processes (later) |

Enable with CMake `-DMPS_BUILD_SRC=ON` once targets exist.
