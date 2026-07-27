# MultiProcessShell development plan

> **中文主文档**：[../zh/dev-plan.md](../zh/dev-plan.md)  
> Status: near/mid-term investment notes. Product vision/milestones remain in the spec; Demo authority is demo-ipc / `.proto`.  
> Updated: 2026-07-26 (M5 gap audit: [../zh/m5-gap-audit.md](../zh/m5-gap-audit.md))

---

## Stance

| Conclusion | Note |
|------------|------|
| **No whole-repo rewrite** | Windows Demo path works; `frame` / `EnvelopeChannel` / `tab_strip` / `TabEmbedMap` are already deep seams |
| **Advance by spec milestones** | **M4b / M5 / M6 done** (heartbeat 2s / 6s timeout /「无响应」+ terminate); next multi-backend / optional M7 |
| **Deepen with the next feature** | Tear-out rule extraction (G6) and REQ timeouts opportunistic |

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
| ~~M4b Python Hello~~ | **Done** — `hello_client.py`, offline `test_frame_envelope.py`, Qt harness `mps_tests_m4b_hello` |
| M5 tear-out / merge | **Closable** (G1/G4 fixed; G2/G3 Demo Client no-op) — [gap audit](../zh/m5-gap-audit.md) |
| ~~M6 heartbeat / unhealthy UI~~ | **Done** — `heartbeat_policy.hpp`; Client 2s EVT; Host 6s Unhealthy; tab suffix + terminate |
| x11 / inproc / multi-client | `IEmbedBackend` next (`wid` already localized) |
| QThemeEngine in Host | Discuss at integration time — do not pre-refactor now |
