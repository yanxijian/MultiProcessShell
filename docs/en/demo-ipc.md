# Demo IPC Contract (Final)

> **中文主文档**: [`../zh/demo-ipc.md`](../zh/demo-ipc.md)  
> Companion to [demo-morphology.md](demo-morphology.md). Control plane: `shell.ipc.v1` + length-prefixed `Envelope`.  
> **Authoritative IDL**: `proto/shell/ipc/v1/ipc.proto` (prefer this file + `.proto` over older sketches in the long-form product spec).  
> Future alternate stacks (gRPC / Cap'n Proto / Zenoh, …): [ipc-alternatives.md](ipc-alternatives.md) (reference only; does not change this contract).

## 1. Decisions

| # | Decision |
|---|----------|
| 1 | Minimal commands + **title scheme A** (Host assigns `Kind-File{M}` (e.g. Demo-File1) via `CreateSubWindow`) |
| 2 | **No** `ApplicationConnected` |
| 3 | **Yes** `NotifyMainWindowReattachment` |
| 4 | Bidirectional IPC; reserve `Invoke` / `InvokeResult` |
| 5 | Heartbeat in the protocol; Demo **enables** periodic Heartbeat (2s / ~6s timeout → Tab「无响应」; no auto-kill) |

## 2. Framing and direction

`Envelope`: `protocol`, `id`, `dir` (`REQ`/`RES`/`EVT`), `session_id`, `tab_id`, `ts_ms`, `oneof body`.

| dir | Meaning |
|-----|---------|
| `REQ` | Needs peer `RES` (same `id`); **must time out** |
| `RES` | Reply to a `REQ`; failures use `RpcError` body |
| `EVT` | One-way notification |

## 3. First Demo commands (minimal)

### 3.1 Client → Host

| Command | dir | Notes |
|---------|-----|-------|
| `Hello` | EVT | `pid`, `app_name`, `caps` |
| `MainWindowAdded` | EVT | `wid`, `pid` |
| `SubWindowAdded` | EVT | Echo `title` (same as Host-assigned) |
| `SubWindowRemoved` | EVT | Child destroyed |
| `Invoke` | REQ | Demo: `demo.request_new_window` |
| `Heartbeat` | EVT | Client→Host every 2s when negotiated |

### 3.2 Host → Client

| Command | dir | Notes |
|---------|-----|-------|
| `HelloAck` | EVT | `session_id`, `protocol`, `host_caps` |
| `CreateSubWindow` | REQ | Host-assigned `tab_id` + **`title`** |
| `ActiveSubWindow` | EVT | Activate child |
| `QueryCloseSubWindow` | REQ | Close tab; Demo accepts immediately |
| `QueryCloseSubWindowResult` | RES | `accept` |
| `NotifyMainWindowReattachment` | EVT | Shell change / about to reparent |
| `SetDragSuppress` | EVT | Suppress window changes during tear-out |
| `InvokeResult` / `RpcError` | RES | Reply to Client `Invoke` |
| `Ping` / `Pong` | — | Protocol reserved |

### 3.3 Out of first Demo

`ApplicationConnected`, modal reporting, complex merge ACLs, multi-language EmbedHelper, etc.

Tear-out/merge: **Host tab model + embed reattach**; no HWND in mime.

## 4. UI mapping

| UI | Host | IPC |
|----|------|-----|
| Home **Create Client** | `QProcess` → `Hello` → `MainWindowAdded` → `CreateSubWindow(title=Demo-File1)` | §3 |
| Client **新建窗口** | `Invoke("demo.request_new_window")` → `CreateSubWindow(Demo-FileM)` | Invoke + CreateSubWindow |
| Close tab | `QueryCloseSubWindow` → accept → remove tab; Client `SubWindowRemoved` idempotent backup | §3 |
| Tear-out / merge | Reassign + reattach | `SetDragSuppress` + `NotifyMainWindowReattachment` |
| 「新建窗口」during tear-out | Host **queues** `CreateSubWindow` until `endTabDrag` (spec S5) | Invoke still ACKs immediately |

## 5. Bidirectional reserve

```text
Invoke {
  string method = 1;    // e.g. "demo.request_new_window"
  bytes  params = 2;
}
InvokeResult {
  bytes  payload = 1;
}
```

- Unknown `method` → `RpcError(UNIMPLEMENTED` / `NOT_FOUND)`; must not crash.  
- Demo implements: `demo.request_new_window` (C→H).  
- Do not use `Invoke` for the embed channel.

## 6. Heartbeat

| Item | Rule |
|------|------|
| Protocol | `Heartbeat` (C→H) + optional `Ping`/`Pong` |
| Demo | **On**: Client every **2s**; Host marks Unhealthy after ~**6s** silence |
| Timeout | Mark unhealthy; **no** auto-kill; right-click Tab →「终止进程」 |
| Repro | Host env `MPS_CLIENT_NO_HEARTBEAT=1` or Client `--no-heartbeat` |

## 7. Title scheme A

Host keeps `instance_index` / per-Client `content_index`, writes `Kind-File{M}` into `CreateSubWindow.title`; Client sets the window title and echoes it in `SubWindowAdded.title`.

## 8. Implementation

- IDL: `proto/shell/ipc/v1/ipc.proto`  
- Host: `src/host/`; Client: `src/client/`; Demo entry: `demos/`  
- **Demo Client limit (accepted):** `SetDragSuppress` / `NotifyMainWindowReattachment` are **no-ops** on the Client; Host still emits them. See [m5-gap-audit.md](../zh/m5-gap-audit.md).
