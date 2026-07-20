# scripts/ (Python)

> **中文主文档**: [`../scripts/README.md`](../scripts/README.md)

| Script | Purpose |
|--------|---------|
| `build_repo.py` | Configure/build this repo (`QTDIR` required unless `--no-demos`) |
| `build_qt.py` | Help build external Qt (qtbase) into a prefix |
| `deploy_demo.py` | Windows: `windeployqt` beside demos + sync `dist/Demo/` |
| `format_source.py` | Format handwritten C/C++ via root `.clang-format` (skips `build/`, `_deps/`, `*.pb.*`) |

## Windows notes

- Run inside an **MSVC x64 Developer / vcvars** shell (`cl`, `rc`, `mt` on `PATH`).  
- Default build enables Demo + Host/Client; set `QTDIR` and `%QTDIR%\bin` on `PATH`.  
- After a successful Demo build, `build_repo.py` auto-runs `deploy_demo.py`.  
- `--no-demos` also disables `MPS_BUILD_SRC` (protocol tests only).  
- **Incremental:** skips cmake reconfigure when options match (avoids protobuf rebuild); `--reconfigure` / `--fresh` when needed.

```bat
python scripts\build_repo.py
python scripts\build_repo.py --no-demos --test
python scripts\deploy_demo.py
python scripts\format_source.py
python scripts\format_source.py --check
python scripts\build_qt.py --source <qt-everywhere-src> --build-dir <qt-build> --prefix %QTDIR%
```

Double-click Host: `dist\Demo\mps_demo_host.exe`.  
Full guide: [build.md](build.md).
