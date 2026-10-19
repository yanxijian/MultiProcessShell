# MultiProcessShell

**English** · [简体中文](README.zh-CN.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

[![CI](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml/badge.svg)](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

C++/Qt multi-process shell: **Host chrome + Client native-window embed + Protobuf IPC**.

Phase-1 platform: **Windows (form A)**; the macOS / Linux directories are placeholders.

## Features

- **Multi-process UI shell**: the Host owns tabs and lifecycle; Clients embed as native HWNDs (`SetParent`)
- **Detachable tabs**: tear-out / merge, MRU tab closing, empty-shell rules (pure rule modules with tests)
- **Protobuf IPC**: the authoritative IDL is `proto/shell/ipc/v1/ipc.proto`
- **`wid` stays behind the embed seam**: the tab model knows only `tabId`; platform handles live in `EmbedContainer` / `TabEmbedMap`
- **One-click demo deploy**: `windeployqt` runs automatically on Windows, so `build-shared/demos/mps_demo_host.exe` just works

## Requirements

| Item | Notes |
|------|--------|
| Qt | **6.8+** to build the demo / Host (protocol-only tests can skip Qt) |
| Toolchain | CMake, Ninja, Python 3; MSVC x64 (`vcvars`) on Windows |
| Deps | Protobuf **v35.1** (FetchContent); the recommended [AbseilPin](https://github.com/yanxijian/AbseilPin) `20260107.1` (`-DMPS_ABSEIL_PIN_PREFIX=`, detected in a sibling directory) |
| Optional | `clang-format` 20 for local format checks |

## Quick start (Windows)

1. Open an **x64 Native Tools / vcvars** environment.
2. Point `QTDIR` at a Qt **6.8+** prefix and add `%QTDIR%\bin` to `PATH`.
3. Recommended: install [QThemeEngine](https://github.com/yanxijian/QThemeEngine) (QTE) → [QFluentRibbon](https://github.com/yanxijian/QFluentRibbon) (QFR) → MPS into a local prefix (shared libraries, build tree `build-shared`):

```bat
:: QTDIR = Qt 6.8+ prefix; PREFIX = install root (optional: sibling prefix/ of the three repos)
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
```

4. Run it (no extra console window):

```text
build-shared\demos\mps_demo_host.exe
```

This repo only (also defaults to `build-shared`; Windows runs `deploy_demo.py` / `windeployqt`):

```bat
python scripts\build_repo.py
```

Protocol / tab-strip tests only (Qt optional):

```bat
python scripts\build_repo.py --no-demos --test
```

Finer build options: [build.md](docs/en/build.md); CI: [ci.md](docs/en/ci.md).

## Layout

```text
cmake/            Qt / Protobuf helpers
proto/            shell.ipc.v1 IDL
src/              Host / Client / common / ipc_qt
demos/            mps_demo_host / mps_demo_client
tests/            protocol + tab-strip unit tests
scripts/          install_stack / build_repo / build_qt / deploy_demo
clients/python/   M4b Python Hello smoke
docs/zh|en/       Chinese + English docs
```

## Documentation

| Topic | English | 简体中文 |
|-------|---------|----------|
| Product spec | [multiprocess-shell-spec.md](docs/en/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](docs/zh/multiprocess-shell-spec.md) |
| Dev plan | [dev-plan.md](docs/en/dev-plan.md) | [dev-plan.md](docs/zh/dev-plan.md) |
| Demo morphology | [demo-morphology.md](docs/en/demo-morphology.md) | [demo-morphology.md](docs/zh/demo-morphology.md) |
| Demo acceptance | [demo-acceptance.md](docs/en/demo-acceptance.md) | [demo-acceptance.md](docs/zh/demo-acceptance.md) |
| Demo IPC | [demo-ipc.md](docs/en/demo-ipc.md) | [demo-ipc.md](docs/zh/demo-ipc.md) |
| IPC alternatives | [ipc-alternatives.md](docs/en/ipc-alternatives.md) | [ipc-alternatives.md](docs/zh/ipc-alternatives.md) |
| Build | [build.md](docs/en/build.md) | [build.md](docs/zh/build.md) |
| CI | [ci.md](docs/en/ci.md) | [ci.md](docs/zh/ci.md) |

Long-form design notes are maintained in English and Simplified Chinese only; this README is the document kept in sync across all four languages.

**Policy:** when the long-form product sketch disagrees with `.proto`, the `.proto` and the demo IPC win.

## Status

| Capability | Status |
|------------|--------|
| Framing + `shell.ipc.v1` + `mps_ipc_tests` (M0) | Done |
| Detachable tab-strip rule tests | Done |
| Windows demo (Home / Create Client / tear-out / embed) | Done (M5 closable) |
| `wid` behind the embed seam | Done |
| M4b Python Hello | Done |
| M6 heartbeat / unresponsive UI | Done |
| Multi-backend / optional M7 | See spec + [dev-plan](docs/en/dev-plan.md) |

## License

Released under the [MIT License](LICENSE).
