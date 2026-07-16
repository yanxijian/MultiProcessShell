# MultiProcessShell — Technical Specification (v2)

> **Chinese (primary)**: [`../zh/multiprocess-shell-spec.md`](../zh/multiprocess-shell-spec.md)  
> This English file is a **summary companion**. For full defect matrices, state machines, and appendices, read the Chinese document.  
>
> **Date**: 2026-07-16  
> **Status**: Spec v2  
> **Project**: `MultiProcessShell`  
> **Platforms**: Windows, macOS, Linux (X11; Wayland — see Chinese appendix B)  
> **IPC**: Control plane = `shell.ipc.v1` Protobuf + Pipe/UDS + length-prefixed frames; embed plane = EmbedBackend / EmbedHelper. Out of scope for this phase: gRPC, Thrift, Mojo, JSON as the primary control protocol.  
> **Demo**: See [demo-morphology.md](demo-morphology.md) and [demo-ipc.md](demo-ipc.md). Checked-in IDL is `proto/shell/ipc/v1/ipc.proto`. Prefer `.proto` + Demo IPC over any older sketches in the Chinese long-form IDL sections when they diverge.

---

## 1. Goals

| Goal | Meaning |
|------|---------|
| Process isolation | Different business types must not crash each other (deployment form **A**) |
| Unified chrome | Shared title bar / tabs / look |
| Multi-window tabs | Browser-like switching of business windows |
| Cross-type same shell | Different Client types share one shell window |
| Tear-out / merge | Tabs tear out to a new shell; merge back or into another shell |

**Non-goals (this phase):** concrete business features; Wayland-native embed; cross-machine remote embed; full sandboxing; full a11y.

## 2. Deployment forms

| Form | Meaning | Primary use |
|------|---------|-------------|
| **A** | True multi-process + native reparent | Windows, Linux/X11 |
| **B** | No dedicated shell (each app has its own frame) | Optional / macOS fallback |
| **C** | In-process modules + `QWidget::setParent` / layout | **macOS primary** |
| **D** | Mixed | As needed |

**Phase-1 target:** Windows form **A** first; macOS/Linux directories are placeholders.

## 3. Roles

- **Host (shell):** chrome, tabs, Client lifecycle, EmbedBackend, IPC server (C++/Qt).  
- **Client:** owns real content windows; one process (or module) per `pageType` by default.  
- **Stable IDs:** `page_id` / `tab_id` assigned by Host; `wid` is an embed credential only.

Object tree (logical):

```text
ShellApp → ShellMainWindow × N
  → TabBar / ClientPage × P
    → EmbedContainer + ClientTab × T
```

## 4. Dual channels

| Channel | Responsibility |
|---------|----------------|
| Protobuf IPC | Intent & state: handshake, create/close, titles, modal, heartbeat, tear-out orchestration |
| Platform channel | Embed geometry/focus: `WM_*` / XEmbed / in-process `QEvent` |

## 5. IPC (Protobuf)

- **IDL:** `proto/shell/ipc/v1/` (`shell.ipc.v1`)  
- **Transport:** Named Pipe (Windows) / UDS (macOS/Linux); Host uses `QLocalSocket` as a byte stream  
- **Framing:** `uint32` big-endian length + `Envelope` protobuf  
- **In-process (form C):** same `Envelope`, `InProcessTransport`  
- **Dependency:** CMake `FetchContent` for protobuf  
- **Multi-language smoke (M4b):** Python  

Envelope fields (conceptual): `protocol`, `id`, `dir` (REQ/RES/EVT), `page_id`, `tab_id`, `ts_ms`, `oneof body`.

Required capability negotiation via `Hello` / `HelloAck` (`Capabilities.embed`, `tab_drag`, `heartbeat`, …).

Heartbeat: Client → Host ~2s; Host marks unhealthy after timeout; UI offers terminate (no auto-kill by default).

## 6. Embed backends

| Backend | Platform |
|---------|----------|
| `WinHwndEmbedBackend` | Windows `SetParent` + custom `WM_*` |
| `X11XEmbedBackend` | Linux/X11 (placeholder this phase) |
| `InProcessWidgetBackend` | macOS form C (placeholder this phase) |

Validate `MainWindowAdded.wid` against Client PID before embed.

## 7. Tech baseline

| Item | Decision |
|------|----------|
| License | MIT |
| UI | Open-source Qt **6.8+** (compat baseline **6.8 LTS**) |
| Env | `QTDIR`; projects use `%QTDIR%` / `$env:QTDIR` |
| PATH | `%QTDIR%\bin` |
| CMake | `CMAKE_PREFIX_PATH=$QTDIR`; `find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)` |
| Modules | Core, Gui, Widgets, Network |
| Build | CMake + `protoc` (FetchContent) |

## 8. Repository layout

See root `README.md`. Phase-1 implements Windows paths under `src/host/embed/win`; `x11` / `inproc` are placeholders.

## 9. Demos vs milestones

- **`demos/`:** Chrome-like Demo (`mps_demo_host` / `mps_demo_client`). Morphology finalized — see [`demo-morphology.md`](demo-morphology.md) / [`../zh/demo-morphology.md`](../zh/demo-morphology.md).  
- **Full milestone suite** (tabs, tear-out, multi-type, crash): later acceptance apps, not necessarily the first Demo.

## 10. Milestones (summary)

| ID | Focus |
|----|--------|
| M0 | Freeze `.proto` + framing unit tests |
| M1 | In-process / local embed experiment |
| M2 | Cross-process embed (Windows) |
| M3–M6 | Multi-subwindow tabs, cross-type, tear-out, health |
| M4b | Python `Hello` smoke |
| M7 | Optional security baseline |

## 11. Open product questions

Defaults live in the Chinese spec §15 (auto-kill policy, empty shell close, Wayland roadmap, etc.).

---

*End of English spec v2 (companion).*
