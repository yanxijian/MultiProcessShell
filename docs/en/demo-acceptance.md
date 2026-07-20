# Demo acceptance checklist (Windows form A)

> **中文主文档**：[../zh/demo-acceptance.md](../zh/demo-acceptance.md)  
> **Companion**: [demo-morphology.md](demo-morphology.md), [demo-ipc.md](demo-ipc.md)  
> **Run**: `dist/Demo/mps_demo_host.exe` (build/deploy per [build.md](build.md))

Manual regression. Finish this list before later polish (tab overflow, REQ timeout, build-script ergonomics).

## Environment

- [ ] Double-click Host starts; no extra console window
- [ ] Startup shows one shell + **Home** tab

## Create / titles

- [ ] Home → Create Client → `Client1-Window1` embeds and fills the client area
- [ ] Client “新建窗口” → `Client1-Window2` (same process, new tab)
- [ ] Home → Create Client again → `Client2-Window1` (distinct accent OK)

## Close tab / MRU

- [ ] Activate A→B→C, close C → returns to B (not forced Home)
- [ ] After closing all tabs of a Client, Home Create Client still works

## Same-shell reorder

- [ ] Drag a Client tab within one shell; blue insert marker appears
- [ ] Drop changes tab order; switching tabs still shows correct embeds
- [ ] Home stays leftmost; cannot insert before Home

## Tear-out / merge

- [ ] Drag Client tab outside shell → new top-level shell with that tab
- [ ] If source has no remaining Client tabs (Home only) and is not the last shell → source destroyed
- [ ] Drop onto another shell’s title/tab strip → merge at insert marker; source rules as above
- [ ] After tear-out/merge, embed fills area, shell focuses, no long black/empty flash

## Close shell / kill process

- [ ] Click × on a spare shell → shell closes; its Client tabs leave UI; Host stays up
- [ ] Close the **last** top-level shell → Demo exits
- [ ] End a `mps_demo_client` in Task Manager → its tabs disappear; Host stays up; other Clients OK

## Smoke stability

- [ ] Sequence: two Clients → two windows each → reorder → tear out → merge back → close tabs → close shell — no crash
