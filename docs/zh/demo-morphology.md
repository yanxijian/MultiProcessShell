# Demo 形态（定稿）

> **English**：[../en/demo-morphology.md](../en/demo-morphology.md)  
> **状态**：已定稿（与当前仓库 Demo 实现一致）

## 1. 产品直觉

交互形态：**浏览器式可撕出多窗口 / 多标签**：

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
- 壳上有一个固定 **Home** Tab（不可关闭、不可拖出）——由 **`mps_host`** 提供。  
- **Home 客户区**（「创建 Client」、Light/Dark）由 **`demo_host`** 经 `ShellApp::setHomeContentFactory` 注入；框架默认仅为空白槽。**不**自动拉起任何 Client 进程。

### 2.2 创建 Client（新进程 / 新 clientKind / ClientSession）

- 在 **Home** 点击「创建 Client」→ 启动一个 Client 进程，并在当前壳新增一个 Client Tab。  
- 首个该 Client 下的子窗口标题形如：`Client1-Window1`。  
- 需要再建 Client 时，切回 **Home** 再点「创建 Client」。

### 2.3 同 Client 再建子窗口

- **Client 窗口（page）**：无系统标题栏的 **QFluentRibbon** 页（`RibbonWindow`）；Ribbon「New Window」经 IPC `Invoke("demo.request_new_window")`，由 Host 再发 `CreateSubWindow`。详见 [qfr-demo-client.md](qfr-demo-client.md)。  
- 点击后在 **同一 Client 进程** 内再建子窗口并新增 Tab。  
- 标题递增：`Client1-Window2`、`Client1-Window3`、…

### 2.4 多 Client

- 再次在 Home「创建 Client」→ `Client2-Window1`、…  
- 不同 Client = 不同进程（形态 A）；Tab 可同壳并列。  
- 页内可用 Ribbon Theme 组切 Light/Dark；经 Host `theme.set` **全局同步**（壳 + 全部 Client）。

### 2.5 QFR 嵌入页

