# Demo IPC 合约（定稿）

> **English**：[../en/demo-ipc.md](../en/demo-ipc.md)  
> 与 [demo-morphology.md](demo-morphology.md) 配套。控制面：`shell.ipc.v1` + 长度前缀 `Envelope`。  
> **权威 IDL**：仓库 `proto/shell/ipc/v1/ipc.proto`（与产品长文规格中的 IDL 草图冲突时，以本文件 + `.proto` 为准）。  
> 远期其他 IPC 栈（gRPC / Cap'n Proto / Zenoh 等）见 [ipc-alternatives.md](ipc-alternatives.md)（备选，不改变本文合约）。

## 1. 决议摘要

| # | 决议 |
|---|------|
| 1 | 精简命令 + **标题方案 A**（Host 生成 `Kind-File{M}`（如 Demo-File1），经 `CreateSubWindow` 下发） |
| 2 | **不要** `ApplicationConnected` |
| 3 | **要** `NotifyMainWindowReattachment` |
| 4 | IPC **双向**：框架预留 `Invoke` / `InvokeResult` |
| 5 | **心跳**纳入协议；Demo **已启用**周期 Heartbeat（2s / 超时 6s → Tab「无响应」；不自动杀进程） |

## 2. 帧与方向

`Envelope`：`protocol`, `id`, `dir` (`REQ`/`RES`/`EVT`), `session_id`, `tab_id`, `ts_ms`, `oneof body`。

| dir | 含义 |
|-----|------|
| `REQ` | 需要对端 `RES`（同一 `id`）；**必须超时** |
| `RES` | 对某 `REQ` 的应答；失败时 body 为 `RpcError` |
| `EVT` | 单向通知 |

## 3. 首 Demo 命令（精简）

### 3.1 Client → Host

| 命令 | dir | 说明 |
|------|-----|------|
| `Hello` | EVT | `pid`, `app_name`, `caps` |
| `MainWindowAdded` | EVT | `wid`, `pid` |
| `SubWindowAdded` | EVT | 回传 `title`（与 Host 下发一致）；ContentView 就绪 |
| `SubWindowRemoved` | EVT | ContentView 已关 |
| `Invoke` | REQ | Demo：`demo.request_new_window` 请求再建 ContentView |
| `Heartbeat` | EVT | Demo 已启用（见 §6） |

### 3.2 Host → Client

| 命令 | dir | 说明 |
|------|-----|------|
| `HelloAck` | EVT | `session_id`, `protocol`, `host_caps` |
| `CreateSubWindow` | REQ | 已分配 `tab_id` + **`title`** |
| `ActiveSubWindow` | EVT | 激活对应 ContentView |
| `QueryCloseSubWindow` | REQ | 关 Tab；Demo 直接同意并关 ContentView |
| `QueryCloseSubWindowResult` | RES | `accept` |
| `NotifyMainWindowReattachment` | EVT | 壳变更 / 即将 reparent |
| `SetDragSuppress` | EVT | 拖出期间抑制改窗 |
| `InvokeResult` / `RpcError` | RES | 对 Client `Invoke` 的应答 |
| `Ping` / `Pong` | — | 协议预留 |

### 3.3 不做进首 Demo 业务

`ApplicationConnected`、模态上报、复杂合入权限协商、多语言 EmbedHelper 等。

拖出/合入：**Host Tab 模型 + embed reattach**；不在 mime 传 HWND。

## 4. 与 UI 的对应

| UI | Host | IPC |
|----|------|-----|
| Home「创建 Client」 | `QProcess` → `Hello` → `MainWindowAdded` → `CreateSubWindow(title=Demo-File1)` | §3 |
| Client「新建窗口」 | 收到 `Invoke("demo.request_new_window")` → `CreateSubWindow(Demo-FileM)` | Invoke + CreateSubWindow |
| 关 Tab | `QueryCloseSubWindow` → accept → 拆 Tab；Client `SubWindowRemoved` 幂等兜底 | §3 |
| 拖出/合入 | 改归属 + reattach | `SetDragSuppress` + `NotifyMainWindowReattachment` |
| 拖出中点「新建窗口」 | Host **排队** `CreateSubWindow`，`endTabDrag` 后再发（规格 S5） | Invoke 仍立即 ACK |

## 5. 双向交互预留（框架）

```text
Invoke {
  string method = 1;    // e.g. "demo.request_new_window"
  bytes  params = 2;
}
InvokeResult {
  bytes  payload = 1;
}
```

- 未知 `method` → `RpcError(UNIMPLEMENTED` / `NOT_FOUND)`，不得崩进程。  
- Demo 已实现：`demo.request_new_window`（C→H）。  
- 禁止用 `Invoke` 替代嵌入通道。

## 6. 心跳

| 项 | 约定 |
|----|------|
| 协议 | `Heartbeat`（C→H）+ 可选 `Ping`/`Pong` |
| Demo | **已启用**：Client 每 **2s** 发 `Heartbeat`；Host **约 6s** 无心跳 → 会话 Unhealthy |
| 超时策略 | 标记 unhealthy，**不**自动杀进程；用户右键 Tab「终止进程」 |
| 复现 | Host 环境变量 `MPS_CLIENT_NO_HEARTBEAT=1` 或 Client `--no-heartbeat` |

## 7. 标题方案 A

Host 维护 `instance_index` / 每 Client 的 `content_index`，生成 `Kind-File{M}`，写入 `CreateSubWindow.title`；Client 设置窗口标题并在 `SubWindowAdded.title` 回传相同字符串。

## 8. 实现

- IDL：`proto/shell/ipc/v1/ipc.proto`  
- Host：`src/host/`；Client：`src/client/`；Demo 入口：`demos/`  
- **Demo Client 限制（接受）**：收到 `SetDragSuppress` / `NotifyMainWindowReattachment` 后 **no-op**；Host 仍按合约发送。见 [m5-gap-audit.md](m5-gap-audit.md)。
