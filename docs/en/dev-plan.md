# MultiProcessShell development plan

> **中文主文档**：[../zh/dev-plan.md](../zh/dev-plan.md)  
> Status: near/mid-term investment notes. Product vision/milestones remain in the spec; Demo authority is demo-ipc / `.proto`.  
> Updated: 2026-07-25 (`wid` behind embed seam)

---

## Stance

| Conclusion | Note |
|------------|------|
| **No whole-repo rewrite** | Windows Demo path works; `frame` / `EnvelopeChannel` / `tab_strip` / `TabEmbedMap` are already deep seams |
| **Advance by spec milestones** | Spec §12.4 (M4b, multi-backend, heartbeat/timeouts, optional M7, …) |
| **Deepen with the next feature** | 2–3 targeted deepenings before M4b / second embed / health — not a standalone refactor season |

Reuse the `tab_strip` / `tab_embed_map` pattern (pure rules; Host + tests share the interface).

## Deepening candidates (recorded, deferred)

| Strength | Candidate |
|----------|-----------|
| **Strong (top)** | Keep `wid` behind the embed seam — **done**: no `TabInfo.wid`; `EmbedContainer` + `TabEmbedMap` bind/activate/transfer by `tabId`; Session still passes `wid` at handshake only |
| **Strong** | Extract tear-out / merge / shell-lifecycle rules like `tab_strip` |
| Worth exploring | Envelope / SessionRpc helper (Host + Client stop hand-packing) |
| Worth exploring | Land `IEmbedBackend` (Win-only first) — bundle when a second adapter is planned |

Scan top recommendation: ~~**wid → embed** first~~ **done**; peel Shell rules when touching tear-out.

## Explicitly not now

- Pre-refactor for a future IPC stack swap  
- A second-platform Backend interface with no second adapter planned  
- One-shot rewrite of Host into the full paper object tree (`ClientPage`, …)

## Milestone hooks (notes)

| Direction | Posture |
|-----------|---------|
| M4b Python Hello | Envelope helper has leverage before/during |
| x11 / inproc / multi-client | `IEmbedBackend` next (`wid` already localized) |
| M6 heartbeat / unhealthy UI | Protocol logic in a SessionRpc-style module |
| QThemeEngine in Host | Discuss at integration time — do not pre-refactor now |
