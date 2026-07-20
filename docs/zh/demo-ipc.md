# Demo IPC 合约（定稿）

> **English**：[../en/demo-ipc.md](../en/demo-ipc.md)  
> 与 [demo-morphology.md](demo-morphology.md) 配套。控制面：`shell.ipc.v1` + 长度前缀 `Envelope`。  
> **权威 IDL**：仓库 `proto/shell/ipc/v1/ipc.proto`（与产品长文规格中的 IDL 草图冲突时，以本文件 + `.proto` 为准）。  
> 远期其他 IPC 栈（gRPC / Cap'n Proto / Zenoh 等）见 [ipc-alternatives.md](ipc-alternatives.md)（备选，不改变本文合约）。

## 1. 决议摘要

| # | 决议 |
|---|------|
| 1 | 精简命令 + **标题方案 A**（Host 生成 `Client{N}-Window{M}`，经 `CreateSubWindow` 下发） |
| 2 | **不要** `ApplicationConnected` |
| 3 | **要** `NotifyMainWindowReattachment` |
| 4 | IPC **双向**：框架预留 `Invoke` / `InvokeResult` |
| 5 | **心跳**纳入协议；Demo **未开**周期定时器（caps 可协商；实现后置） |

## 2. 帧与方向

`Envelope`：`protocol`, `id`, `dir` (`REQ`/`RES`/`EVT`), `page_id`, `tab_id`, `ts_ms`, `oneof body`。

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
| `SubWindowAdded` | EVT | 回传 `title`（与 Host 下发一致） |
| `SubWindowRemoved` | EVT | 子窗已毁 |
| `Invoke` | REQ | Demo：`demo.request_new_window` 请求再建子窗 |
| `Heartbeat` | EVT | 协议预留；Demo 默认不发 |

### 3.2 Host → Client

| 命令 | dir | 说明 |
|------|-----|------|
| `HelloAck` | EVT | `session_id`, `protocol`, `host_caps` |
| `CreateSubWindow` | REQ | 已分配 `tab_id` + **`title`**（原名 CreateWindow，因 Win32 宏冲突改名） |
| `ActiveSubWindow` | EVT | 激活对应子窗 |
| `QueryCloseSubWindow` | REQ | 关 Tab；Demo 直接同意并关窗 |
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
| Home「创建 Client」 | `QProcess` → `Hello` → `MainWindowAdded` → `CreateSubWindow(title=ClientN-Window1)` | §3 |
| Client「新建窗口」 | 收到 `Invoke("demo.request_new_window")` → `CreateSubWindow(ClientN-WindowM)` | Invoke + CreateSubWindow |
| 关 Tab | `QueryCloseSubWindow` → accept → 拆 Tab；Client `SubWindowRemoved` 幂等兜底 | §3 |
| 拖出/合入 | 改归属 + reattach | `SetDragSuppress` + `NotifyMainWindowReattachment` |

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
| Demo | **未启用**周期心跳；Hello caps 可声明 `heartbeat` |
| 超时策略 | 与规格一致：标记 unhealthy，默认不自动杀进程 |

## 7. 标题方案 A

Host 维护 `client_index` / 每 Client 的 `window_index`，生成 `Client{N}-Window{M}`，写入 `CreateSubWindow.title`；Client 设置窗口标题并在 `SubWindowAdded.title` 回传相同字符串。

## 8. 实现

- IDL：`proto/shell/ipc/v1/ipc.proto`  
- Host：`src/host/`；Client：`src/client/`；Demo 入口：`demos/`
