# Demo Morphology (Final)

> **Status**: Finalized (matches the in-repo Demo)  
> **Chinese**: [`../zh/demo-morphology.md`](../zh/demo-morphology.md)

## 1. Product intuition

Chrome-like multi-window / multi-tab behavior:

| Capability | Behavior |
|------------|----------|
| Side-by-side | Windows from different Client processes appear as **tabs** in one shell |
| Close | Close a Client tab; activate the previous tab via **MRU history** |
| Tear-out | Drag a Client tab **out** to create a **new** top-level shell |
| Merge | Drag a tab **into** another shell’s tab bar |
| Destroy empty | If a shell has **no Client tabs** left (Home only) and is not the last shell → **destroy** it |

Phase-1 platform: **Windows (form A)**.

## 2. Startup and creation

### 2.1 Startup

- Exactly one top-level shell.  
- Permanent **Home** tab (not closable, not tear-out).  
- **Create Client** lives on Home content; no Client auto-start.

### 2.2 Create Client

- On Home, click **Create Client** → start a Client process and add a Client tab.  
- First child title: `Client1-Window1`.  
- Switch back to **Home** to create another Client.

### 2.3 Same Client, new child window

- Inside the Client page: **New Window** / 「新建窗口」.  
- Client sends `Invoke("demo.request_new_window")`; Host replies with another `CreateSubWindow`.  
- Titles: `Client1-Window2`, …

### 2.4 Multiple Clients

- Create Client again from Home → `Client2-Window1`, …  
- Different Clients = different processes; tabs may share one shell.

## 3. Tab title rules

```text
Client{N}-Window{M}
```

| Field | Meaning | Increment |
|-------|---------|-----------|
| `N` | Client instance index | +1 per new Client process |
| `M` | Child window index within that Client | +1 per new child |

**Home** title is fixed (`Home`) and is outside this scheme.

## 4. Window structure

```text
[Home] [Client1-Window1 ×] [Client2-Window1 ×]   _ □ ×
────────────────────────────────────────────────────
 Home active → Create Client
 Client tab active → embedded Client HWND (New Window button inside)
```

## 5. Tear-out / merge

1. Drag a **Client** tab outside → new shell (with Home + that tab).  
2. Source shell with no Client tabs left (Home only), if not the sole shell → **destroy**.  
3. Drop on another shell’s tab bar → merge.  
4. Host model only; no HWND in mime; then reattach.  
5. **Home** cannot tear out / merge.

## 6. Close + activation history

MRU history includes Home and Client tabs. Closing the active tab selects the previous still-present tab (not forced to Home).

## 7. In scope / deferred

**In scope (implemented):** Home + Create Client; multi-Client; same-Client New Window; titles; close with history; tear-out/merge; destroy spare empty shells; Windows embed + Demo Protobuf.

**Deferred:** polished drag animation; heartbeat timer/UI; macOS/Linux embed; multi-language EmbedHelper.

## 8. Decisions

| # | Decision |
|---|----------|
| 1 | Chrome-like tabs / tear-out / merge |
| 2 | No Client tabs left on a spare shell → destroy |
| 3 | Start with one shell + permanent **Home**; Create Client on Home |
| 4 | Client page New Window (same Client) via Invoke |
| 5 | Titles: `Client{N}-Window{M}` |
| 6 | Close uses activation history (includes Home) |

## 9. IPC

See [`demo-ipc.md`](demo-ipc.md).
