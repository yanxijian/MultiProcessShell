# src/

> **中文主文档**: [`../src/README.md`](../src/README.md)

| Path | Status |
|------|--------|
| `common/` | Framing + generated `shell.ipc.v1` (`mps::ipc`) |
| `ipc_qt/` | `EnvelopeChannel` over `QIODevice` / `QLocalSocket` |
| `host/` | Shell UI, tab model, client sessions, Win `EmbedContainer`, tear-out preview (`tear_out_preview`) |
| `client/` | Framework `ClientApp` / `ContentView` (demo UI lives under `demos/demo_client`) |

CMake: built when `MPS_BUILD_SRC` or `MPS_BUILD_DEMOS` is ON (both default ON; `--no-demos` turns both off).

Platform embed notes: `host/embed/win`, `x11`, `inproc`.

**Terminology:** Docs may say **EmbedSlot** for the per-shell embed seat; the code has one `EmbedContainer` per `ShellWindow`, multiplexing ContentView/Tab HWNDs.
