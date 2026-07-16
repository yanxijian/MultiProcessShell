# MultiProcessShell

> **中文主文档**: [`../../README.md`](../../README.md)

MIT-licensed C++/Qt multi-process shell: Host chrome + Client native-window embed + Protobuf IPC.

Phase-1 platform: **Windows (form A)**; macOS / Linux directories are placeholders.

## Documentation

| Topic | 中文主文档 | English |
|-------|-------------|---------|
| Product spec | [../zh/multiprocess-shell-spec.md](../zh/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](multiprocess-shell-spec.md) |
| Demo morphology | [../zh/demo-morphology.md](../zh/demo-morphology.md) | [demo-morphology.md](demo-morphology.md) |
| Demo IPC | [../zh/demo-ipc.md](../zh/demo-ipc.md) | [demo-ipc.md](demo-ipc.md) |
| Build | [../zh/build.md](../zh/build.md) | [build.md](build.md) |
| demos / scripts / src / … | folder `README.md`（中文） | [demos.md](demos.md), [scripts.md](scripts.md), [src.md](src.md), … |

**Policy:** Prefer Chinese docs day-to-day; English is a synced mirror. Demo IDL authority is `proto/shell/ipc/v1/ipc.proto` + Demo IPC. If the long-form product sketch disagrees with `.proto`, prefer `.proto` / Demo IPC.

## Layout

```text
MultiProcessShell/
  cmake/           Qt / Protobuf helpers
  proto/           shell.ipc.v1 IDL
  src/             Host / Client / common / ipc_qt
  demos/           Demo (mps_demo_host / mps_demo_client)
  tests/           M0 protocol tests
  clients/python/  M4b smoke (later)
  docs/zh|en/      Chinese + English docs
  scripts/         build_repo / build_qt / deploy_demo
  dist/Demo/       Windows double-click bundle (generated; gitignored)
```

## Quick start (Windows)

1. Open an **x64 Native Tools / vcvars** shell.  
2. Set `QTDIR` to Qt **6.8+**; put `%QTDIR%\bin` on `PATH`.  
3. Build and deploy:

```bat
python scripts\build_repo.py
```

4. Double-click (no extra console):

```text
dist\Demo\mps_demo_host.exe
```

Default build includes Host/Client Demo; on Windows `deploy_demo.py` runs automatically (`windeployqt`).

M0 tests only (Qt optional):

```bat
python scripts\build_repo.py --no-demos --test
```

## Status

- Specs, Demo morphology/IPC, and scripts are in-tree.  
- **M0 done**: framing + `shell.ipc.v1` + `mps_ipc_tests`.  
- **Windows Demo done**: Home tab, Create Client, same-Client New Window, close-tab MRU history, tear-out/merge hooks, `SetParent` embed, GUI subsystem (no console).

## English mirror index

| File | Mirrors |
|------|---------|
| [build.md](build.md) | [../zh/build.md](../zh/build.md) |
| [demo-morphology.md](demo-morphology.md) | [../zh/demo-morphology.md](../zh/demo-morphology.md) |
| [demo-ipc.md](demo-ipc.md) | [../zh/demo-ipc.md](../zh/demo-ipc.md) |
| [multiprocess-shell-spec.md](multiprocess-shell-spec.md) | [../zh/multiprocess-shell-spec.md](../zh/multiprocess-shell-spec.md) (summary) |
| [demos.md](demos.md) | [../../demos/README.md](../../demos/README.md) |
| [scripts.md](scripts.md) | [../../scripts/README.md](../../scripts/README.md) |
| [src.md](src.md) | [../../src/README.md](../../src/README.md) |
| [tests.md](tests.md) | [../../tests/README.md](../../tests/README.md) |
| [proto-ipc.md](proto-ipc.md) | [../../proto/shell/ipc/v1/README.md](../../proto/shell/ipc/v1/README.md) |
| [clients-python.md](clients-python.md) | [../../clients/python/README.md](../../clients/python/README.md) |
| [embed-win.md](embed-win.md) | [../../src/host/embed/win/README.md](../../src/host/embed/win/README.md) |
| [embed-x11.md](embed-x11.md) | [../../src/host/embed/x11/README.md](../../src/host/embed/x11/README.md) |
| [embed-inproc.md](embed-inproc.md) | [../../src/host/embed/inproc/README.md](../../src/host/embed/inproc/README.md) |

## License

[MIT](../../LICENSE)
