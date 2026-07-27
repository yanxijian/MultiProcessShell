# Build guide

> **中文主文档**: [`../zh/build.md`](../zh/build.md)

## Architecture: independent frameworks, composed demos

| Library | Depends on |
|---------|------------|
| `qte_engine` | Qt only |
| `qfr_ribbon` | Qt only (local `ribbon_tokens`; does not link QTE) |
| `mps_*` | Qt + protobuf (does not link QTE/QFR) |

QTE / QFR are linked only by **demos**: `mps_demo_host`→QTE, `mps_demo_client`→QFR+QTE.

## Recommended: local prefix (shared libs)

```bat
:: QTDIR = Qt 6.8+ prefix; PREFIX = install root (optional; default sibling prefix/)
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
:: Run (from this repo root):
build-shared\demos\mps_demo_host.exe
```

Local convention uses out-of-source dir **`build-shared`** (`install_stack.py` and `build_repo.py` default). CI may still use `-B build`.

## Dependencies

- Windows: MSVC x64, CMake ≥ 3.21, Ninja, Python 3.10+
- Qt 6.8+
- For demos: installed QThemeEngine + QFluentRibbon (`CMAKE_PREFIX_PATH`)
- Protobuf via FetchContent (`MPS_BUILD_SHARED` default ON → shared `libprotobuf.dll` + `abseil_dll.dll` + `utf8_validity.dll`). Generated `Envelope` lives only in `mps_ipc.dll`; receivers use `EnvelopePtr` so destruction stays in-module.

## Demo-only sibling embed

```bat
cmake -S . -B build-embed -G Ninja -DCMAKE_PREFIX_PATH=%QTDIR% ^
  -DMPS_DEV_EMBED_QTE=ON -DMPS_DEV_EMBED_QFR=ON -DMPS_INSTALL=OFF
```

## Helper scripts

```bash
python scripts/install_stack.py --help
python scripts/build_repo.py --help
```

See also: [qfr-demo-client.md](../zh/qfr-demo-client.md) (Chinese), [CI](ci.md).
