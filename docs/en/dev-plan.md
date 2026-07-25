# MultiProcessShell development plan

> **中文主文档**：[../zh/dev-plan.md](../zh/dev-plan.md)  
> Status: near/mid-term investment notes. Product vision/milestones remain in the spec; Demo authority is demo-ipc / `.proto`.  
> Updated: 2026-07-25 (M4b Python Hello + Envelope helper)

---

## Stance

| Conclusion | Note |
|------------|------|
| **No whole-repo rewrite** | Windows Demo path works; `frame` / `EnvelopeChannel` / `tab_strip` / `TabEmbedMap` are already deep seams |
| **Advance by spec milestones** | Spec §12.4; **M4b done** (`clients/python` + `mps_m4b_python_hello`) |
| **Deepen with the next feature** | Next: M6 heartbeat/timeouts; peel tear-out rules when touching that code |

Reuse `tab_strip` / `tab_embed_map` / `envelope_builder.hpp` (pure rules / thin helpers; Host·Client + tests share).

## Deepening candidates (recorded, deferred)

| Strength | Candidate |
|----------|-----------|
| **Strong (top)** | Keep `wid` behind the embed seam — **done** |
| **Strong** | Extract tear-out / merge / shell-lifecycle rules like `tab_strip` |
| Done | Envelope helper — `makeEnvelope` / `makeResponse` in `envelope_builder.hpp` |
| Worth exploring | Land `IEmbedBackend` (Win-only first) — when a second adapter is planned |

Scan top recommendation: ~~**wid → embed** first~~ **done**; peel Shell rules when touching tear-out.

## Explicitly not now

- Pre-refactor for a future IPC stack swap  
- A second-platform Backend interface with no second adapter planned  
- One-shot rewrite of Host into the full paper object tree (`ClientPage`, …)

## Milestone hooks (notes)

| Direction | Posture |
|-----------|---------|
| ~~M4b Python Hello~~ | **Done** — `hello_client.py`, offline `test_frame_envelope.py`, Qt harness `mps_m4b_python_hello` |
| x11 / inproc / multi-client | `IEmbedBackend` next (`wid` already localized) |
| M6 heartbeat / unhealthy UI | Extend Session-side helper / timers |
| QThemeEngine in Host | Discuss at integration time — do not pre-refactor now |
