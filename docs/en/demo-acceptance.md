# Demo acceptance checklist (Windows form A)

> **中文主文档**：[../zh/demo-acceptance.md](../zh/demo-acceptance.md)  
> **Companion**: [demo-morphology.md](demo-morphology.md), [demo-ipc.md](demo-ipc.md)  
> **Run**: `build/demos/mps_demo_host.exe` (build/deploy per [build.md](build.md))

Manual regression. Finish this list before later polish (tab overflow, REQ timeout, build-script ergonomics).

## Environment

- [ ] Double-click Host starts; no extra console window
- [ ] Startup shows one shell + **Home** tab

## Create / titles

- [ ] Home → Create Client → `Client1-Window1` embeds and fills the client area
- [ ] Client “新建窗口” → `Client1-Window2` (same process, new tab)
- [ ] Home → Light / Dark → shell chrome + embedded Client Ribbon sync
- [ ] Ribbon Theme Light/Dark → Host + other Client processes switch together
- [ ] Dark first, then Create Client → new process starts Dark
- [ ] Same-Client “New Window” follows without re-toggling
- [ ] Home → Create Client again → `Client2-Window1` (distinct accent OK)

## Close tab / MRU

- [ ] Activate A→B→C, close C → returns to B (not forced Home)
- [ ] Middle-click a Client tab → closes that tab and its Client child window (same as ×)
- [ ] Middle-click Home → no effect
- [ ] After closing all tabs of a Client, Home Create Client still works

## Same-shell reorder

- [ ] Drag a Client tab within one shell: tab ghost follows; siblings **live-yield** (no blue insert bar)
- [ ] Drop changes tab order; switching tabs still shows correct embeds
- [ ] Home stays leftmost; cannot insert before Home
- [ ] Esc cancel snaps the ghost back; order unchanged

## Tear-out / merge

- [ ] Leave the strip past leave slop → window preview appears; its tab bar vertically wraps the tab ghost (ghost on top)
- [ ] Once the preview appears, siblings on the source shell immediately claim the vacated tab slot (before mouse-up)
- [ ] Return near the strip (return hysteresis) → preview hides; strip yield resumes
- [ ] Fast vertical drag keeps wrap alignment between preview and tab ghost
- [ ] Drag Client tab outside → new top-level shell with that tab; no long black/empty flash
- [ ] If source has no remaining Client tabs (Home only) and is not the last shell → source destroyed
- [ ] Drop onto another shell’s tabs / trailing strip → merge (yield shows slot); source rules as above
- [ ] Over min / max / close → forbidden cursor; no merge; **release does not tear-out** (cancel)
- [ ] During tear-out, Client「新建窗口」→ new tab appears only after drag ends (Create queued)
- [ ] After tear-out/merge, embed fills area and shell focuses

## Close shell / kill process

- [ ] Click × on a spare shell → shell closes; its Client tabs leave UI; Host stays up
- [ ] Close the **last** top-level shell → Demo exits
- [ ] End a `mps_demo_client` in Task Manager → its tabs disappear; Host stays up; other Clients OK

## M6 heartbeat / unresponsive

- [ ] Healthy Client: tab title has no「无响应」suffix
- [ ] Start Host with `MPS_CLIENT_NO_HEARTBEAT=1` → within ~6s tabs show「…（无响应）」; Host stays up; no auto-kill
- [ ] Right-click unresponsive tab →「终止进程」→ that session’s tabs disappear
- [ ] Optional: heartbeats resume → suffix clears

## Smoke stability

- [ ] Sequence: two Clients → two windows each → reorder → tear out → merge back → close tabs → close shell — no crash

## M4b (multi-language Hello, optional)

- [ ] `python clients/python/test_frame_envelope.py` passes
- [ ] `ctest -R M4bPythonHello` (or `mps_m4b_python_hello`) passes: Python `Hello(EMBED_NONE)` ↔ `HelloAck`
