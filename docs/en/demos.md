# demos/

> **中文主文档**: [`../demos/README.md`](../demos/README.md)

| Target | Role |
|--------|------|
| `mps_demo_host` | Browser-style detachable-tab shell (Home, Create Client, tabs, embed) |
| `mps_demo_client` | Client process (spawned by Host; New Window) |

Morphology / IPC: [demo-morphology.md](demo-morphology.md), [demo-ipc.md](demo-ipc.md) (Chinese primaries under `docs/zh/`).

```bat
:: vcvars x64 shell, QTDIR set
python scripts\build_repo.py
:: double-click (GUI subsystem, no console):
dist\Demo\mps_demo_host.exe
```

On Windows, `build_repo.py` auto-runs `scripts/deploy_demo.py` (`windeployqt`) and syncs `dist/Demo/`.

## Current UX

- Permanent **Home** tab (not closable / not tear-out); **Create Client** on Home.  
- Client tabs: close via × **or middle-click**; MRU activation history (not forced to Home).  
- Same-Client New Window: `Invoke("demo.request_new_window")` → Host `CreateSubWindow`.  
- Tab drag: tab ghost + live yield on strip; after leaving the strip the window preview **wraps** the tab and the source slot is claimed immediately; Esc cancel snaps back; merge hot zone excludes window buttons.