# Demo Morphology — Minimum Window Embed

> **Status**: Discussion draft (`demos/` has no implementation yet)  
> **Chinese**: [`../zh/demo-morphology.md`](../zh/demo-morphology.md)

## Goal

The first `demos/` deliverable proves **one** thing: a Host container can embed another process’s native window on **Windows (form A)**, with a minimal Protobuf handshake.

It is **not** the full tab tear-out / multi-type acceptance suite.

## Proposed minimum shape (TBD)

```text
demo_host (Qt)          Pipe+Envelope         demo_client (Qt)
QMainWindow + native    <------------------>  embeddable top-level
EmbedContainer                                solid-color widget
SetParent(client HWND)                        readable label
```

### Suggested acceptance

1. Host launches (or attaches) Client.  
2. `Hello` → `HelloAck` → `MainWindowAdded(wid)`.  
3. Host `SetParent`s Client into the container; resize follows.  
4. Killing Client does not crash Host; UI shows disconnected.

### Out of scope for the first Demo

TabBar, tear-out, multi Client types, macOS/Linux embed.

## Open questions

1. Host `QProcess` spawn vs manual attach?  
2. IPC: handshake + `MainWindowAdded` only, or also `CreateWindow`?  
3. Solid-color Client enough, or a button proving bidirectional IPC?  
4. Link `src/` early vs keep Demo self-contained then lift?  
5. Embed-failure UX (message / retry)?

Finalize here and in `demos/README.md` before coding.
