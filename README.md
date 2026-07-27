# MultiProcessShell

[![CI](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml/badge.svg)](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

C++/Qt 多进程壳：**Host 壳框 + Client 原生窗嵌入 + Protobuf IPC**。

首期平台：**Windows（形态 A）**；macOS / Linux 目录占位。

> English overview：[docs/en/README.md](docs/en/README.md)

## 特性

- **多进程 UI 壳**：Host 管理 Tab / 生命周期；Client 以原生 HWND 嵌入（`SetParent`）
- **可撕出 Tab**：拖出 / 合入、MRU 关 Tab、空壳规则（纯规则模块 + 单测）
- **Protobuf IPC**：权威 IDL 为 `proto/shell/ipc/v1/ipc.proto`，Demo 合约见文档
- **`wid` 在 embed seam**：Tab 模型只认 `tabId`；平台句柄由 `EmbedContainer` / `TabEmbedMap` 持有
- **一键 Demo 部署**：Windows 上自动 `windeployqt`，可直接运行 `build-shared/demos/mps_demo_host.exe`

## 要求

| 项 | 说明 |
|----|------|
| Qt | **6.8+**（编 Demo / Host；纯协议单测可不装 Qt） |
| 工具链 | CMake、Ninja、Python 3；Windows 上 MSVC x64（`vcvars`） |
| 依赖 | Protobuf（可由 CMake FetchContent 拉取） |
| 可选 | `clang-format` 20（本地格式检查） |

## 快速开始（Windows）

1. 打开 **x64 Native Tools / vcvars** 环境。  
2. 设置 `QTDIR` 为 Qt **6.8+** 前缀，并把 `%QTDIR%\bin` 加入 `PATH`。  
3. 推荐：安装 QTE→QFR→MPS 到本地 prefix（共享库，构建目录 `build-shared`）：

```bat
:: QTDIR = Qt 6.8+ 前缀；PREFIX = 安装根（可选，默认三仓同级的 prefix/）
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
```

4. 双击运行（无额外控制台）：

```text
build-shared\demos\mps_demo_host.exe
```

仅编本仓（默认也输出到 `build-shared`；Windows 上会自动 `deploy_demo.py` / `windeployqt`）：

```bat
python scripts\build_repo.py
```

仅协议 / Tab 规则单测（可不装 Qt）：

```bat
python scripts\build_repo.py --no-demos --test
```

更细的构建选项见 [docs/zh/build.md](docs/zh/build.md)。CI：[docs/zh/ci.md](docs/zh/ci.md)。

## 仓库布局

```text
cmake/            Qt / Protobuf 辅助
proto/            shell.ipc.v1 IDL
src/              Host / Client / common / ipc_qt
demos/            mps_demo_host / mps_demo_client
tests/            协议单测 + Tab 条规则单测
scripts/          install_stack / build_repo / build_qt / deploy_demo
clients/python/   M4b Python Hello 烟测
docs/zh|en/       中英文文档
```

## 文档

| 主题 | 中文（主） | English |
|------|------------|---------|
| 产品技术规格 | [multiprocess-shell-spec.md](docs/zh/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](docs/en/multiprocess-shell-spec.md) |
| 开发计划 | [dev-plan.md](docs/zh/dev-plan.md) | [dev-plan.md](docs/en/dev-plan.md) |
| Demo 形态 | [demo-morphology.md](docs/zh/demo-morphology.md) | [demo-morphology.md](docs/en/demo-morphology.md) |
| Demo 验收 | [demo-acceptance.md](docs/zh/demo-acceptance.md) | [demo-acceptance.md](docs/en/demo-acceptance.md) |
| Demo IPC | [demo-ipc.md](docs/zh/demo-ipc.md) | [demo-ipc.md](docs/en/demo-ipc.md) |
| IPC 备选（远期） | [ipc-alternatives.md](docs/zh/ipc-alternatives.md) | [ipc-alternatives.md](docs/en/ipc-alternatives.md) |
| 构建 | [build.md](docs/zh/build.md) | [build.md](docs/en/build.md) |
| CI | [ci.md](docs/zh/ci.md) | [ci.md](docs/en/ci.md) |

**约定**：日常以中文文档为准；英文为同步译本。若长文规格草图与 `.proto` 冲突，以 `.proto` / [Demo IPC](docs/zh/demo-ipc.md) 为准。

## 现状

| 能力 | 状态 |
|------|------|
| 拼帧 + `shell.ipc.v1` + `mps_ipc_tests`（M0） | 完成 |
| 可撕出 Tab 规则单测（`mps_tab_strip_tests`） | 完成 |
| Windows Demo（Home / Create Client / 拖出合入 / 嵌入） | 完成（M5 可关账） |
| `wid` 收到 embed seam（Tab 模型 tabId-only） | 完成 |
| M4b Python Hello | 完成 |
| M6 心跳 / 无响应 UI | 完成 |
| 多 Backend、可选 M7 等 | 见规格与 [dev-plan](docs/zh/dev-plan.md) |

## License

Released under the [MIT License](LICENSE).
