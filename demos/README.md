# demos/

| Target | Role |
|--------|------|
| `mps_demo_host` | Chrome-like shell UI |
| `mps_demo_client` | Client process (spawned by Host) |

Morphology / IPC: `docs/zh/demo-morphology.md`, `docs/zh/demo-ipc.md`.

```bat
:: from vcvars x64 shell, with QTDIR set
python scripts\build_repo.py
build\demos\mps_demo_host.exe
```