Demo Client 页为 frameless `qfluentribbon::RibbonWindow`；构建与嵌入态限制见 [qfr-demo-client.md](qfr-demo-client.md)。

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
│  Client Tab 激活：嵌入 QFR Ribbon 页（无系统标题栏；「New Window」在 Ribbon） │
└────────────────────────────────────────────────────────┘
```

- **标题栏**：Home + Client Tabs + 最小化 / 最大化 / 关闭（框架 chrome）。  
- **工作区**：Home 客户区内容（Demo 注入）或当前 Client 嵌入区（应铺满客户区）。

## 5. 拖出 / 合入（浏览器式可撕出 Tab）

### 5.1 跟手视觉

| 层 | 何时出现 | 行为 |
|----|----------|------|
| **Tab 幽灵** | 一开始拖 Client Tab | 始终跟手；在 Tab 条上时 **Y 锁在 Tab 行**、X 跟光标；撕出后仍叠在窗口预览之上 |
| **窗口预览** | 垂直离开 Tab 条超过 **离开空隙** 后 | 不以独立热区跟手；按 Tab 幽灵摆放，使预览 **标题/Tab 栏垂直居中包裹** 该按钮（左侧 Home 占位）；回到条附近需进入更紧的 **返回空隙**（迟滞）才切回条内模式 |

拖动中：源 Tab 透明占位；同壳 / 合入目标壳其它 Tab **实时让位**（不是蓝色竖线）；源壳客户区切到上一激活 Tab。  
**撕出后**：窗口预览一出现，源壳上原 Tab 空位即被相邻 Tab **立刻占住**（不必等松手）；拖回条上则重新让出空隙。  
**唯一 Client Tab**（规格 §7.2 / Chrome 末页）：不走窗口预览，**真壳整窗跟手**；悬停他壳 Tab 条（较窄磁吸带）时目标让位并**自动合入**（整窗平移到槽位并淡出，无需松开）；拖到空白处松开则留在落点（不新建第二壳）。  
落在最小化 / 最大化 / 关闭上 → 非合入热区；**松开不 tear-out**（视为取消，Tab 回源壳）。  
拖出进行中若 Client 点「新建窗口」→ Host **排队** `CreateSubWindow`，拖结束后再发（规格 S5）。

### 5.2 落点规则

1. **同壳水平拖** → 按让位顺序 **重排**（Home 始终最左，不可插到 Home 前）。  
2. **拖离条外松开** → 按预览几何（与「包裹 Tab」同一套对齐）**新建顶层壳**；预览短暂盖住新壳直至嵌入首帧（减闪黑）。**唯一 Client Tab** 例外：真壳已跟手，松开即留在落点，不新建壳。  
3. **拖到他壳 Tab + 条末空白** → **合入**（实时让位指示落点；不含窗口按钮）。  
4. **Esc**、仍在条/返回迟滞内松开、或松手在 **min/max/close** 上 → **取消**（幽灵弹回；不撕出）。Windows OLE 下 Esc 由 Host 轮询。  
5. 若原壳因此 **无剩余 Client Tab**（仅剩 Home）且不是唯一壳 → **销毁原顶层壳**。  
6. DnD **只改 Host 侧模型**，mime 只传 tabId（`application/x-mps-tab-id`），禁止传 HWND；合入/拖出后再 reattach。  
7. **Home** 不可拖出 / 合入 / 重排。

实现要点（Host）：`TabDragGhost` + `TearOutPreview::alignToTabContent`；`previewTabYieldAtCursor` / `collapseTornOutTabSlot` / `commitTabYieldPreview`；让位空隙无子控件时 OLE 可能 `IgnoreAction`，Host 仍提交重排/合入。

## 6. 关 Tab 与激活链

- 关闭 Client Tab（点 × **或中键点击 Tab**）→ Host `QueryCloseSubWindow` → Client 同意并 `SubWindowRemoved`（Host 在 accept 时即可拆 Tab；对应 Client 子窗一并关闭）。  
- **Home** 不可关（中键无效）。  
- 激活链（MRU）包含 **Home 与 Client Tabs**；关当前 Tab 时切到历史上一个仍存在的 Tab（不强制回 Home）。

## 7. 首期范围边界

### 7.1 本 Demo 要做（已实现）

- 单实例启动 → 一壳 + **Home** +「Create Client」  
- 多 Client 进程 + 同 Client 多子窗 + Tab 名递增  
- 关 Tab（激活历史）、同窗重排、拖出新壳、合入他壳、无 Client Tab 时毁多余壳  
- 关壳清理 / 杀 Client 后 Host 不崩  
- Windows `SetParent` 嵌入 + 精简 Protobuf 合约  
- **心跳**：Client 2s `Heartbeat`；Host 约 6s 无响应 → Tab「无响应」；右键「终止进程」（不自动杀）  

手工验收步骤见 [demo-acceptance.md](demo-acceptance.md)。

### 7.2 可后置或简化

- Tab 幽灵 ↔ 窗口预览的连续形变、更强磁吸合入  
- REQ 通用超时 UI（与心跳 Unhealthy 独立）  
- macOS / Linux 嵌入  
- 完整 `EmbedHelper` 多语言  

## 8. 与仓库目录

| 路径 | 角色 |
|------|------|
| `demos/demo_host/` | Host Demo：`ThemeService`、Home 客户区（`home_page`）、组装配方 |
| `demos/demo_client/` | Client Demo：QFR Ribbon 页 |
| `src/host/` | 壳、Tab、会话、Win embed、Home **空槽**；拖出：`tear_out_preview.*`（**无**业务客户区控件） |
| `src/client/` | Client 进程与抽象 `ContentView`（无具体业务 UI） |
| `src/common/` + `proto/` | 帧与 IDL |

## 9. 已决议摘要

| # | 决议 |
|---|------|
| 1 | 浏览器式可撕出 Tab：并列 Tab、关 Tab、拖出新壳、合入他壳 |
| 2 | 无剩余 Client Tab 的多余壳 → 销毁 |
| 3 | 启动一壳 + 固定 **Home Tab**（框架）；Create Client / Light/Dark 在 **demo_host** Home 客户区 |
| 4 | Client page「新建窗口」→ 同 Client 子窗 |
| 5 | Tab 名：`Client{N}-Window{M}`；Home 除外 |
| 6 | 关 Tab 走激活历史（含 Home），默认不强制回 Home |

## 10. IPC

见 [demo-ipc.md](demo-ipc.md)。
