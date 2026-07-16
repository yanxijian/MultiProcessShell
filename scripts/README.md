# Scripts (Python)

| Script | Purpose |
|--------|---------|
| `build_repo.py` | Configure/build this repo (`QTDIR` required unless `--no-demos`) |
| `build_qt.py` | Help configure/build an external Qt (qtbase) into a prefix |
| `deploy_demo.py` | Windows: `windeployqt` beside demos + copy bundle to `dist/Demo/` |

## Windows notes

- Run inside an **MSVC x64 Developer / vcvars** shell (`cl`, `rc`, `mt` on `PATH`).
- Default build enables Demo + Host/Client libraries; set `QTDIR` and `%QTDIR%\bin` on `PATH`.
- After a successful Demo build, `build_repo.py` auto-runs `deploy_demo.py`.
- `--no-demos` also disables `MPS_BUILD_SRC` (protocol tests only).

```bat
python scripts\build_repo.py
python scripts\build_repo.py --no-demos --test
python scripts\deploy_demo.py
python scripts\build_qt.py --source <qt-everywhere-src> --build-dir <qt-build> --prefix %QTDIR%
```

Double-click runnable Host: `dist\Demo\mps_demo_host.exe`.
