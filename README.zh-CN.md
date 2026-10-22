# MultiProcessShell

**简体中文** · [English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

[![CI](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml/badge.svg)](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6.8%2B-green.svg)](https://www.qt.io/)

<!-- translated-from: README.md sha256:45a7ef36d758 -->


C++/Qt 多进程壳：**Host 壳框 + Client 原生窗嵌入 + Protobuf IPC**。

首期平台：**Windows（形态 A）**；macOS / Linux 目录占位。

## 特性

- **多进程 UI 壳**：Host 管理 Tab 与生命周期；Client 以原生 HWND 嵌入（`SetParent`）
- **可撕出 Tab**：拖出 / 合入、MRU 关 Tab、空壳规则（纯规则模块 + 单测）
- **Protobuf IPC**：权威 IDL 为 `proto/shell/ipc/v1/ipc.proto`，Demo 合约见文档
- **`wid` 藏在嵌入接缝之后**：Tab 模型只认 `tabId`；平台句柄由 `EmbedContainer` / `TabEmbedMap` 持有
- **一键 Demo 部署**：Windows 上自动执行 `windeployqt`，`build-shared/demos/mps_demo_host.exe` 可直接运行

## 要求

| 项 | 说明 |
|----|------|
| Qt | **6.8+**（编 Demo / Host；纯协议单测可不装 Qt） |
| 工具链 | CMake、Ninja、Python 3；Windows 上 MSVC x64（`vcvars`） |
| 依赖 | Protobuf **v35.1**（FetchContent）；推荐 [AbseilPin](https://github.com/yanxijian/AbseilPin) `20260107.1`（`-DMPS_ABSEIL_PIN_PREFIX=`，旁路可自动探测） |
| 可选 | `clang-format` 20（本地格式检查） |

## 快速开始（Windows）

1. 打开 **x64 Native Tools / vcvars** 环境。
2. 设置 `QTDIR` 为 Qt **6.8+** 前缀，并把 `%QTDIR%\bin` 加入 `PATH`。
3. 推荐：把 [QThemeEngine](https://github.com/yanxijian/QThemeEngine)（QTE）→ [QFluentRibbon](https://github.com/yanxijian/QFluentRibbon)（QFR）→ MPS 依次装进本地 prefix（共享库，构建目录 `build-shared`）：

```bat
:: QTDIR = Qt 6.8+ 前缀；PREFIX = 安装根（可选，默认三仓同级的 prefix/）
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
```

4. 运行（不弹额外控制台）：

```text
build-shared\demos\mps_demo_host.exe
```

只编本仓（默认也输出到 `build-shared`；Windows 上会自动跑 `deploy_demo.py` / `windeployqt`）：

```bat
python scripts\build_repo.py
```

只跑协议 / Tab 规则单测（可不装 Qt）：

```bat
python scripts\build_repo.py --no-demos --test
```

更细的构建选项见 [build.md](docs/zh/build.md)；CI 见 [ci.md](docs/zh/ci.md)。

## 仓库布局

```text
cmake/            Qt / Protobuf 辅助
proto/            shell.ipc.v1 IDL
src/              Host / Client / common / ipc_qt
demos/            mps_demo_host / mps_demo_client
tests/            协议与 Tab 条规则单测
scripts/          install_stack / build_repo / build_qt / deploy_demo
clients/python/   M4b Python Hello 冒烟
docs/zh|en/       中文与英文文档
```

## 文档

| 主题 | English | 简体中文 |
|------|---------|----------|
| 产品方案 | [multiprocess-shell-spec.md](docs/en/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](docs/zh/multiprocess-shell-spec.md) |
| 开发计划 | [dev-plan.md](docs/en/dev-plan.md) | [dev-plan.md](docs/zh/dev-plan.md) |
| Demo 形态 | [demo-morphology.md](docs/en/demo-morphology.md) | [demo-morphology.md](docs/zh/demo-morphology.md) |
| Demo 验收 | [demo-acceptance.md](docs/en/demo-acceptance.md) | [demo-acceptance.md](docs/zh/demo-acceptance.md) |
| Demo IPC | [demo-ipc.md](docs/en/demo-ipc.md) | [demo-ipc.md](docs/zh/demo-ipc.md) |
| IPC 备选 | [ipc-alternatives.md](docs/en/ipc-alternatives.md) | [ipc-alternatives.md](docs/zh/ipc-alternatives.md) |
| 构建 | [build.md](docs/en/build.md) | [build.md](docs/zh/build.md) |
| CI | [ci.md](docs/en/ci.md) | [ci.md](docs/zh/ci.md) |

长文设计文档目前只有英文与简体中文两版；这份 README 是四种语言之间保持同步的那份。

**取舍原则**：长文产品描述与 `.proto` 冲突时，以 `.proto` 与 Demo IPC 为准。

## 现状

| 能力 | 状态 |
|------|------|
| 帧格式 + `shell.ipc.v1` + `mps_ipc_tests`（M0） | 完成 |
| 可撕出 Tab 条规则单测 | 完成 |
| Windows Demo（主页 / 新建 Client / 撕出 / 嵌入） | 完成（M5 可收尾） |
| `wid` 藏在嵌入接缝之后 | 完成 |
| M4b Python Hello | 完成 |
| M6 心跳 / 界面无响应 | 完成 |
| 多后端 / 可选的 M7 | 见方案与 [dev-plan](docs/zh/dev-plan.md) |

## License

Released under the [MIT License](LICENSE)。
