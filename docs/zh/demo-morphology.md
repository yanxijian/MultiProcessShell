# Demo 形态（定稿）

> **状态**：已定稿  
> **English**: [`../en/demo-morphology.md`](../en/demo-morphology.md)  
> **示意图**：产品侧 Chrome 式多进程 Tab 壳（自定义顶栏 + 客户区嵌入）

## 1. 产品直觉

交互对标 **Chrome 多窗口 / 多标签**：

| 能力 | 行为 |
|------|------|
| 并列 | 不同 Client 进程的窗口以 **Tab** 并列在同一顶层壳窗 |
| 关闭 | 可单独关闭某个 Tab（对应子窗口 / page） |
| 拖出 | 将 Tab **拖出** 成新的顶层壳窗 |
| 合入 | 将 Tab **拖入** 另一顶层壳窗的 Tab 栏 |
| 空壳销毁 | 若拖出的是该壳的 **最后一个 Tab**，原顶层壳窗 **直接销毁** |

首期平台：**Windows（形态 A）**。

## 2. 启动与创建流程

### 2.1 启动

- 启动 Demo 后 **只有一个** 顶层壳窗。  
- 客户区中央有一个按钮（例如「创建 Client」）。  
- **尚未** 自动拉起任何 Client 进程。

### 2.2 创建 Client（新进程 / 新 pageType 会话）

- 点击壳上的「创建 Client」→ 启动（或复用策略见下）一个 Client 进程，并在当前壳新增一个 Tab。  
- 首个该 Client 下的子窗口标题形如：`Client1-Window1`。

### 2.3 同 Client 再建子窗口

- **Client 窗口（page）客户区中央** 也有一个按钮（例如「新建窗口」）。  
- 点击后在 **同一 Client 进程** 内再建一个子窗口，并在壳上新增 Tab。  
- 标题递增：`Client1-Window2`、`Client1-Window3`、…

### 2.4 多 Client

- 再次点击壳上的「创建 Client」→ 新的 Client 序号：`Client2-Window1`、…  
- 不同 Client = 不同进程（形态 A）；Tab 可同壳并列（示意上可用不同强调色区分类型，非必须）。

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

## 4. 窗口结构（与示意图一致）

```text
┌─ Shell 顶层窗（自定义标题栏）─────────────────────┐
│  [Client1-Window1 ×] [Client2-Window1 ×]   _ □ × │  ← Tab + 系统按钮
├──────────────────────────────────────────────────┤
│                                                    │
│              当前 Tab 的嵌入客户区                  │
│         （无 Tab 时：中央「创建 Client」）            │
│         （有 Client page 时：中央「新建窗口」）        │
│                                                    │
└────────────────────────────────────────────────────┘
```

- **黄区（示意）**：自定义标题栏（Tab + 最小化 / 最大化 / 关闭）。  
- **蓝区（示意）**：嵌入区；显示当前激活 Tab 对应的 Client 原生窗口。

## 5. 拖出 / 合入（Chrome 式）

1. 按住 Tab 拖出壳外 → 释放后 **新建** 一个顶层壳，并挂上该 Tab。  
2. 若原壳因此 **没有剩余 Tab** → **销毁原顶层壳**。  
3. 拖到另一壳的 Tab 栏 → 合入；插入位置与指示器一致（实现阶段细化）。  
4. DnD **只改 Host 侧模型**，禁止在 mime 中传 HWND；合入/拖出后再 `reattach`。

## 6. 首期范围边界

### 6.1 本 Demo 要做

- 单实例启动 → 一壳 +「创建 Client」  
- 多 Client 进程 + 同 Client 多子窗 + Tab 名递增  
- 关 Tab、拖出新壳、合入他壳、末 Tab 拖出毁壳  
- Windows `SetParent` 嵌入 + 最小 Protobuf 握手 / 建窗意图  

### 6.2 可后置或简化

- 精致拖出跟手动画（可用简单 `QDrag` 截图）  
- 心跳 / 无响应 UI（可后接规格 M6）  
- macOS / Linux 嵌入  
- 完整 `EmbedHelper` 多语言  

## 7. 与仓库目录

| 路径 | 角色 |
|------|------|
| `demos/` | 可执行 Demo（壳入口 + 创建按钮体验） |
| `src/host/` | 壳、Tab 模型、拖出合入、EmbedBackend(win) |
| `src/client/` | Client 进程；page 内「新建窗口」 |
| `src/common/` + `proto/` | 帧与 IDL |

**实现顺序建议**：先定 IPC 最小子集（`Hello` / 建 Client / 建子窗 / 关窗）→ `src` 薄层 → `demos` 入口；具体编码等本定稿确认入库后再开。

## 8. 已决议摘要

| # | 决议 |
|---|------|
| 1 | 交互对标 Chrome：并列 Tab、关 Tab、拖出新壳、合入他壳 |
| 2 | 末 Tab 拖出 → 原顶层壳销毁 |
| 3 | 启动仅一壳；中央按钮创建 Client |
| 4 | Client page 中央按钮创建同 Client 子窗口 |
| 5 | Tab 名：`Client{N}-Window{M}` 各自递增 |
