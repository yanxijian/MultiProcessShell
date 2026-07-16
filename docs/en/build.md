# Build guide

> **Chinese (primary)**: [`../zh/build.md`](../zh/build.md)

## Dependencies

- Windows: MSVC (x64), CMake ≥ 3.21, Ninja (recommended), Python 3.10+, Git (FetchContent)
- Qt: open-source **6.8+** (needed for default Host/Client/Demo; **optional for M0-only**)
- Protobuf / GoogleTest: CMake **FetchContent**

## Environment

| Variable | Purpose |
|----------|---------|
| `QTDIR` | Qt install prefix (required for default Demo build) |
| `PATH` | should include `%QTDIR%\bin` |

Use `%QTDIR%` / `$env:QTDIR` in projects.

On Windows, configure/build inside an **x64 vcvars / Native Tools** shell (`cl`, `rc`, `mt`).

## Default: Demo + deploy

CMake defaults: `MPS_BUILD_SRC=ON`, `MPS_BUILD_DEMOS=ON`, `MPS_BUILD_TESTS=ON`.

```bat
python scripts\build_repo.py
:: double-click (no console):
dist\Demo\mps_demo_host.exe
```

On Windows, a successful `build_repo.py` runs `scripts/deploy_demo.py` (`windeployqt`), copies Qt/CRT next to the exes, and syncs `dist/Demo/`. You can also run:

```bat
python scripts\deploy_demo.py
```

Demo targets use the **Windows GUI** subsystem (`WIN32_EXECUTABLE`); double-click does not open an extra console.

## M0 (framing + proto tests, Qt optional)

```bat
python scripts\build_repo.py --no-demos --test
```

Note: `--no-demos` also turns off `MPS_BUILD_SRC`.

Equivalent manual CMake:

```bat
cmake -S . -B build -G Ninja ^
  -DMPS_BUILD_TESTS=ON -DMPS_FETCH_PROTOBUF=ON ^
  -DMPS_BUILD_DEMOS=OFF -DMPS_BUILD_SRC=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

First configure downloads protobuf and GoogleTest (needs network).

## Helper scripts

```bash
python scripts/build_qt.py --help
python scripts/build_repo.py --help
python scripts/deploy_demo.py --help
```

## Manual CMake (with Demo)

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Related: [product spec](multiprocess-shell-spec.md), [Demo morphology](demo-morphology.md), [Demo IPC](demo-ipc.md)  
IDL: `proto/shell/ipc/v1/ipc.proto`
