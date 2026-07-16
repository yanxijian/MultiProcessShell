# Demo IPC Contract (Final)

> **Chinese**: [`../zh/demo-ipc.md`](../zh/demo-ipc.md)  
> Companion to [`demo-morphology.md`](demo-morphology.md).  
> **Authoritative IDL**: `proto/shell/ipc/v1/ipc.proto`. Prefer this doc + `.proto` over older sketches in the long-form product spec.

## Decisions

| # | Decision |
|---|----------|
| 1 | Minimal command set + **title scheme A** via `CreateSubWindow` |
| 2 | **No** `ApplicationConnected` |
| 3 | **Yes** `NotifyMainWindowReattachment` |
| 4 | Bidirectional IPC; `Invoke` / `InvokeResult` reserved (Demo uses `demo.request_new_window`) |
| 5 | Heartbeat in the protocol; **Demo does not run a timer yet** |

## Demo commands (minimal)

**C→H:** `Hello`, `MainWindowAdded`, `SubWindowAdded`, `SubWindowRemoved`, `Invoke` (`demo.request_new_window`), `Heartbeat` (proto only)

**H→C:** `HelloAck`, `CreateSubWindow` (+ `title`; renamed from CreateWindow for Win32), `ActiveSubWindow`, `QueryCloseSubWindow` / `QueryCloseSubWindowResult`, `NotifyMainWindowReattachment`, `SetDragSuppress`, optional `Ping`/`Pong`

Tear-out/merge stay on the Host tab model + embed reattach (no HWND in DnD mime).

## UI mapping

| UI | IPC |
|----|-----|
| Home **Create Client** | spawn → Hello → MainWindowAdded → `CreateSubWindow` |
| Client **New Window** | `Invoke("demo.request_new_window")` → Host `CreateSubWindow` |
| Close tab | `QueryCloseSubWindow` → accept → Host removes tab; `SubWindowRemoved` idempotent |

## Bidirectional reserve

```text
Invoke { string method; bytes params; }
InvokeResult { bytes payload; }
```

Unknown methods → `RpcError`. Do not use `Invoke` for `SetParent`.

## Heartbeat

Protocol has `Heartbeat` / `Ping` / `Pong`. Demo negotiates caps but does not send periodic heartbeats yet.

## Title scheme A

Host generates `Client{N}-Window{M}` in `CreateSubWindow.title`; Client echoes it in `SubWindowAdded.title`.
