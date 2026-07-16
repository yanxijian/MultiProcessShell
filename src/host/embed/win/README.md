# Windows 嵌入（形态 A）

> **English**：[../../../../docs/en/embed-win.md](../../../../docs/en/embed-win.md)

`EmbedContainer` 通过 `SetParent` / `SetWindowPos` 托管外部 HWND。

| 文件 | 作用 |
|------|------|
| `embed_container.hpp` / `.cpp` | `ShellWindow` 使用的原生宿主控件 |

Windows Demo Host 使用本目录。X11 / 同进程后端仍在兄弟目录占位。
