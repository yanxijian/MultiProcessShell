# Demo Morphology (Final)

> **中文主文档**: [`../zh/demo-morphology.md`](../zh/demo-morphology.md)  
> **Status**: Finalized (matches the in-repo Demo)

## 1. Product intuition

Browser-style detachable multi-window / multi-tab behavior:

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
- Permanent **Home** tab (not closable, not tear-out) — owned by **`mps_host`**.  
- **Home client area** (Create Client, Light/Dark) is injected by **`demo_host`** via `ShellApp::setHomeContentFactory`; the framework default is an empty slot. No Client process starts automatically.

### 2.2 Create Client (new process / clientKind session)

- On **Home**, click **Create Client** → start a Client process and add a Client tab.  
- First child title: `Tab1` (framework default).  
- To create another Client, switch back to **Home** and click **Create Client** again.

### 2.3 Same Client, new child window

- Client content is a **frameless QFluentRibbon** `RibbonWindow` (`ContentView` / `RibbonContentWindow`); Ribbon **New Window** → IPC `Invoke("demo.request_new_window")` → Host `CreateSubWindow` in the **same Client process**. See Chinese [qfr-demo-client.md](../zh/qfr-demo-client.md).  
- Titles: `Tab2`, `Tab3`, …

### 2.4 Multiple Clients

- Create Client again from Home → another process / `Tab1`, …  
- Different Clients = different processes (form A); tabs may share one shell. Theme Light/Dark is **global** (Host SSOT via `Invoke theme.set`).

## 3. Tab title rules

```text
Tab{M}
```

| Field | Meaning | Increment |
|-------|---------|-----------|
| `M` | ContentView / Tab index within that Client | +1 per new ContentView in that Client |

Framework default examples: `Tab1`, `Tab2`. A product Host may inject another scheme via `TabTitleFactory`.  
**Home** title is fixed (`Home`) and outside this scheme.

## 4. Window structure

```text
┌─ Shell (custom title bar) ─────────────────────────────┐
│  [Home] [Tab1 ×] [Tab1 ×]  _ □ × │
├────────────────────────────────────────────────────────┤
│  Home active → center “Create Client”                  │
│  Client tab active → embedded Client HWND (“新建窗口”) │
└────────────────────────────────────────────────────────┘
```

- **Title bar**: Home + Client tabs + min / max / close (framework chrome).  
- **Workspace**: Home client-area content (Demo-injected) or the current Client embed (should fill the client area).

## 5. Tear-out / merge (browser-style detachable tabs)

### 5.1 Drag visuals

| Layer | When | Behavior |
|-------|------|----------|
| **Tab ghost** | As soon as a Client tab drag starts | Always follows the cursor; on the strip, **Y locks to the tab row**, X follows; after tear-out it stays above the window preview |
| **Window preview** | After leaving the strip past the **leave slop** | Not an independent hotspot follow; positioned from the tab ghost so the preview **title/tab bar vertically wraps** the button (Home stub on the left); returning needs the tighter **return slop** to re-enter strip mode |

While dragging: source tab is a transparent placeholder; other tabs on the same / merge-target shell **live-yield** (not a blue insert bar); source shell shows the previous active tab.  
**After tear-out**: as soon as the window preview appears, siblings on the source shell **immediately claim** the vacated slot (no wait for mouse-up); returning to the strip re-opens a yield gap.  
**Sole Client tab** (spec §7.2 / Chrome last-tab): skip the window preview and **move the real shell** with the cursor; hovering another shell’s tab strip (narrow magnet) shows target yield and **auto-merges** (translate shell to the slot and fade out — no mouse-up); release in empty space leaves the shell in place (no second shell).  
Over min / max / close → **forbidden cursor** (not a merge target); **release does not tear-out** (treated as cancel).  
If the Client clicks「新建窗口」during tear-out → Host **queues** `CreateSubWindow` and sends after the drag ends (spec S5).

### 5.2 Drop rules

1. **Horizontal drag in the same shell** → **reorder** from live yield (Home stays leftmost; cannot insert before Home).  
2. **Release outside the strip** → **new top-level shell** at preview geometry (same wrap-around-tab alignment); preview briefly covers the new shell until the first embed paint (less flash). **Sole Client tab** exception: the real shell already followed the cursor — release leaves it in place (no second shell).
3. **Drop on another shell’s tabs + trailing strip** → **merge** (live yield shows the slot; not window buttons).
4. **Esc**, release still near the strip / return hysteresis, or release over **min/max/close** → **cancel** (ghost snaps back; no tear-out). On Windows OLE, Esc is polled by the Host.  
5. If the source then has **no Client tabs** (Home only) and is not the sole shell → **destroy** the source.  
6. DnD updates **Host model only**; mime carries only tabId (`application/x-mps-tab-id`), never HWND; then reattach.  
7. **Home** cannot tear out / merge / reorder.

Host notes: `TabDragGhost` + `TearOutPreview::alignToTabContent`; `previewTabYieldAtCursor` / `collapseTornOutTabSlot` / `commitTabYieldPreview`; the yield gap has no child widget so OLE may return `IgnoreAction` — Host still commits reorder/merge.

## 6. Close tab + activation history

- Close a Client tab (× **or middle-click the tab**) → Host `QueryCloseSubWindow` → Client accepts and emits `SubWindowRemoved` (Host may remove the tab on accept; the Client child window closes with it).  
- **Home** is not closable (middle-click ignored).  
- MRU history includes **Home and Client tabs**; closing the active tab selects the previous still-present tab (not forced to Home).

## 7. Scope

### 7.1 In this Demo (implemented)

- One shell at start → **Home** + Create Client  
- Multi-Client processes + same-Client multi-window + title increments  
- Close tab (history), same-shell reorder, tear-out, merge, destroy spare shells with no Client tabs  
- Shell close cleanup / Host survives Client kill  
- Windows `SetParent` embed + minimal Protobuf contract  
- **Heartbeat**: Client 2s `Heartbeat`; Host ~6s silence → Tab「无响应」; right-click「终止进程」(no auto-kill)  

Manual checklist: [demo-acceptance.md](demo-acceptance.md).

### 7.2 Deferred / simplified

- Continuous tab-ghost ↔ window-preview morph, stronger magnetic merge  
- Generic REQ timeout UI (separate from heartbeat Unhealthy)  
- macOS / Linux embed  
- Full multi-language EmbedHelper  

## 8. Repo paths

| Path | Role |
|------|------|
| `demos/demo_host/` | Host Demo: `ThemeService`, Home client area (`home_content` / `HomeContent`), composition |
| `demos/demo_client/` | Client Demo: QFR Ribbon content |
| `src/host/` | Shell, tabs, sessions, Win embed, Home **slot**; tear-out: `tear_out_preview.*` (**no** business client-area widgets) |
| `src/client/` | Client process + abstract `ContentView` (no concrete business UI) |
| `src/common/` + `proto/` | Framing and IDL |

## 9. Decisions

| # | Decision |
|---|----------|
| 1 | Browser-style detachable tabs / close / tear-out / merge |
| 2 | Spare shell with no Client tabs → destroy |
| 3 | One shell + permanent **Home tab** (framework); Create Client / Light/Dark in **demo_host** Home client area |
| 4 | Client content New Window → same-Client child |
| 5 | Titles: framework default `Tab{M}` (product may inject); Home excluded |
| 6 | Close uses activation history (includes Home); not forced to Home |

## 10. IPC

See [demo-ipc.md](demo-ipc.md).
