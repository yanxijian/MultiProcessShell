# Demo IPC Contract (Final)

> **Chinese**: [`../zh/demo-ipc.md`](../zh/demo-ipc.md)  
> Companion to [`demo-morphology.md`](demo-morphology.md).

## Decisions

| # | Decision |
|---|----------|
| 1 | Minimal command set + **title scheme A** (Host assigns `Client{N}-Window{M}` via `CreateWindow`) |
| 2 | **No** `ApplicationConnected` |
| 3 | **Yes** `NotifyMainWindowReattachment` |
| 4 | **Bidirectional** IPC; keep early UX simple; reserve a generic `Invoke` channel |
| 5 | **Heartbeat** in the protocol; implementation may ship later with health UI |

## Demo commands (minimal)

**C→H:** `Hello`, `MainWindowAdded`, `MainWindowDestroyed`, `SubWindowAdded`, `SubWindowRemoved`, `Heartbeat` (proto now; runtime optional later), `Invoke` (reserved)

**H→C:** `HelloAck`, `CreateWindow` (+ `title`), `ActiveSubWindow`, `QueryCloseSubWindow`, `NotifyMainWindowReattachment`, `SetDragSuppress`, optional `Ping`, `Invoke` (reserved)

Tear-out/merge stay on the Host tab model + embed reattach (no HWND in DnD mime).

## Bidirectional reserve

```text
Invoke { string method; bytes params; }
InvokeResult { bytes payload; }
```

Either side may send `Invoke` as `REQ`. Unknown methods → `RpcError(NOT_FOUND)`. First Demo may stub `Invoke` as not-implemented but must route with timeouts. Do not use `Invoke` for `SetParent`.

## Heartbeat

`Heartbeat` (C→H) and optional `Ping`/`Pong`. Suggested period 2s. Runtime enablement may follow health UI; hooks exist from M0.

## Title scheme A

Host generates `Client{N}-Window{M}` and sends it in `CreateWindow.title`; Client echoes it in `SubWindowAdded.title`.
