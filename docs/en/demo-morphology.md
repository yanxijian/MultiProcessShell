# Demo Morphology (Final)

> **Status**: Finalized  
> **Chinese**: [`../zh/demo-morphology.md`](../zh/demo-morphology.md)

## 1. Product intuition

Chrome-like multi-window / multi-tab behavior:

| Capability | Behavior |
|------------|----------|
| Side-by-side | Windows from different Client processes appear as **tabs** in one shell |
| Close | Close a single tab (one child window / page) |
| Tear-out | Drag a tab **out** to create a **new** top-level shell |
| Merge | Drag a tab **into** another shell’s tab bar |
| Destroy empty | If the torn-out tab was the **last** tab, the **original** shell is **destroyed** |

Phase-1 platform: **Windows (form A)**.

## 2. Startup and creation

### 2.1 Startup

- Demo starts with **exactly one** top-level shell.  
- Center of the content area: a button (e.g. **Create Client**).  
- No Client process is started automatically.

### 2.2 Create Client

- Click **Create Client** → start a Client process and add a tab.  
- First child title: `Client1-Window1`.

### 2.3 Same Client, new child window

- Inside the Client page content: a button (e.g. **New Window**).  
- Creates another child window **in the same Client process** and a new tab.  
- Titles: `Client1-Window2`, `Client1-Window3`, …

### 2.4 Multiple Clients

- Click **Create Client** again → `Client2-Window1`, …  
- Different Clients = different processes; tabs may share one shell.

## 3. Tab title rules

```text
Client{N}-Window{M}
```

| Field | Meaning | Increment |
|-------|---------|-----------|
| `N` | Client instance index | +1 per new Client process (per Demo session) |
| `M` | Child window index within that Client | +1 per new child in that Client |

## 4. Tear-out / merge

1. Drag tab outside → new top-level shell hosts that tab.  
2. If the source shell has **no tabs left** → **destroy** that shell.  
3. Drop on another shell’s tab bar → merge (insert index refined in implementation).  
4. DnD updates **Host model only**; never put HWND in mime; then `reattach`.

## 5. In scope / deferred

**In scope:** one shell at start; Create Client; same-Client New Window; titles; close tab; tear-out; merge; destroy empty shell; Windows embed + minimal Protobuf.

**Deferred:** polished drag animation, full health UI, macOS/Linux embed, multi-language EmbedHelper.

## 6. Decisions

| # | Decision |
|---|----------|
| 1 | Chrome-like tabs / tear-out / merge |
| 2 | Last tab torn out → destroy source shell |
| 3 | Start with one shell + center **Create Client** |
| 4 | Client page center **New Window** (same Client) |
| 5 | Titles: `Client{N}-Window{M}` |
