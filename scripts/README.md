# Scripts (Python)

| Script | Purpose |
|--------|---------|
| `build_repo.py` | Configure/build this repo using `QTDIR` |
| `build_qt.py` | Help configure/build an external Qt (qtbase) into a prefix |

Windows Qt builds must run inside an **MSVC Developer Command Prompt** (vcvars).

```bash
python scripts/build_repo.py --configure-only
python scripts/build_qt.py --source <qt-everywhere-src> --build-dir <qt-build> --prefix %QTDIR%
```
