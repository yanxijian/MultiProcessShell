# Demo IPC 合约（定稿）

> **English**: [`../en/demo-ipc.md`](../en/demo-ipc.md)  
> 与 [`demo-morphology.md`](demo-morphology.md) 配套。控制面仍为 `shell.ipc.v1` + 长度前缀 `Envelope`。

## 1. 决议摘要

| # | 决议 |
|---|------|
| 1 | 采用精简命令清单 + **标题方案 A**（Host 生成 `Client{N}-Window{M}`，经 `CreateSubWindow` 下发） |
| 2 | **不要** `ApplicationConnected` |
| 3 | **要** `NotifyMainWindowReattachment` |
| 4 | IPC **双向**：初期交互从简，框架预留通用调用通道 |
| 5 | **心跳**纳入协议；实现可与 UI 健康态一并后置，但 `.proto` / 调度器预留 |

## 2. 帧与方向

`Envelope`：`protocol`, `id`, `dir` (`REQ`/`RES`/`EVT`), `page_id`, `tab_id`, `ts_ms`, `oneof body`。

| dir | 含义 |
|-----|------|
| `REQ` | 需要对端 `RES`（带同一 `id`）；**必须超时** |
| `RES` | 对某 `REQ` 的应答；失败时 body 为 `RpcError` |
| `EVT` | 单向通知，无需应答 |

**双向**：Host→Client 与 Client→Host 均可发 `REQ`/`EVT`。具体业务命令见下；通用扩展走 §5。

## 3. 首 Demo 命令（精简）

### 3.1 Client → Host

| 命令 | dir | 说明 |
|------|-----|------|
| `Hello` | EVT 或 REQ | `pid`, `app_name`, `caps`；若 REQ 则等 `HelloAck` |
| `MainWindowAdded` | EVT | `wid`, `pid`（校验后嵌入） |
| `MainWindowDestroyed` | EVT | 主窗销毁 |
| `SubWindowAdded` | EVT | 回传 `title`（与 Host 下发一致） |
| `SubWindowRemoved` | EVT | 子窗已毁 |
| `Heartbeat` | EVT | 周期心跳（协议必有；实现可后置开启） |
| `Invoke`（预留） | REQ | 见 §5，初期可不发 |

### 3.2 Host → Client

| 命令 | dir | 说明 |
|------|-----|------|
| `HelloAck` | RES/EVT | `session_id`, `protocol`, `host_caps` |
| `CreateSubWindow` | REQ | Host 已分配 `tab_id` + **`title`**（方案 A；原名 CreateWindow，因 Win32 宏冲突已改名） |
| `ActiveSubWindow` | REQ/EVT | 激活对应子窗 |
| `QueryCloseSubWindow` | REQ | 关 Tab；Demo 可直接同意 |
| `NotifyMainWindowReattachment` | EVT/REQ | 壳变更 / 即将 reparent |
| `SetDragSuppress` | EVT | 拖出期间抑制改窗 |
| `Heartbeat` 相关 | — | Host 可发 `Ping`（REQ）或仅统计 Client `Heartbeat` |
| `Invoke`（预留） | REQ | 见 §5，初期可不发 |

### 3.3 不做进首 Demo 业务（可后加）

`ApplicationConnected`、模态上报、复杂合入权限协商、多语言 EmbedHelper 等。

拖出/合入：**Host Tab 模型 + embed reattach**；不在 mime 传 HWND。

## 4. 与 UI 的对应

| UI | Host | IPC |
|----|------|-----|
| 「创建 Client」 | `QProcess` 启动 → 等 `Hello` / `MainWindowAdded` → `CreateWindow(title=ClientN-Window1)` | §3 |
| Client「新建窗口」 | 已有 session → `CreateWindow(ClientN-WindowM)` | `CreateWindow` |
| 关 Tab | `QueryCloseSubWindow` → `SubWindowRemoved` | §3 |
| 拖出/合入 | 改归属 + reattach | `SetDragSuppress` + `NotifyMainWindowReattachment` |

## 5. 双向交互预留（框架）

初期可不做复杂业务，但 **Envelope / 分发器必须支持**：

```text
Invoke {
  string method = 1;    // 约定名，如 "demo.ping_ui"
  bytes  params = 2;    // 方法自定义载荷（可再嵌 protobuf）
}
InvokeResult {
  bytes  payload = 1;
}
```

| 方向 | 用途示例（后期） |
|------|------------------|
| C→H `Invoke` | Client 请求壳改标题、弹状态、查询壳几何 |
| H→C `Invoke` | 壳请求 Client 执行业务命令（在固定命令之外） |

约定：

- 未知 `method` → `RpcError(NOT_FOUND)`，不得崩进程。  
- 首 Demo 实现可对 `Invoke` **一律返回未实现**，但链路与超时要通。  
- 禁止用 `Invoke` 替代嵌入通道（`SetParent` 等仍走平台消息）。

## 6. 心跳

| 项 | 约定 |
|----|------|
| 协议 | `Heartbeat`（C→H EVT）+ 可选 `Ping`/`Pong`（H↔C） |
| 默认周期 | 建议 2s（可配置） |
| 实现阶段 | **可后置**到健康 UI 里程碑；M0 `.proto` 与 session 调度器预留钩子 |
| 超时策略 | 与规格一致：标记 unhealthy，默认不自动杀进程 |

## 7. 标题方案 A

Host 维护 `client_index` / 每 Client 的 `window_index`，生成 `Client{N}-Window{M}`，写入 `CreateWindow.title`；Client 设置窗口标题并在 `SubWindowAdded.title` 回传相同字符串。

## 8. 实现提示

- `.proto` 见仓库 `proto/shell/ipc/v1/ipc.proto`。  
- 业务代码仍待开工指令；本合约已冻结，可供 M0 生成与单测。
