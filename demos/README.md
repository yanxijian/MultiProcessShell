# demos/

| Target | Role |
|--------|------|
| `mps_demo_host` | Chrome-like shell UI |
| `mps_demo_client` | Client process (spawned by Host) |

Morphology / IPC: `docs/zh/demo-morphology.md`, `docs/zh/demo-ipc.md`.

```bat
:: from vcvars x64 shell, with QTDIR set
python scripts\build_repo.py
:: then double-click:
dist\Demo\mps_demo_host.exe
```

`build_repo.py` 在 Windows 上会自动调用 `scripts/deploy_demo.py`（`windeployqt`），把 Qt/CRT 依赖拷到 exe 旁，并同步到 `dist/Demo/`。
