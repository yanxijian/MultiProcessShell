# Demo 形态（定稿）

> **英文文档**：[../en/demo-morphology.md](../en/demo-morphology.md)  
> **状态**：已定稿（与当前仓库 Demo 实现一致）

## 1. 产品直觉

交互对标 **Chrome 多窗口 / 多标签**：

| 能力 | 行为 |
|------|------|
| 并列 | 不同 Client 进程的窗口以 **Tab** 并列在同一顶层壳窗 |
| 关闭 | 可单独关闭某个 Client Tab；关闭后按 **激活历史（MRU）** 切回上一 Tab |
| 拖出 | 将 Client Tab **拖出** 成新的顶层壳窗 |
| 合入 | 将 Tab **拖入** 另一顶层壳窗的 Tab 栏 |
| 空壳销毁 | 若某壳已无剩余 **Client Tab**（仅剩 Home），且不是唯一顶层壳 → **销毁该壳** |

首期平台：**Windows（形态 A）**。

## 2. 启动与创建流程

### 2.1 启动

- 启动 Demo 后 **只有一个** 顶层壳窗。  
- 壳上有一个固定 **Home** Tab（不可关闭、不可拖出）。  
- **Home** 客户区中央为「创建 Client」；**不**自动拉起任何 Client 进程。

### 2.2 创建 Client（新进程 / 新 pageType 会话）

- 在 **Home** 点击「创建 Client」→ 启动一个 Client 进程，并在当前壳新增一个 Client Tab。  
- 首个该 Client 下的子窗口标题形如：`Client1-Window1`。  
- 需要再建 Client 时，切回 **Home** 再点「创建 Client」。

### 2.3 同 Client 再建子窗口

- **Client 窗口（page）客户区中央** 有按钮「新建窗口」。  
- 点击后经 IPC `Invoke("demo.request_new_window")`，由 Host 再发 `CreateSubWindow`，在 **同一 Client 进程** 内再建子窗口并新增 Tab。  
- 标题递增：`Client1-Window2`、`Client1-Window3`、…

### 2.4 多 Client

- 再次在 Home「创建 Client」→ `Client2-Window1`、…  
- 不同 Client = 不同进程（形态 A）；Tab 可同壳并列（示意上可用不同强调色区分）。

## 3. 标题（Tab 名）规则

格式：

```text
Client{N}-Window{M}
```

| 字段 | 含义 | 递增规则 |
|------|------|----------|
| `N` | Client 实例序号 | 每成功创建一个新 Client 进程 +1（全局，按 Demo 会话） |
| `M` | 该 Client 内子窗口序号 | 每在该 Client 内新建一个子窗口 +1 |

示例：`Client1-Window1`、`Client1-Window2`、`Client2-Window1`。  
**Home** Tab 标题固定为 `Home`（不属于上述命名）。

## 4. 窗口结构

```text
┌─ Shell 顶层窗（自定义标题栏）──────────────────────────┐
│  [Home] [Client1-Window1 ×] [Client2-Window1 ×]  _ □ × │
├────────────────────────────────────────────────────────┤
│  Home 激活：中央「创建 Client」                          │
│  Client Tab 激活：嵌入该 Client 原生窗（内有「新建窗口」） │
└────────────────────────────────────────────────────────┘
```

- **标题栏**：Home + Client Tabs + 最小化 / 最大化 / 关闭。  
- **工作区**：Home 页或当前 Client 嵌入区（应铺满客户区）。

## 5. 拖出 / 合入（Chrome 式）

1. 按住 **Client** Tab 拖出壳外 → 释放后 **新建** 一个顶层壳（含 Home + 该 Tab）。  
2. 若原壳因此 **没有剩余 Client Tab**（仅剩 Home）且不是唯一壳 → **销毁原顶层壳**。  
3. 拖到另一壳的 Tab 栏 → 合入。  
4. DnD **只改 Host 侧模型**，禁止在 mime 中传 HWND；合入/拖出后再 reattach。  
5. **Home** 不可拖出 / 合入。

## 6. 关 Tab 与激活链

- 关闭 Client Tab → Host `QueryCloseSubWindow` → Client 同意并 `SubWindowRemoved`（Host 在 accept 时即可拆 Tab）。  
- 激活链（MRU）包含 **Home 与 Client Tabs**；关当前 Tab 时切到历史上一个仍存在的 Tab（不强制回 Home）。

## 7. 首期范围边界

### 7.1 本 Demo 要做（已实现）

- 单实例启动 → 一壳 + **Home** +「创建 Client」  
- 多 Client 进程 + 同 Client 多子窗 + Tab 名递增  
- 关 Tab（激活历史）、拖出新壳、合入他壳、无 Client Tab 时毁多余壳  
- Windows `SetParent` 嵌入 + Protobuf Protobuf 合约  

### 7.2 可后置或简化

- 精致拖出跟手动画  
- 心跳定时器 / 无响应 UI（协议已预留）  
- macOS / Linux 嵌入  
- 完整 `EmbedHelper` 多语言  

## 8. 与仓库目录

| 路径 | 角色 |
|------|------|
| `demos/` | `mps_demo_host` / `mps_demo_client` |
| `src/host/` | 壳、Tab、会话、Win embed |
| `src/client/` | Client 进程与 page |
| `src/common/` + `proto/` | 帧与 IDL |
| `dist/Demo/` | Windows 可双击运行包（生成物） |

## 9. 已决议摘要

| # | 决议 |
|---|------|
| 1 | 交互对标 Chrome：并列 Tab、关 Tab、拖出新壳、合入他壳 |
| 2 | 无剩余 Client Tab 的多余壳 → 销毁 |
| 3 | 启动一壳 + 固定 **Home**；Create Client 在 Home |
| 4 | Client page「新建窗口」→ 同 Client 子窗 |
| 5 | Tab 名：`Client{N}-Window{M}`；Home 除外 |
| 6 | 关 Tab 走激活历史（含 Home），默认不强制回 Home |

## 10. IPC

见 [demo-ipc.md](demo-ipc.md)。
