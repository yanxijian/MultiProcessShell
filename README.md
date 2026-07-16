# MultiProcessShell

MIT-licensed C++/Qt multi-process shell: Host chrome + Client native-window embed + Protobuf IPC.

首期平台：**Windows（形态 A）**；macOS / Linux 目录占位。

## Documentation

| Doc | 中文 | English |
|-----|------|---------|
| Technical spec (authoritative) | [`docs/zh/multiprocess-shell-spec.md`](docs/zh/multiprocess-shell-spec.md) | [`docs/en/multiprocess-shell-spec.md`](docs/en/multiprocess-shell-spec.md) |
| Demo morphology (final) | [`docs/zh/demo-morphology.md`](docs/zh/demo-morphology.md) | [`docs/en/demo-morphology.md`](docs/en/demo-morphology.md) |
| Demo IPC contract (final) | [`docs/zh/demo-ipc.md`](docs/zh/demo-ipc.md) | [`docs/en/demo-ipc.md`](docs/en/demo-ipc.md) |
| Build guide | [`docs/build.md`](docs/build.md) | same file (bilingual) |

## Layout

```text
MultiProcessShell/
  cmake/           Qt / Protobuf helpers
  proto/           shell.ipc.v1 IDL
  src/             Host / Client / common (code TBD)
  demos/           Minimum window-embed Demo (morphology TBD; code TBD)
  tests/           Unit / protocol tests
  clients/python/  M4b smoke (later)
  docs/            Specs & guides (zh + en)
  scripts/         Python: build_repo / build_qt
```

## Quick start

1. Set `QTDIR` to a Qt 6.8+ prefix; ensure `%QTDIR%\bin` is on `PATH`.
2. `python scripts/build_repo.py` — configures the CMake project (no Demo/src targets yet).
3. Optional: `python scripts/build_qt.py` — helps build a minimal external Qt into `QTDIR`.

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QTDIR%"
cmake --build build
```

## Status

- Specs, Demo morphology/IPC contract, and scripts are in-tree.
- **M0 done**: `mps::ipc` framing + generated `shell.ipc.v1` + `mps_ipc_tests`.
- Host/Client/`demos/` application code not started yet.

## License

[MIT](LICENSE)
