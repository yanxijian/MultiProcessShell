# Windows embed (form A)

> **中文主文档**: [`../../src/host/embed/win/README.md`](../../src/host/embed/win/README.md)

`EmbedContainer` hosts a foreign HWND via `SetParent` / `SetWindowPos`.

| File | Role |
|------|------|
| `embed_container.hpp` / `.cpp` | Native host widget used by `ShellWindow` |

Used by the Demo Host on Windows. X11 / in-proc backends remain placeholders under sibling dirs.
