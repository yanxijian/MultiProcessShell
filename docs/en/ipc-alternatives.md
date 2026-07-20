# IPC Alternatives (Future Reference)

> **中文主文档**：[../zh/ipc-alternatives.md](../zh/ipc-alternatives.md)  
> **Status**: Alternatives list — **not** the current implementation.  
> **Current path**: [demo-ipc.md](demo-ipc.md) and spec §5 — **Protobuf + length-prefixed Envelope + Named Pipe / UDS**.

This note absorbs conclusions from a six-platform (desktop + mobile) C++ IPC guide and adds options more relevant to **MultiProcessShell** (chrome + embed).  
**Do not treat this document as a replacement for the frozen Demo / `.proto` contract.**

---

## 1. Current path (reminder)

| Item | Choice |
|------|--------|
| Serialization / IDL | Protobuf (`shell.ipc.v1`) |
| Framing | `uint32` BE length + `Envelope` |
| Transport | Windows Named Pipe; macOS/Linux UDS; Host uses `QLocalSocket` as a byte stream |
| Out of scope (this phase) | gRPC runtime, Thrift, Mojo, JSON as primary protocol |
| Embed | Platform EmbedBackend (**separate** from control-plane IPC) |

---

## 2. Split the scenario first

```text
Who talks to whom?
  ├── Host(C++/Qt) ↔ Client(C++/Qt) shell orchestration
  │     └── Current: Protobuf custom framing (shipped)
  │         Alt: Cap'n Proto (if encode/decode CPU matters)
  ├── C++ engine ↔ native UI (Swift / Kotlin / ArkTS / …)
  │     └── Prefer gRPC (mature cross-language stubs)
  └── Multi-device async state / pub-sub / weak network
        └── Alt: Zenoh; or EVT streams on the existing Envelope
```

Shell control messages are usually small and frequent; embed geometry/handles stay on the platform channel.

---

## 3. Transport consensus (keep)

| Platform | Prefer |
|----------|--------|
| Windows | Named Pipe |
| Linux / macOS / Android | UDS |
| iOS / HarmonyOS Next | Sandbox-allowed UDS (e.g. under App Group) |

Mobile often cannot freely fork processes; background freeze needs reconnect/retry. “Library builds on six platforms” ≠ “shell morphology works on six platforms.”

---

## 4. Upper-layer alternatives

| Option | Good for | Vs this repo | Cost / risk |
|--------|----------|--------------|-------------|
| **Protobuf + custom frames** (current) | Host↔Client control plane; shared IDL | **Primary** | You own timeouts, reconnect, backpressure |
| **gRPC** (same or evolved `.proto`) | C++ engine ↔ multi-language UI | **Future optional** | Heavy deps; Windows often loopback TCP unless bound carefully |
| **Cap'n Proto** | Pure C++↔C++; minimal CPU | Alternative | Weaker cross-language story; **new IDL**, incompatible with `shell.ipc.v1` |
| **Zenoh** | Pub/sub, weak network, multi-device | Alternative (data plane) | Poor fit as the only Tab lifecycle RPC |
| **FlatBuffers / zero-copy** | Large read-only snapshots | Local alt | Little win for small control messages; avoid dual primary IDL |
| **nng / similar** | Ready-made patterns | Alternative | Still need a payload contract |
| **OS buses** (e.g. D-Bus) | Desktop system integration | Glue only | Not a cross-Win/Mac shell primary |
| **Chromium Mojo** | Strong desktop sandbox / handles | **Not recommended** for small teams | Deep Chromium/GN coupling |
| **Thrift / newline JSON** | — | **Reject** as control primary | Dual IDL / fragile text |

Guide mapping: Cap'n for all-C++; gRPC for cross-language UI; Zenoh for distributed/async — accepted as **optional futures**, not current work.

---

## 5. Extra recommendations

1. Keep **control vs embed** channels separate after any stack change.  
2. Prefer fixing session/lifecycle on the current Envelope before switching libraries.  
3. If adopting gRPC: bind UDS/Pipe on localhost; keep token/ACL.  
4. If adopting Cap'n: POC with fixed shell scenarios before committing.  
5. Treat mobile as a **separate morphology** doc, not a drop-in of desktop `QProcess`.  
6. Optional upgrades **on the current path**: backpressure, stronger idempotency, SHM/file mapping for large payloads with RPC carrying only tokens.

---

## 6. When to reopen selection

- Multi-language native UI becomes the main client surface.  
- Profiling shows serialization dominates shell IPC CPU.  
- Product needs multi-device / weak-network collaboration beyond EVT.  
- Binary size/compliance cannot keep the current protobuf build approach (try prebuilt/trim first).

---

## 7. Related

| Doc | Role |
|-----|------|
| [demo-ipc.md](demo-ipc.md) | **Current** Demo contract |
| [multiprocess-shell-spec.md](multiprocess-shell-spec.md) §5 | Product IPC + primary path |
| `proto/shell/ipc/v1/ipc.proto` | Current IDL |
