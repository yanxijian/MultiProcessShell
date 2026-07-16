# src/

| Path | Status |
|------|--------|
| `common/` | Framing + generated `shell.ipc.v1` (`mps::ipc`) |
| `ipc_qt/` | `EnvelopeChannel` over `QIODevice` / `QLocalSocket` |
| `host/` | Shell UI, tab model, client sessions, Win `EmbedContainer` |
| `client/` | Demo Client process glue (`ClientApp` / pages) |

CMake: built when `MPS_BUILD_SRC` or `MPS_BUILD_DEMOS` is ON (both default ON; `--no-demos` turns both off).

Platform embed notes: `host/embed/win`, `x11`, `inproc`.
