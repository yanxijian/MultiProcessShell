# Demo Morphology (Final)

> **中文主文档**: [`../zh/demo-morphology.md`](../zh/demo-morphology.md)  
> **Status**: Finalized (matches the in-repo Demo)

## 1. Product intuition

Chrome-like multi-window / multi-tab behavior:

| Capability | Behavior |
|------------|----------|
| Side-by-side | Windows from different Client processes appear as **tabs** in one shell |
| Close | Close a Client tab; switch via **MRU activation history** |
| Tear-out | Drag a Client tab **out** into a **new** top-level shell |
| Merge | Drag a tab **into** another shell’s tab bar (insert at drop point) |
| Reorder | Drag Client tabs within the same shell to change order |
| Destroy empty | If a shell has **no Client tabs** left (Home only) and is not the sole shell → **destroy** it |

Phase-1 platform: **Windows (form A)**.

## 2. Startup and creation

### 2.1 Startup

- Demo starts with **exactly one** top-level shell.  
- Permanent **Home** tab (not closable, not tear-out).  
- Home content center shows **Create Client**; no Client process starts automatically.

### 2.2 Create Client (new process / pageType session)

- On **Home**, click **Create Client** → start a Client process and add a Client tab.  
- First child title: `Client1-Window1`.  
- To create another Client, switch back to **Home** and click **Create Client** again.

### 2.3 Same Client, new child window

- Client page content has **新建窗口** / New Window.  
- Click → IPC `Invoke("demo.request_new_window")` → Host sends another `CreateSubWindow` in the **same Client process** and adds a tab.  
- Titles: `Client1-Window2`, `Client1-Window3`, …

### 2.4 Multiple Clients

- Create Client again from Home → `Client2-Window1`, …  
- Different Clients = different processes (form A); tabs may share one shell (optional accent colors).

## 3. Tab title rules

```text
Client{N}-Window{M}
```

| Field | Meaning | Increment |
|-------|---------|-----------|
| `N` | Client instance index | +1 per new Client process (per Demo session) |
| `M` | Child window index within that Client | +1 per new child in that Client |

Examples: `Client1-Window1`, `Client1-Window2`, `Client2-Window1`.  
**Home** title is fixed (`Home`) and outside this scheme.

## 4. Window structure

```text
┌─ Shell (custom title bar) ─────────────────────────────┐
│  [Home] [Client1-Window1 ×] [Client2-Window1 ×]  _ □ × │
├────────────────────────────────────────────────────────┤
│  Home active → center “Create Client”                  │
│  Client tab active → embedded Client HWND (“新建窗口”) │
└────────────────────────────────────────────────────────┘
```

- **Title bar**: Home + Client tabs + min / max / close.  
- **Workspace**: Home page or current Client embed (should fill the client area).

## 5. Tear-out / merge (Chrome-like)

### 5.1 Drag visuals

| Layer | When | Behavior |
|-------|------|----------|
| **Tab ghost** | As soon as a Client tab drag starts | Always follows the cursor; on the strip, **Y locks to the tab row**, X follows; after tear-out it stays above the window preview |
| **Window preview** | After leaving the strip past the **leave slop** | Not an independent hotspot follow; positioned from the tab ghost so the preview **title/tab bar vertically wraps** the button (Home stub on the left); returning needs the tighter **return slop** to re-enter strip mode |

While dragging: source tab is a transparent placeholder; other tabs on the same / merge-target shell **live-yield** (not a blue insert bar); source shell shows the previous active tab.  
**After tear-out**: as soon as the window preview appears, siblings on the source shell **immediately claim** the vacated slot (no wait for mouse-up); returning to the strip re-opens a yield gap.  
Over min / max / close → **forbidden cursor** (not a merge target).

### 5.2 Drop rules

1. **Horizontal drag in the same shell** → **reorder** from live yield (Home stays leftmost; cannot insert before Home).  
2. **Release outside the strip** → **new top-level shell** at preview geometry (same wrap-around-tab alignment); preview briefly covers the new shell until the first embed paint (less flash).  
3. **Drop on another shell’s tabs + trailing strip** → **merge** (live yield shows the slot; not window buttons).  
4. **Esc** or release still near the strip / return hysteresis → **cancel** (ghost snaps back; no tear-out). On Windows OLE, Esc is polled by the Host.  
5. If the source then has **no Client tabs** (Home only) and is not the sole shell → **destroy** the source.  
6. DnD updates **Host model only**; mime carries only tabId (`application/x-mps-tab-id`), never HWND; then reattach.  
7. **Home** cannot tear out / merge / reorder.

Host notes: `TabDragGhost` + `TearOutPreview::alignToTabContent`; `previewTabYieldAtCursor` / `collapseTornOutTabSlot` / `commitTabYieldPreview`; the yield gap has no child widget so OLE may return `IgnoreAction` — Host still commits reorder/merge.

## 6. Close tab + activation history

- Close a Client tab (× **or middle-click the tab**, Chrome-like) → Host `QueryCloseSubWindow` → Client accepts and emits `SubWindowRemoved` (Host may remove the tab on accept; the Client child window closes with it).  
- **Home** is not closable (middle-click ignored).  
- MRU history includes **Home and Client tabs**; closing the active tab selects the previous still-present tab (not forced to Home).

## 7. Scope

### 7.1 In this Demo (implemented)

- One shell at start → **Home** + Create Client  
- Multi-Client processes + same-Client multi-window + title increments  
- Close tab (history), same-shell reorder, tear-out, merge, destroy spare shells with no Client tabs  
- Shell close cleanup / Host survives Client kill  
- Windows `SetParent` embed + minimal Protobuf contract  

Manual checklist: [demo-acceptance.md](demo-acceptance.md).

### 7.2 Deferred / simplified

- Continuous tab-ghost ↔ window-preview morph, stronger magnetic merge  
- Heartbeat timer / unhealthy UI (protocol reserved)  
- macOS / Linux embed  
- Full multi-language EmbedHelper  

## 8. Repo paths

| Path | Role |
|------|------|
| `demos/` | `mps_demo_host` / `mps_demo_client` |
| `src/host/` | Shell, tabs, sessions, Win embed; tear-out: `tear_out_preview.*` |
| `src/client/` | Client process and pages |
| `src/common/` + `proto/` | Framing and IDL |
| `dist/Demo/` | Windows double-click bundle (generated) |

## 9. Decisions

| # | Decision |
|---|----------|
| 1 | Chrome-like tabs / close / tear-out / merge |
| 2 | Spare shell with no Client tabs → destroy |
| 3 | One shell + permanent **Home**; Create Client on Home |
| 4 | Client page New Window → same-Client child |
| 5 | Titles: `Client{N}-Window{M}`; Home excluded |
| 6 | Close uses activation history (includes Home); not forced to Home |

## 10. IPC

See [demo-ipc.md](demo-ipc.md).
