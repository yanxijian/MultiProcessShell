# Demo 形态（定稿）

> **English**：[../en/demo-morphology.md](../en/demo-morphology.md)  
> **状态**：已定稿（与当前仓库 Demo 实现一致）

## 1. 产品直觉

交互对标 **Chrome 多窗口 / 多标签**：

| 能力 | 行为 |
|------|------|
| 并列 | 不同 Client 进程的窗口以 **Tab** 并列在同一顶层壳窗 |
| 关闭 | 可单独关闭某个 Client Tab；关闭后按 **激活历史（MRU）** 切回上一 Tab |
| 拖出 | 将 Client Tab **拖出** 成新的顶层壳窗 |
| 合入 | 将 Tab **拖入** 另一顶层壳窗的 Tab 栏（按落点插入） |
| 同窗重排 | 在同一顶层壳内拖动 Client Tab，调整顺序 |
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

### 5.1 跟手视觉

| 层 | 何时出现 | 行为 |
|----|----------|------|
| **Tab 幽灵** | 一开始拖 Client Tab | 始终跟手；在 Tab 条上时 **Y 锁在 Tab 行**、X 跟光标；撕出后仍叠在窗口预览之上 |
| **窗口预览** | 垂直离开 Tab 条超过 **离开空隙** 后 | 不以独立热区跟手；按 Tab 幽灵摆放，使预览 **标题/Tab 栏垂直居中包裹** 该按钮（左侧 Home 占位）；回到条附近需进入更紧的 **返回空隙**（迟滞）才切回条内模式 |

拖动中：源 Tab 透明占位；同壳 / 合入目标壳其它 Tab **实时让位**（不是蓝色竖线）；源壳客户区切到上一激活 Tab。  
**撕出后**：窗口预览一出现，源壳上原 Tab 空位即被相邻 Tab **立刻占住**（不必等松手）；拖回条上则重新让出空隙。  
落在最小化 / 最大化 / 关闭上 → **禁止光标**（非合入热区）。

### 5.2 落点规则

1. **同壳水平拖** → 按让位顺序 **重排**（Home 始终最左，不可插到 Home 前）。  
2. **拖离条外松开** → 按预览几何（与「包裹 Tab」同一套对齐）**新建顶层壳**；预览短暂盖住新壳直至嵌入首帧（减闪黑）。  
3. **拖到他壳 Tab + 条末空白** → **合入**（实时让位指示落点；不含窗口按钮）。  
4. **Esc** 或仍在条/返回迟滞内松开 → **取消**（幽灵弹回；不撕出）。Windows OLE 下 Esc 由 Host 轮询。  
5. 若原壳因此 **无剩余 Client Tab**（仅剩 Home）且不是唯一壳 → **销毁原顶层壳**。  
6. DnD **只改 Host 侧模型**，mime 只传 tabId（`application/x-mps-tab-id`），禁止传 HWND；合入/拖出后再 reattach。  
7. **Home** 不可拖出 / 合入 / 重排。

实现要点（Host）：`TabDragGhost` + `TearOutPreview::alignToTabContent`；`previewTabYieldAtCursor` / `collapseTornOutTabSlot` / `commitTabYieldPreview`；让位空隙无子控件时 OLE 可能 `IgnoreAction`，Host 仍提交重排/合入。

## 6. 关 Tab 与激活链

- 关闭 Client Tab（点 × **或中键点击 Tab**，对标 Chrome）→ Host `QueryCloseSubWindow` → Client 同意并 `SubWindowRemoved`（Host 在 accept 时即可拆 Tab；对应 Client 子窗一并关闭）。  
- **Home** 不可关（中键无效）。  
- 激活链（MRU）包含 **Home 与 Client Tabs**；关当前 Tab 时切到历史上一个仍存在的 Tab（不强制回 Home）。

## 7. 首期范围边界

### 7.1 本 Demo 要做（已实现）

- 单实例启动 → 一壳 + **Home** +「Create Client」  
- 多 Client 进程 + 同 Client 多子窗 + Tab 名递增  
- 关 Tab（激活历史）、同窗重排、拖出新壳、合入他壳、无 Client Tab 时毁多余壳  
- 关壳清理 / 杀 Client 后 Host 不崩  
- Windows `SetParent` 嵌入 + 精简 Protobuf 合约  

手工验收步骤见 [demo-acceptance.md](demo-acceptance.md)。

### 7.2 可后置或简化

- Tab 幽灵 ↔ 窗口预览的连续形变、更强磁吸合入  
- 心跳定时器 / 无响应 UI（协议已预留）  
- macOS / Linux 嵌入  
- 完整 `EmbedHelper` 多语言  

## 8. 与仓库目录

| 路径 | 角色 |
|------|------|
| `demos/` | `mps_demo_host` / `mps_demo_client` |
| `src/host/` | 壳、Tab、会话、Win embed；拖出：`tear_out_preview.*` |
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
