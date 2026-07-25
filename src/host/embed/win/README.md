# Windows 嵌入（形态 A）

> **English**：[../../../../docs/en/embed-win.md](../../../../docs/en/embed-win.md)

`EmbedContainer` 通过 `SetParent` / `SetWindowPos` 托管外部 HWND。Tab 模型只见 `tabId`；`wid` 经 `bind` / `takeBinding` / `transferBinding` / `activate` 留在本 seam（见 `common/tab_embed_map.hpp`）。窗口截图走 `win_capture`。

| 文件 | 作用 |
|------|------|
| `embed_container.hpp` / `.cpp` | `ShellWindow` 使用的原生宿主控件 |

Windows Demo Host 使用本目录。X11 / 同进程后端仍在兄弟目录占位。
