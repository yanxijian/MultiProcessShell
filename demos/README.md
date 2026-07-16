# demos/

| Target | Role |
|--------|------|
| `mps_demo_host` | Chrome-like shell (Home tab, Create Client, tabs, embed) |
| `mps_demo_client` | Client process (spawned by Host; 「新建窗口」) |

Morphology / IPC: `docs/zh/demo-morphology.md`, `docs/zh/demo-ipc.md` (EN twins under `docs/en/`).

```bat
:: from vcvars x64 shell, with QTDIR set
python scripts\build_repo.py
:: then double-click (GUI subsystem — no console):
dist\Demo\mps_demo_host.exe
```

`build_repo.py` on Windows auto-runs `scripts/deploy_demo.py` (`windeployqt`), copies Qt/CRT next to the exes, and syncs `dist/Demo/`.

## Demo UX (current)

- Permanent **Home** tab (not closable / not tear-out); **Create Client** on Home content.
- Client tabs: close via ×; activation follows MRU history (not forced back to Home).
- Same-Client **新建窗口** via `Invoke("demo.request_new_window")` → Host `CreateSubWindow`.
