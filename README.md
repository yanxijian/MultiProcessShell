# MultiProcessShell

MIT-licensed C++/Qt multi-process shell: Host chrome + Client native-window embed + Protobuf IPC.

首期平台：**Windows（形态 A）**；macOS / Linux 目录占位。

## Documentation

| Doc | 中文 | English |
|-----|------|---------|
| Technical spec (authoritative product vision) | [`docs/zh/multiprocess-shell-spec.md`](docs/zh/multiprocess-shell-spec.md) | [`docs/en/multiprocess-shell-spec.md`](docs/en/multiprocess-shell-spec.md) |
| Demo morphology (final) | [`docs/zh/demo-morphology.md`](docs/zh/demo-morphology.md) | [`docs/en/demo-morphology.md`](docs/en/demo-morphology.md) |
| Demo IPC contract (final) | [`docs/zh/demo-ipc.md`](docs/zh/demo-ipc.md) | [`docs/en/demo-ipc.md`](docs/en/demo-ipc.md) |
| Build guide | [`docs/build.md`](docs/build.md) | same file (bilingual) |

**Demo IDL:** `proto/shell/ipc/v1/ipc.proto` + [`docs/zh/demo-ipc.md`](docs/zh/demo-ipc.md). If the long-form product spec sketches differ from the checked-in `.proto`, prefer `.proto` / Demo IPC for what ships today.

## Layout

```text
MultiProcessShell/
  cmake/           Qt / Protobuf helpers
  proto/           shell.ipc.v1 IDL
  src/             Host / Client / common / ipc_qt
  demos/           Chrome-like Demo (mps_demo_host / mps_demo_client)
  tests/           Unit / protocol tests (M0)
  clients/python/  M4b smoke (later)
  docs/            Specs & guides (zh + en)
  scripts/         build_repo / build_qt / deploy_demo
  dist/Demo/       Self-contained Windows bundle (generated; gitignored)
```

## Quick start (Windows)

1. Open an **x64 Native Tools / vcvars** shell.
2. Set `QTDIR` to a Qt **6.8+** prefix; put `%QTDIR%\bin` on `PATH`.
3. Build and deploy:

```bat
python scripts\build_repo.py
```

4. Double-click (no console window):

```text
dist\Demo\mps_demo_host.exe
```

`build_repo.py` builds Host/Client Demo by default and on Windows runs `scripts/deploy_demo.py` (`windeployqt`) so Qt/CRT sit beside the exes.

M0 tests only (no Qt):

```bat
python scripts\build_repo.py --no-demos --test
```

## Status

- Specs, Demo morphology/IPC, and scripts are in-tree.
- **M0 done**: `mps::ipc` framing + generated `shell.ipc.v1` + `mps_ipc_tests`.
- **Demo done (Windows)**: Home tab, Create Client, same-Client New Window, tab close with activation history, tear-out/merge hooks, `SetParent` embed, GUI subsystem (no console).

## License

[MIT](LICENSE)
