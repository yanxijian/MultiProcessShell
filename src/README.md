# src/

> **English**：[../docs/en/src.md](../docs/en/src.md)

| 路径 | 状态 |
|------|------|
| `common/` | 拼帧 + 生成的 `shell.ipc.v1`（`mps::ipc`） |
| `ipc_qt/` | 基于 `QIODevice` / `QLocalSocket` 的 `EnvelopeChannel` |
| `host/` | 壳 UI、Tab 模型、Client 会话、Win `EmbedContainer` |
| `client/` | Demo Client 进程（`ClientApp` / page） |

CMake：`MPS_BUILD_SRC` 或 `MPS_BUILD_DEMOS` 为 ON 时编译（二者默认 ON；`--no-demos` 会一起关掉）。

平台嵌入说明：`host/embed/win`、`x11`、`inproc`。
