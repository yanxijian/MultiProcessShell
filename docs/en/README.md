# MultiProcessShell

[![CI](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml/badge.svg)](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)

C++/Qt multi-process shell: **Host chrome + Client native-window embed + Protobuf IPC**.

Phase-1 platform: **Windows (form A)**; macOS / Linux directories are placeholders.

> Canonical docs are Chinese — start at the [root README](../../README.md).

## Features

- **Multi-process UI shell**: Host owns tabs / lifecycle; Clients embed via native HWND (`SetParent`)
- **Detachable tabs**: tear-out / merge, close-tab MRU, empty-shell rules (pure rule modules + tests)
- **Protobuf IPC**: authoritative IDL at `proto/shell/ipc/v1/ipc.proto`
- **`wid` behind the embed seam**: tab model is `tabId`-only; platform handles live in `EmbedContainer` / `TabEmbedMap`
- **One-click Demo deploy**: Windows `windeployqt` beside `build/demos`

## Requirements

| Item | Notes |
|------|--------|
| Qt | **6.8+** for Demo / Host (protocol-only tests can skip Qt) |
| Toolchain | CMake, Ninja, Python 3; MSVC x64 (`vcvars`) on Windows |
| Deps | Protobuf (optionally via CMake FetchContent) |
| Optional | `clang-format` 20 for local format checks |

## Quick start (Windows)

```bat
python scripts\build_repo.py
build\demos\mps_demo_host.exe
```

Protocol / tab-strip tests only (Qt optional):

```bat
python scripts\build_repo.py --no-demos --test
```

See [build.md](build.md) and [ci.md](ci.md).

## Documentation

| Topic | 中文 | English |
|-------|------|---------|
| Product spec | [../zh/multiprocess-shell-spec.md](../zh/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](multiprocess-shell-spec.md) |
| Dev plan | [../zh/dev-plan.md](../zh/dev-plan.md) | [dev-plan.md](dev-plan.md) |
| M5 gap audit | [../zh/m5-gap-audit.md](../zh/m5-gap-audit.md) | [m5-gap-audit.md](m5-gap-audit.md) |
| Demo morphology | [../zh/demo-morphology.md](../zh/demo-morphology.md) | [demo-morphology.md](demo-morphology.md) |
| Demo acceptance | [../zh/demo-acceptance.md](../zh/demo-acceptance.md) | [demo-acceptance.md](demo-acceptance.md) |
| Demo IPC | [../zh/demo-ipc.md](../zh/demo-ipc.md) | [demo-ipc.md](demo-ipc.md) |
| IPC alternatives | [../zh/ipc-alternatives.md](../zh/ipc-alternatives.md) | [ipc-alternatives.md](ipc-alternatives.md) |
| Build | [../zh/build.md](../zh/build.md) | [build.md](build.md) |
| CI | [../zh/ci.md](../zh/ci.md) | [ci.md](ci.md) |

**Policy:** Prefer Chinese docs day-to-day. If the long-form product sketch disagrees with `.proto`, prefer `.proto` / Demo IPC.

## Status

| Capability | Status |
|------------|--------|
| Framing + `shell.ipc.v1` + `mps_ipc_tests` (M0) | Done |
| Detachable tab-strip rule tests | Done |
| Windows Demo (Home / Create Client / tear-out / embed) | Done |
| `wid` behind embed seam | Done |
| M4b Python Hello | Done |
| M5 tear-out / merge | Closable — [gap audit](../zh/m5-gap-audit.md) |
| M6 heartbeat / unresponsive UI | Done |
| Multi-backend / optional M7 | See spec + [dev-plan](dev-plan.md) |

## License

Released under the [MIT License](../../LICENSE).
