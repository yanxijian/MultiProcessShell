# M5 gap audit (Tab tear-out / merge)

> **中文主文档**：[../zh/m5-gap-audit.md](../zh/m5-gap-audit.md)

Status: closeout audit of spec §7 / §12.4 M5 vs the Windows Demo.  
Updated: 2026-07-26 — **G1 / G4 fixed**; G2 / G3 documented as accepted Demo Client no-ops.

**Verdict:** Win Demo M5 can close. Tear-out / merge / reattach work; CreateSubWindow is deferred during drag (S5); release over window buttons cancels tear-out. Host still emits `SetDragSuppress` / `NotifyMainWindowReattachment`; Demo Client ignores them by design.

Full tables live in the Chinese document.
