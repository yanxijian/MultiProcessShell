# 多进程共窗口（壳托管）技术说明与最小 Demo 蓝图（改进版）

> **用途**  
> 在《多进程共窗口-技术规格与最小 Demo 蓝图》基础上，补齐架构抽象、协议契约、生命周期状态机、安全与可观测性，并明确三平台降级策略与 Demo 验收边界。  
>  
> **日期**：2026-07-16  
> **状态**：规格 v2  
> **平台**：Windows、macOS、Linux（X11；Wayland 见 §4.5 / 附录 B）  
> **工程名**：`MultiProcessShell`  

> **IPC**：控制面 = `shell.ipc.v1` Protobuf + Pipe/UDS + 长度前缀帧；嵌入面 = EmbedBackend / EmbedHelper；本期不含 gRPC / Thrift / Mojo / JSON 主协议。  
> **Demo 落地**：形态见 [`demo-morphology.md`](demo-morphology.md)，精简 IPC 见 [`demo-ipc.md`](demo-ipc.md)；仓库权威 IDL 为 `proto/shell/ipc/v1/ipc.proto`（含 `CreateSubWindow` 等）。本文后续 IDL / 握手草图为产品全量愿景，若与 `.proto` 或 Demo 合约冲突，**以仓库 `.proto` + Demo 文档为准**。

---

## 修订说明（相对原文档）

| 类别 | 原文档已有 | 本版强化 / 新增 |
|------|------------|-----------------|
| 架构 | 角色、挂接、IPC 轮廓 | **EmbedBackend 抽象**、稳定 ID、能力协商、降级矩阵 |
| 进程模型 | 一类型一进程 | 明确「一类型一进程」为默认，并给出多实例扩展点 |
| 协议 | 方法名列表 | **Protobuf IDL**、`Envelope`、correlationId、心跳、错误码、幂等、多语言 |
| 生命周期 | 创建/关闭流程 | **ClientPage / EmbedSession 状态机**、崩溃恢复策略 |
| 安全 | 较少 | Pipe ACL、句柄校验、禁止跨完整性盲目 SetParent |
| 可观测 | 缺陷清单 | 日志字段、诊断模式、验收自动化建议 |
| Mac | 同进程直调 | 明确为 **一等公民路径**，而非「Win 方案的残缺移植」 |
| Wayland | 限制一句带过 | 独立附录：可选路线与 Demo 边界 |

**设计原则（本版新增）**

1. **Host 拥有编排，Client 拥有内容**：壳不理解业务；Client 不绘制壳外框。  
2. **稳定逻辑 ID 优先于原生句柄**：`tabId` / `pageId` 永不复用；`wid` 仅作挂接凭证，可失效。  
3. **双通道职责分离**：Protobuf IPC = 意图与状态；平台消息/`QEvent` = 嵌入几何与焦点。  
4. **同步调用必须有超时与降级**：壳线程永不被 Client 无限阻塞。  
5. **平台差异收敛到 EmbedBackend**：上层 Tab/拖出逻辑与挂接实现解耦。  
6. **能力协商优于硬编码**：启动时交换 `Capabilities`，Host 按能力启用功能。  
7. **契约单一来源**：`.proto` 为多语言唯一 IDL；禁止并行维护 JSON/Thrift 第二套主协议。

---

## 0. 结论

**壳进程（Host）负责外框与 Tab 编排；各业务子进程（Client）拥有真实程序窗口；跨进程协作走版本化 Protobuf IPC（多语言共用 IDL）；窗口挂接由平台 EmbedBackend / EmbedHelper 完成。**

| 平台 | 共窗挂接本质 | Demo 主路径 |
|------|----------------|-------------|
| **Windows** | 外进程 HWND → `SetParent` 进壳容器 + `WM_*` 旁路 | 形态 **A** |
| **Linux/X11** | 外进程 X window → **XEmbed / `XReparentWindow`** | 形态 **A** |
| **macOS** | 跨进程嵌入不可靠；**同进程模块 + `layout->addWidget`** | 形态 **C**（一等公民） |
| **Linux/Wayland** | 经典 reparent 不可用 | Demo **不做**；见附录 B |

不同类型的 Client 窗口可以作为**同一壳窗口下的不同 Tab**。同一类型的多个业务子窗口默认共享该类型的一个 Client 进程，在进程内切换子窗口；若需「同类型崩溃隔离」，走可选的多实例扩展（§2.4）。

---

## 1. 目标与非目标

### 1.1 要解决什么

| 需求 | 含义 | 验收可观察行为 |
|------|------|----------------|
| 多进程隔离 | 不同类型业务崩溃互不影响（形态 A） | 杀 `beta`，`alpha` Tab 仍可用，Host 不崩 |
| 统一外壳 | 统一标题栏、Tab、外观 | 用户感知为一个 App |
| 多窗口 Tab | 像浏览器一样切换业务窗口 | 切 Tab 内容与焦点正确 |
| 跨类型同壳 | 同一壳挂接不同类型 Client | alpha+beta 同窗两 Tab |
| 拖出 / 合入 | Tab 拖出新壳；拖回或拖到另一壳 | 状态机完整，无僵尸窗 |

### 1.2 非目标（Demo 与本期规格）

- 具体业务功能（文档、表格等）  
- 进程预热、挂起、内存压缩等进阶策略（可留接口）  
- Wayland 完整嵌入  
- 跨机器远程嵌入  
- 安全沙箱（AppContainer / seccomp）落地——仅要求 **不引入明显危险默认值**  
- 无障碍（UI Automation / VoiceOver）完整方案——列入后续

### 1.3 成功标准（产品级，不仅 Demo）

1. **感知一致**：用户感觉是「一个窗口里的多个文档/页面」。  
2. **故障域清晰**：Client 挂死/崩溃不拖死 Host UI 线程。  
3. **平台诚实**：Mac 不以假多进程冒充隔离；文档与 UI 行为一致。  
4. **可演进**：新增 `pageDelta` 类型时，Host 只需注册启动器与能力，不改 Tab 核心。  
5. **多语言 Client**：任意语言只要实现 `shell.ipc.v1` + 本地连接 +（可选）EmbedHelper，即可接入壳。

---

## 2. 角色、进程模型与抽象

### 2.1 进程角色

```
┌─────────────────────────────────────────────────────────────┐
│  Host / Shell 进程 (C++/Qt)                                   │
│  - 可执行体：host                                               │
│  - 职责：UI 外框、Tab、启停 Client、EmbedBackend、IPC Server     │
│  - 不职责：绘制业务内容、解释业务文档格式                         │
└───────────────────────────┬─────────────────────────────────┘
                            │ Protobuf Envelope over Pipe/UDS
                            │ + EmbedBackend (native / inproc)
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
   Client: alpha       Client: beta        Client: gamma
   (C++/其它语言)        (C++/其它语言)        (C++/其它语言)
```

**类型 ↔ 进程（约定示例）**

| pageType | appName | 可执行体 / 模块 | 说明 |
|----------|---------|-----------------|------|
| pageAlpha | `alpha` | `alpha` / `client_alpha` | 类型 A |
| pageBeta | `beta` | `beta` / `client_beta` | 类型 B |
| pageGamma | `gamma` | `gamma` / `client_gamma` | 类型 C |

壳侧默认维护 **「一 pageType ↔ 一个 ClientSession（进程或模块）」**。跨类型可同壳多 `ClientPage`。

### 2.2 部署形态

| 形态 | 含义 | 常见平台 | 崩溃隔离 |
|------|------|----------|----------|
| **A. 真·多进程嵌入** | Host/Client 分进程，原生窗口树挂接 | Windows、Linux/X11 | ✅ 跨类型 |
| **B. 无独立壳** | 每业务进程自带完整外框 | macOS 备选 | ✅ 按进程 |
| **C. 同进程直调** | Client UI 以模块跑在 Host 地址空间 | macOS 主路径 / 调试 | ❌ |
| **D. 混合** | 部分类型 A，部分 C | 按部署 | 部分 |

**决策树**

```
需要跨类型同壳 Tab？
 ├─ 否 → 形态 B（多顶层窗）即可
 └─ 是 → 目标平台支持稳定跨进程 reparent？
           ├─ Win / X11 → 形态 A
           ├─ macOS → 形态 C（接受隔离弱）或 B（放弃跨类型同壳）
           └─ Wayland → 附录 B / 暂缓
```

macOS **禁止**将「跨进程 `SetParent` / `fromWinId`」作为主路径。Mac Demo 显式选择 **C**（推荐）或 **B**。

### 2.3 壳内对象树

```
ShellApp
 └─ ShellMainWindow × N          // 可拖出多个壳窗
     ├─ HeaderBar
     ├─ TabBar → TabItem × M     // 视图；绑定 tabId
     └─ CentralArea
         └─ ClientPage × P       // 一 Client 主窗 / 一嵌入会话
             ├─ EmbedContainer   // EmbedBackend 挂接点
             └─ ClientTab × T    // 每个业务子窗口
```

| 壳对象 | Client 对象 | 稳定 ID | 易变凭证 |
|--------|-------------|---------|----------|
| `ClientPage` | Client 主窗口 | `pageId`（Host 生成） | `wid`（HWND/XID/`QWidget*`） |
| `ClientTab` | 业务子窗口 | `tabId`（Host 生成，单调） | Client 侧 `subId`（可选映射） |
| `ClientSession` | 进程或模块 | `sessionId` + `appName` | `pid` / 模块句柄 |

**改进点（相对原文档）**：上层逻辑一律用 `pageId`/`tabId`；`wid` 仅出现在 EmbedBackend 与握手瞬间。避免「`find(winId)` 作为唯一查找键」导致的短暂失效问题（原 C4）。

### 2.4 可选扩展：同类型多实例

原约定「一类型一进程」利于共享状态与减内存。若产品需要「同类型文档互不影响崩溃」：

| 策略 | 说明 |
|------|------|
| **默认** | 一 `appName` 一 `ClientSession` |
| **扩展** | `sessionKey = appName + "#" + instanceIndex`；每实例独立进程与 `ClientPage` |
| **Demo** | **不实现**；Host 注册表预留 `launch(appName, instanceKey)` |

### 2.5 EmbedBackend（本版核心抽象）

```text
IEmbedBackend
  - capabilities() -> EmbedCaps
  - embed(pageId, container, wid, opts) -> Result
  - updateGeometry(pageId, rect, dpi)
  - activate(pageId, tabId?)
  - detach(pageId)
  - reattach(pageId, newContainer)   // 拖出后
  - onClientDied(pageId)
```

| Backend | 平台 | 实现要点 |
|---------|------|----------|
| `WinHwndEmbedBackend` | Windows | `SetParent` + `WM_HOST_*` |
| `X11XEmbedBackend` | Linux/X11 | XEmbed + reparent |
| `InProcessWidgetBackend` | macOS / Debug | `layout->addWidget` + `QEvent` |

Host 的 TabBar / 拖出状态机 **只依赖 `IEmbedBackend`**，不直接 `#ifdef` 散落业务代码。

---

## 3. 启动、握手与能力协商

### 3.1 命令行（形态 A Client）

```
beta --from-host --endpoint=<path-or-pipe> --pipe-token=<token> --protocol=1 --host-pid=<pid>
```

| 参数 | 用途 |
|------|------|
| `--from-host` | 禁止当独立 App 误开完整外框（或开后立即进入托管模式） |
| `--endpoint` | Pipe/UDS 路径；缺省可由 token 推导（见 §5.1） |
| `--pipe-token` | 连接命名管道 / UDS 的共享密钥（见 §10） |
| `--protocol` | 协议主版本；不匹配则退出并上报 |
| `--host-pid` | 可选；用于校验父进程仍存活 |

形态 C：无 spawn；`loadModule("client_beta")` 后走同一套 `Envelope`（进程内传输）。

### 3.2 握手（形态 A）

```
Host                              Client
 |-- listen(pipe-token) ---------->|
 |-- spawn ----------------------->|
 |<-- hello{protocol,caps,pid} ----|
 |-- helloAck{protocol,hostCaps} ->|
 |<-- applicationConnected --------|
 |<-- mainWindowAdded{pageHint,wid}|
 |-- embed.begin(pageId) --------->|   // IPC 意图
 |-- BUILDPARENT / XEmbed -------->|   // 平台通道
 |<-- FINISHPARENT / embed.ok -----|
 |-- createWindow{tabId} --------->|
 |<-- subWindowAdded{tabId,...} ---|
 |-- TabBar add + activate --------|
```

### 3.3 握手（形态 C，macOS）

```
Host
 |-- load client_alpha
 |-- 主线程创建 ClientMainWindow*
 |-- pageId 分配；wid := (i64)(uintptr_t)widget
 |-- InProcessWidgetBackend.embed → addWidget
 |-- FinishParent QEvent
 |-- 后续 createWindow / Tab 与形态 A 同形
```

### 3.4 能力协商

Client `Hello.caps`（Protobuf 字段，逻辑等价于下表）：

| 字段 | 类型 | 含义 |
|------|------|------|
| `embed` | enum | `EMBED_HWND` / `EMBED_XEMBED` / `EMBED_INPROCESS` / `EMBED_NONE` |
| `tab_drag` | bool | 是否允许 Tab 拖出 |
| `modal_report` | bool | 是否上报模态 |
| `heartbeat` | bool | 是否发心跳 |
| `dpi_sync` | bool | 是否接受 DPI 同步 |
| `multi_sub_window` | bool | 是否支持多子窗 Tab |

Host 按 caps 关闭 UI 入口（例如 Client 不支持 `tab_drag` 则 Tab 禁止拖出）。**禁止**假设所有 Client 能力相同。多语言 Client 若无法嵌入，必须报 `EMBED_NONE`，由 Host 降级。

---

## 4. 三平台挂接实现

### 4.1 总表

| 维度 | Windows | Linux (X11) | macOS |
|------|---------|-------------|-------|
| Backend | `WinHwndEmbedBackend` | `X11XEmbedBackend` | `InProcessWidgetBackend` |
| 挂接 API | `SetParent` | XEmbed / `xcb_reparent_window` | `addWidget` / `setParent` |
| 容器 | 强制 `WA_NativeWindow` | XEmbed 容器 | **不要**乱设 `WA_NativeWindow` |
| 控制通道 | `WM_USER+N` | `QEvent` + 少量 ClientMessage | `QEvent` |
| `wid` | `HWND` | `WId` / `xcb_window_t` | `QWidget*` 指针值 |
| 浮动窗 | `GWLP_HWNDPARENT` / owner | transient / property | Qt modality / `NSWindow` parent |
| 输入 | 可转发 `NCHITTEST` | XEmbed focus | 正常 Qt 冒泡 |
| Tab 拖出反馈 | pixmap `QDrag` | 同左 | 建议独立 `NSWindow`+CALayer |
| 菜单 | 窗内 / 自绘 | 同左 | **全局菜单栏**随前台 Page 切换 |
| 控制面 IPC | Named Pipe + Protobuf | UDS + Protobuf | InProcess 队列 + 同一 Envelope |

### 4.2 Windows（形态 A）

#### 建立父子

```cpp
// Host → Client: WM_HOST_BUILDPARENT
// wParam = hostContainerHwnd, lParam = visible
SetWindowLongPtr(hSelf, GWL_EXSTYLE,
    GetWindowLongPtr(hSelf, GWL_EXSTYLE) & ~(WS_EX_LAYERED | WS_EX_TOOLWINDOW));
SetParent(hSelf, hHost);
MoveWindow(hSelf, 0, 0, hostW, hostH, TRUE);
PostMessage(hHost, WM_CLIENT_FINISHPARENT, (WPARAM)hHost, (LPARAM)hSelf);
```

脱离：`SetParent(hSelf, nullptr)`，去掉 `WS_CHILD`，恢复顶层样式。

#### 建议消息子集

| 宏 | 作用 |
|----|------|
| `WM_HOST_BUILDPARENT` | 建父子 |
| `WM_HOST_UPDATEGEOM` | 同步尺寸 |
| `WM_HOST_ACTIVATE` / `SETFOCUS` | 激活与焦点 |
| `WM_HOST_SUBWIN_ACTIVATE` | 切子窗口 |
| `WM_HOST_MOVED` | 壳移动 |
| `WM_HOST_DPI_CHANGED` | DPI 同步（新增建议） |
| `WM_HOST_DRAG_SUPPRESS_BEGIN/END` | 拖出抑制 |
| `WM_CLIENT_FINISHPARENT` | 建父子完成 |
| `WM_CLIENT_MODAL_CHANGED` | 模态上报（新增建议） |

#### Windows 注意点（保留并加强）

1. 建父子前阻塞布局（挡 `WM_WINDOWPOSCHANGING`）。  
2. 闪烁时可临时调整 `WS_CLIPCHILDREN`。  
3. 壳非客户区命中有时需转到 Client。  
4. 外进程弹窗：`AttachThreadInput`；拆挂接时枚举「壳下且属 Client PID」的窗并重挂。  
5. **一律 `SendMessageTimeout`**；超时 → 标记 session unhealthy → 可杀进程。  
6. Host/Client DPI 感知模式一致；挂接后推送 DPI。  
7. **新增**：嵌入前校验 `wid` 所属 PID == 期望 Client PID，防止错误句柄。  
8. **新增**：Host 与 Client 完整性级别不一致时，SetParent 可能失败——安装/启动应同级，失败要有明确错误态 Tab。

### 4.3 Linux / X11（形态 A）

```
Host EmbedContainer
  └─ XEmbedContainer
        └─ embedClient(clientWId)
```

注意：Owner/Transient 重挂；销毁检测；Demo 固定一种 WM；Wayland 见附录 B。

### 4.4 macOS（形态 C 为主，一等公民）

**为何不走 Win 式跨进程嵌入**（原结论保留）：AppKit 无稳定跨进程嵌入；`fromWinId` 跨进程问题多；乱设 `WA_NativeWindow` 有害。

**推荐路径 C**

```cpp
QWidget* hostContainer = ...;
QWidget* clientMw = reinterpret_cast<QWidget*>(static_cast<uintptr_t>(wid));
ensureLayout(hostContainer)->addWidget(clientMw);
QCoreApplication::postEvent(hostContainer, new FinishParentEvent);
```

- **Envelope 形状与 Win 相同**；传输改为进程内队列（仍序列化或直接传 `shared_ptr<Envelope>`，对外语义一致）。  
- 崩溃隔离变弱：规格明确接受；可选看门狗仅检测卡死（无法隔离 abort）。  
- Cocoa：全局菜单、红绿灯、全屏拖出降级、多屏钳制——同原文档。  
- **多语言限制**：形态 C 仅适合可链进 Host 的原生模块（C/C++/Qt）。Python/Go/Electron 等在 Mac 上应走真进程，并在 `caps.embed=EMBED_NONE` 下降级（独立顶层窗或仅部分类型可进壳）。

**路径 B**：放弃跨类型同壳时使用。

### 4.5 Wayland（Demo 边界）

本期 **不实现**。可选远期路线见 **附录 B**。Demo 在纯 Wayland 会话应检测并提示「请使用 X11 或 XWayland 验证形态 A」。

---

## 5. IPC 协议：Protobuf + 进程间字节流

> 方案以 M0（`.proto` + 帧编解码单测）为实现起点。

### 5.0 选型结论

| 项 | 冻结选择 |
|----|----------|
| IDL | **Protocol Buffers（proto3）**，包名 `shell.ipc.v1` |
| 传输 | Windows **Named Pipe**；macOS/Linux **Unix Domain Socket**；调试可临时使用 TCP loopback |
| Host API | Qt `QLocalServer` / `QLocalSocket` 作为字节流传输，不承载 JSON |
| 帧 | **长度前缀 + `Envelope` protobuf**（见 §5.2） |
| 同进程（Mac） | 同一 `Envelope`；`InProcessTransport`（队列），语义与跨进程一致 |
| 本期范围外 | gRPC 运行时、Thrift、Mojo、JSON 主协议、跨机器 RPC |
| 远期可选 | 同一 `.proto` 上叠加 gRPC（UDS）；不并行维护第二套 IDL |

**分层示意**

```
Host (C++/Qt)
  Tab / Session / IEmbedBackend
       ▲
       │ Envelope (protobuf)
  LocalChannel (QLocalSocket 或 InProcess)
       │  length-prefixed frames
       │  Win Pipe / *nix UDS
Client (C++ / Go / Python / C# / …)
  proto stub + LocalConn + EmbedHelper(native, 可选)
```

**双通道（不变）**

| 通道 | 职责 | 是否进 `.proto` |
|------|------|-----------------|
| Protobuf IPC | 握手、窗口生命周期意图、标题、模态、心跳、拖出编排 | ✅ |
| 平台第二通道 | `SetParent`/`WM_*`、XEmbed、同进程 `QEvent`、几何与焦点 | ❌ 由 EmbedBackend / EmbedHelper 执行 |

### 5.1 传输与端点

| 平台 | 端点约定（示例） |
|------|------------------|
| Windows | `\\.\pipe\shell-embed-<token>` |
| macOS / Linux | `$XDG_RUNTIME_DIR/shell-embed-<token>.sock` 或 `/tmp/shell-embed-<token>.sock` |
| 调试 | `127.0.0.1:<port>`（仅开发；正式默认禁用） |

命令行（形态 A）：

```
beta --from-host --endpoint=<path-or-pipe> --pipe-token=<token> --protocol=1 --host-pid=<pid>
```

| 参数 | 用途 |
|------|------|
| `--endpoint` | 显式端点；缺省由 token 推导 |
| `--pipe-token` | 随机密钥；用于路径派生与（可选）首包校验 |
| `--protocol` | Client 声明的协议主版本；与 Host 协商 |
| `--host-pid` | 可选；父进程存活校验 |

形态 C：无 socket；`InProcessTransport` 投递同一 `Envelope`。

### 5.2 帧格式

**推荐（正式）**：单层长度前缀包住完整 `Envelope`。

```text
| payload_len uint32 BE | Envelope protobuf bytes |
```

- `payload_len`：后续字节数，建议上限（如 16 MiB），超限断连。  
- 接收方按长度拼包，再 `Envelope.ParseFromArray`。  
- **禁止**换行 JSON、禁止依赖文本分隔符（payload 可为任意二进制）。

**可选增强（非 Demo 必须）**：帧头增加 `magic`（如 `0xSE 0x01`）便于抓包识别；仍以 `Envelope.protocol` 做版本裁决。

### 5.3 Envelope 与消息契约（规范草案）

契约源文件（实现时置于仓库）：

```text
proto/shell/ipc/v1/ipc.proto
```

逻辑结构（文档级草案，非生成代码）：

```protobuf
syntax = "proto3";
package shell.ipc.v1;

enum Dir {
  DIR_UNSPECIFIED = 0;
  DIR_REQ = 1;
  DIR_RES = 2;
  DIR_EVT = 3;
}

enum EmbedKind {
  EMBED_UNSPECIFIED = 0;
  EMBED_HWND = 1;
  EMBED_XEMBED = 2;
  EMBED_INPROCESS = 3;
  EMBED_NONE = 4;
}

enum ErrorCode {
  ERROR_UNSPECIFIED = 0;
  ERROR_TIMEOUT = 1;
  ERROR_BUSY = 2;
  ERROR_NOT_FOUND = 3;
  ERROR_DENIED = 4;
  ERROR_PROTOCOL = 5;
  ERROR_EMBED_FAILED = 6;
  ERROR_UNHEALTHY = 7;
}

message Capabilities {
  EmbedKind embed = 1;
  bool tab_drag = 2;
  bool modal_report = 3;
  bool heartbeat = 4;
  bool dpi_sync = 5;
  bool multi_sub_window = 6;
}

message RpcError {
  ErrorCode code = 1;
  string message = 2;
}

message Envelope {
  uint32 protocol = 1;       // 主版本；不兼容则拒绝
  string id = 2;             // correlation id；DIR_EVT 可空
  Dir dir = 3;
  int64 page_id = 4;         // 无则 0
  int64 tab_id = 5;
  int64 ts_ms = 6;
  oneof body {
    // handshake
    Hello hello = 10;
    HelloAck hello_ack = 11;
    ApplicationConnected application_connected = 12;
    ApplicationDestroyed application_destroyed = 13;

    // windows
    MainWindowAdded main_window_added = 20;
    MainWindowDestroyed main_window_destroyed = 21;
    NewMainWindow new_main_window = 22;
    CreateWindow create_window = 23;
    SubWindowAdded sub_window_added = 24;
    SubWindowRemoved sub_window_removed = 25;
    SubWindowTitleChanged sub_window_title_changed = 26;
    SubWindowActivated sub_window_activated = 27;
    ActiveSubWindow active_sub_window = 28;
    QueryCloseSubWindow query_close_sub_window = 29;
    QueryCloseMainWindow query_close_main_window = 30;
    MoveSubWindowTo move_sub_window_to = 31;
    NotifyMainWindowReattachment notify_main_window_reattachment = 32;
    ContainerPageVisibleChanged container_page_visible_changed = 33;

    // misc
    ModalChanged modal_changed = 40;
    SetDragSuppress set_drag_suppress = 41;
    Heartbeat heartbeat = 42;
    Ping ping = 43;
    Pong pong = 44;
    Unhealthy unhealthy = 45;

    RpcError error = 100;    // 仅 DIR_RES
  }
}

message Hello {
  uint32 min_protocol = 1;
  uint32 max_protocol = 2;
  uint32 pid = 3;
  string app_name = 4;
  Capabilities caps = 5;
}

message HelloAck {
  uint32 protocol = 1;
  string session_id = 2;
  Capabilities host_caps = 3;
}

message MainWindowAdded {
  uint64 wid = 1;            // HWND / XID / QWidget* 指针值
  uint32 pid = 2;            // 必须与 Hello.pid 一致，供 Host 校验
  bool visible = 3;
}

message CreateWindow {
  // page_id / tab_id 在 Envelope 头；Host 分配 tab_id 后下发
}

message SubWindowAdded {
  string title = 1;
}

message MoveSubWindowTo {
  int64 target_page_id = 1;
  int32 insert_index = 2;
}

message Heartbeat {}
message Ping {}
message Pong {}
message Unhealthy { string reason = 1; }
message ModalChanged { bool modal = 1; }
message SetDragSuppress { bool suppress = 1; }
// 其余消息体按同风格补全；无字段的用空 message 占位
```

**版本规则**

- `Envelope.protocol` / `Hello.min_protocol`/`max_protocol`：主版本不交叠 → 断连，`ERROR_PROTOCOL`。  
- 字段演进遵循 proto3：只增字段、废弃用 `reserved`；禁止复用 field number。  
- **禁止手改生成代码**；CI 由 `.proto` 生成各语言 stub。

### 5.4 方向与方法子集

**Client → Host（多为 `DIR_EVT`）**  
`Hello` / `ApplicationConnected` / `MainWindowAdded` / `MainWindowDestroyed` /  
`SubWindowAdded|Removed|TitleChanged|Activated` / `ModalChanged` / `Heartbeat` /  
`ApplicationDestroyed` / `Unhealthy`

**Host → Client（多为 `DIR_REQ`，配对 `DIR_RES`）**  
`HelloAck` / `NewMainWindow` / `CreateWindow` / `ActiveSubWindow` /  
`QueryCloseSubWindow` / `QueryCloseMainWindow` / `MoveSubWindowTo` /  
`NotifyMainWindowReattachment` / `ContainerPageVisibleChanged` / `SetDragSuppress` / `Ping`

**超时**：凡 `DIR_REQ` 必须有超时（建议默认 3–5s）；超时对内标记 session `Unhealthy`，对外可 `ERROR_TIMEOUT`，**禁止无限阻塞 Host UI 线程**。

### 5.5 心跳与健康

| 项 | 建议默认（Demo 可调） |
|----|----------------------|
| Client → Host `Heartbeat` | 每 2s（若 `caps.heartbeat`） |
| Host 判定超时 | 连续约 6s 无心跳 → Unhealthy |
| Unhealthy UI | Tab「无响应」+「终止进程」 |
| `Ping`/`Pong` | Host 主动探活 |

### 5.6 双通道、幂等与禁止项

- **Protobuf**：创建/关闭、标题、模态、健康、拖出编排意图、能力协商。  
- **原生/`QEvent`**：真正执行 embed、几何、激活、焦点（可经官方 **EmbedHelper C ABI** 供多语言 FFI）。  
- **幂等**：重复 `ApplicationDestroyed` / 重复 `detach` 必须安全。  
- **禁止**：DnD mime 传 HWND/WId；无超时同步调用；用 JSON/Thrift 作第二主协议；在 protobuf 里「命令 Host 对任意 HWND SetParent」而不绑定 `session_id`/`pid` 校验。

### 5.7 ID 规则

| ID | 分配者 | 生命周期 |
|----|--------|----------|
| `session_id` | Host（`HelloAck`） | ClientSession 存活期 |
| `page_id` | Host | Page 存活期；销毁后不复用 |
| `tab_id` | Host | Tab 存活期；销毁后不复用 |
| `wid` | Client（`MainWindowAdded`） | 可随 reparent 变化；仅 Backend 使用 |

### 5.8 Host（Qt）集成要点

```text
QLocalSocket.readyRead
  → 缓冲按 uint32 拼帧
  → Envelope.ParseFromArray
  → SessionManager.dispatch(body)
       ├─ 更新 TabModel / 状态机
       └─ 需挂接 → IEmbedBackend（平台第二通道）
```

### 5.9 多语言 Client 形态

```text
lang-app
  ├─ generated stub (shell.ipc.v1)
  ├─ LocalConn（Pipe/UDS 读写长度前缀帧）
  └─ EmbedHelper（官方 C ABI，推荐）
        Win: 配合 WM_HOST_* / SetParent
        X11: XEmbed
        无法嵌入: caps.embed = EMBED_NONE
```

- 业务可用任意语言；**嵌入逻辑优先复用 EmbedHelper**，避免每语言重写。  
- Demo 主路径仍以 **C++ Client（alpha/beta）** 验收嵌入；另规划 **一种异语言烟测**（如 Python/Go）证明契约（见 §12.4 M0/M4b）。

### 5.10 与 gRPC / Thrift / JSON 的关系

| 方案 | 本规格态度 |
|------|------------|
| Protobuf + 长度前缀帧 | **控制面主路径** |
| gRPC | 远期可选；复用同一 `.proto`，本期不引入 |
| Thrift | 不采用（避免双 IDL） |
| 换行 JSON | 不作为控制面主协议；调试日志可另用文本 |
| Mojo | 不采用 |

### 5.11 安全（控制面）

见 §10；与 Protobuf 相关的要点：`MainWindowAdded.pid` 必须与 `Hello.pid` 一致且与 OS 查询的窗口属主 PID 一致，否则 `ERROR_DENIED` / 拒绝 embed。

---

## 6. 生命周期状态机（新增）

### 6.1 ClientSession

```
Idle → Launching → Handshaking → Ready → Unhealthy
                      │             │         │
                      └→ Failed ←───┴→ Stopping → Stopped
                                      ↑
                                 Crashed / Exited
```

### 6.2 ClientPage / EmbedSession

```
Created → Embedding → Embedded → Detaching → Detached
                │                     │
                └→ EmbedFailed        └→ Reembedding → Embedded
```

拖出合入：`Embedded → Detaching → Detached → Reembedding → Embedded`。  
关闭：先 `queryClose*`，成功后 `Detaching` → 毁 Tab/Page → 会话可停。

### 6.3 崩溃与退出策略

| 事件 | Host 行为 |
|------|-----------|
| 进程退出 / 断连 | 幂等清理；受影响 Tab → 错误占位或关闭（Demo：**关闭**） |
| unhealthy 超时 | 不自动杀（避免误杀）；提供用户操作「终止」 |
| 终止后 | 允许「重新打开」→ 新 session（新 pageId/tabId） |

形态 C：无进程崩溃隔离；模块 `abort` 仍会导致整壳退出——规格中写明。

---

## 7. Tab 拖出 / 合入

### 7.1 状态机

```
Idle → PressOnTab → DragThresholdExceeded → Dragging
        → OverOtherTabBar ? MergePreview
        → DropOutside     ? CreateNewShell + moveTo
        → DropCancel      ? SnapBack
```

### 7.2 平台差异

| 步骤 | Windows / Linux | macOS |
|------|-----------------|-------|
| 跟手反馈 | `QDrag` + Tab 截图 | 建议独立 NSWindow 动画层 |
| 拖出 | `IgnoreAction` → 新壳 | 同左；全屏/多屏分支 |
| 挂接 | Backend `reattach` | `addWidget` / 延后 `moveTo` |
| 合入 | TabBar 命中 + `canMergeInto` | 同左 |
| 仅一壳一 Tab | 可改为移动整窗 | 同左 |

### 7.3 体验细则（保留 + 微调）

1. **hotspot**：落窗 `pos = cursor - hotspot - 标题栏偏移`。  
2. **拖出抑制**：`SetDragSuppress(suppress=true)`；结束再恢复。  
3. **原壳焦点**：拖出前激活上一 Tab。  
4. **空壳关闭**：Tab 归零则关窗（Demo 固定此策略）。  
5. **不可拖**：加载中、模态、`caps.tab_drag=false`、标记固定 Tab。  
6. **合入权限**：Client 可暂时 `canMergeInto=false`。  
7. **附属面板宽度**：新壳几何扣除侧栏。  
8. **DnD 只在 Host 模型内完成**：先改 Tab 归属，再 `reattach`。  
9. **新增**：拖出进行中禁止对该 `page_id` 发起 `CreateWindow`（队列化）。

---

## 8. 创建 / 切换 / 关闭

### 8.1 创建

```
用户请求 → 解析 app_name
 → 无 Session 则 launch / loadModule
 → 确保 ClientPage + EmbedContainer
 → CreateWindow(tab_id) → SubWindowAdded → 加 Tab → 激活
```

启动限流：同时 launching 的 Session 数建议 ≤ 2；其余显示加载占位。

### 8.2 切换 Tab

1. 更新 current `tab_id`  
2. 跨 Page：切换 Central 显隐（**多 Container 保活**，避免重挂接）  
3. `ActiveSubWindow` + Backend `activate`  
4. macOS：刷新全局菜单栏  

### 8.3 关闭

1. `QueryCloseSubWindow`（可确认）  
2. 成功删 Tab；Page 空则拆挂接并考虑结束 Session  
3. **销毁顺序**：先 `detach` → 再允许 Client 毁窗 → 再毁 Container  

---

## 9. 交互与缺陷处理

原文档 §8 缺陷表（G/F/M/D/C/P）仍然有效，本版 **增补**：

| ID | 现象 | 处理 |
|----|------|------|
| S1 | 错误 HWND 被嵌入 | embed 前 PID/类名校验 |
| S2 | 协议版本不匹配 | hello 失败；Tab 显示「版本不兼容」 |
| S3 | 管道名可预测被连 | token + ACL（§10） |
| S4 | 重复销毁回调 | 幂等；状态机拒绝非法迁移 |
| S5 | 拖出中 createWindow | 队列至 Drag 结束 |
| S6 | 完整性级别导致 SetParent 失败 | 明确错误；勿静默重试死循环 |
| A1 | 读屏找不到客户区 | 后续：暴露 accessible 代理（非 Demo） |

平台能力矩阵同原文档；Mac「真崩溃隔离」在形态 C 为 ❌。

---

## 10. 安全基线（新增）

Demo 也需满足最低安全默认值：

| 项 | 要求 |
|----|------|
| 管道命名 | 含随机 `token`；勿用固定全局名 |
| Windows Pipe ACL | 默认仅当前用户；拒绝 Everyone 写 |
| 连接校验 | 首包必须为 `Hello`；token 派生证明或共享 token（Demo） |
| 句柄校验 | `MainWindowAdded.wid` 所属 PID == `Hello.pid`，否则拒绝 embed |
| 完整性 | 同完整性启动；失败可见 |
| 注入面 | Client 不因 `--from-host` 自动提权 |
| 帧上限 | 单帧超限（如 >16MiB）断连，防 OOM |

完整沙箱不在本期范围。

---

## 11. 可观测性与诊断（新增）

### 11.1 日志字段

每条关键日志建议带：`sessionId,pageId,tabId,appName,pid,envelope.body_case,elapsedMs,errorCode`。

诊断时可对 Envelope 做 **短 hex / 字段摘要**，不要默认把整帧当 JSON 打日志。

### 11.2 诊断模式

`host --embed-debug`：

- 容器绘制半透明边框  
- 显示当前 `pageId/tabId/wid`  
- 记录每次 embed/detach 耗时  

### 11.3 崩溃信息

- Host 捕获 Client 退出码  
- Windows：可选 WER / 最小 dump 路径配置（Demo 可只记退出码）

---

## 12. 最小 Demo 蓝图

### 12.1 验收标准

**Windows / Linux（X11）**

1. `host` 空壳 → 拉起 `alpha`/`beta`，同壳两 Tab，可切换。  
2. `alpha` 内再建一子窗 → 同 Session 两 Tab，不换进程。  
3. `beta` Tab 拖出第二壳，再拖回合入。  
4. 杀 `beta`，Host 不崩；对应 Tab 清理。  
5. 断心跳后 Tab 进入无响应态，可手动终止。  
6. 协议版本不匹配时，启动失败可见。  
7. 至少一种非 C++ Client 能完成 `Hello`↔`HelloAck`（嵌入可降级；约定为 Python）。

**macOS**

1. 同进程加载 alpha/beta，同壳两 Tab。  
2. Tab 拖出 + 合入（动画可选）。  
3. 切 Tab 全局菜单正确。  
4. 全屏拖出禁止或明确降级。  
5. 产品说明中声明：macOS 为同进程模块路径，无跨类型崩溃隔离。

### 12.2 技术栈

| 项 | 约定 |
|----|------|
| UI | 开源 Qt **6.8+**（兼容基线 6.8 LTS） |
| 环境变量 | `QTDIR`；工程用 `%QTDIR%` / `$env:QTDIR` |
| PATH | `%QTDIR%\bin` |
| CMake | `CMAKE_PREFIX_PATH` 指向 `$QTDIR`；`find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)` |
| 模块 | `Core` / `Gui` / `Widgets` / `Network` |
| IDL | Protobuf proto3（`shell.ipc.v1`） |
| IPC 传输 | Named Pipe / UDS；Host 用 `QLocalSocket` 作字节流 |
| 帧 | `uint32 BE` 长度 + `Envelope` |
| 抽象 | `IEmbedBackend` + 三实现；可选 `EmbedHelper` C ABI |
| Windows | `SetParent` + `WM_*` |
| Linux | XEmbed |
| macOS | 同进程 `addWidget`；可选 Cocoa 扩展（`.mm`） |
| 构建 | CMake + `protoc` |
| 本期不做 | gRPC / Thrift / Mojo / JSON 主协议 |

### 12.3 建议目录结构

```
MultiProcessShell/
  proto/shell/ipc/v1/     ipc.proto          # 唯一契约源
  common/cpp/             帧编解码、校验、结果类型
  embed_helper/           C ABI（Win/X11），供多语言 FFI
  host/
    core/                 ShellApp, SessionManager, TabModel
    ipc/                  LocalServer, EnvelopeDispatcher
    embed/                IEmbedBackend + win/x11/inproc
    ui/                   MainWindow, TabBar, HeaderBar
    mac/                  DragAnim.mm, MenuBarBridge.mm  (可选)
  client/
    runtime/              ClientConn, embed hooks
    alpha/                C++ 示例
    beta/                 C++ 示例
  clients/python/         多语言烟测（M4b）
  tests/                  proto 编解码与拼帧单测；可选 embed smoke
  README.md
```

### 12.4 里程碑

| 里程碑 | Win/Linux | macOS | 交付物 |
|--------|-----------|-------|--------|
| **M0 契约** | — | — | 冻结 `.proto` + 帧格式；C++ 编解码/拼帧单测（无 UI） |
| M1 进程内挂接 | 同进程 SetParent 实验 | addWidget | `IEmbedBackend` 接口冻结 |
| M2 跨进程挂接 | spawn + Protobuf 握手 + BUILDPARENT/XEmbed | 跳过→load 模块 | `Hello` 能力协商 |
| M3 多子窗 Tab | `ActiveSubWindow` | 进程内 | 稳定 `tab_id` |
| M4 跨类型同壳 | 双进程双 Container | 双模块双 Page | Session 状态机 |
| **M4b 多语言烟测** | Python Client `Hello`（嵌入可 `EMBED_NONE`） | 同左或跳过 | 验证 IDL 跨语言 |
| M5 拖出合入 | reattach | addWidget+可选动画 | 拖出中请求队列 |
| M6 关闭崩溃 | 超时与僵尸 Tab | 模块异常说明 | 心跳 + 无响应 UI |
| M7（可选） | Pipe ACL + wid PID 校验 | 菜单切换单测 | 安全基线 |

### 12.5 伪代码

**Win：mainWindowAdded**

```cpp
auto page = session->ensurePage(pageId);
backend->embed(pageId, page->container(), wid, {.visible=vis});
// 内部：PostMessage(client, WM_HOST_BUILDPARENT, container->winId(), vis);
```

**Mac：mainWindowAdded**

```cpp
auto* clientMw = reinterpret_cast<QWidget*>(static_cast<uintptr_t>(wid));
backend->embed(pageId, container, wid, {});
// 内部：ensureLayout(container)->addWidget(clientMw);
```

**拖出**

```cpp
auto* newShell = app->createMainWindow();
tabModel->moveTab(tabId, newShell);
backend->reattach(pageId, newShell->containerFor(pageId));
```

### 12.6 禁止事项

- Mac 上把跨进程 `SetParent`/`fromWinId` 当主路径。  
- DnD mime 传 HWND/WId。  
- 无超时同步调用 Client。  
- 一个原生主窗同时承载两种 `pageType`。  
- Linux 忽略浮动窗 owner 重挂。  
- 用易变 `wid` 作为 Tab 主键。  
- 假设所有 Client 具备相同 caps。  
- Everyone 可写的命名管道。  
- 以 JSON/Thrift 作为第二套控制面主协议。

---

## 13. 方案对比

| 方案 | 优点 | 缺点 | 本架构中的位置 |
|------|------|------|----------------|
| 单进程多窗口 | 简单 | 崩溃面大 | Mac 形态 C / 调试 |
| 多进程多顶层窗 | 隔离好 | 无统一 Tab | 形态 B |
| 壳 + 原生挂接 + Protobuf IPC | 统一 UI + 隔离 + 多语言 | 平台分叉大 | Win/X11 主路径 |
| 壳 + 同进程模块 | Mac 友好 | 隔离弱；多语言受限 | Mac 主路径 |
| WebEngine/CEF 多进程 | 成熟进程模型 | 内容必须是 Web | **不采用** |
| Wayland Compositor 壳 | 现代 Linux 正统 | 成本高 | 远期附录 B |
| gRPC（本机 UDS） | 流式 RPC 成熟 | 本期过重 | 远期可选 |
| Mojo | 贴近 Chromium | 非通用库 | **不采用** |

---

## 14. 实现提示词（可复制）

```text
实现「壳托管多进程/多模块共窗口」最小 Demo（按本规格 v2）。
从里程碑 M0 开始交付。

架构要求：
- Host 只做外壳与 Tab 编排；挂接通过 IEmbedBackend。
- 开源 Qt 6.8+（兼容基线 6.8 LTS；CMAKE_PREFIX_PATH=$QTDIR；
  find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)；禁止仅 6.9+ API）。
- 工程通过环境变量引用 Qt：%QTDIR% / $env:QTDIR。
- Windows/Linux(X11)：形态 A；Win=SetParent+WM_*；Linux=XEmbed。
- macOS：形态 C；同进程模块 + addWidget；禁止跨进程嵌入主路径。
- IPC：Protobuf shell.ipc.v1；Envelope + uint32 BE 长度前缀；
  传输为 Named Pipe / UDS（QLocalSocket 仅作字节流）；同进程用同一 Envelope。
- 支持 Hello 能力协商、Heartbeat、REQ/RES 超时、RpcError。
- 稳定 ID：page_id/tab_id 由 Host 分配；wid 仅给 Backend；校验 wid 的 PID。
- 多语言：.proto 为唯一 IDL；可选 EmbedHelper C ABI；M4b 使用 Python 做 Hello 烟测。
- 本期不上 gRPC/Thrift/Mojo/JSON 主协议。
- 处理：几何稳定、焦点、模态 owner、拖出抑制、空壳关闭、超时、销毁顺序、多屏钳制、
  拖出中请求队列、Client 无响应 UI、Pipe 最小 ACL。
- 按里程碑 M0→M6（可选 M7）交付，并分别给出 Win/Linux/Mac 验收说明。
```

---

## 15. 开放问题（建议在立项时拍板）

| # | 问题 | 建议默认 |
|---|------|----------|
| Q1 | 同类型是否允许多进程实例？ | Demo 否；接口预留 |
| Q2 | Client 无响应是否自动杀？ | 否，需用户确认 |
| Q3 | 空壳是否保留？ | 否，Tab 归零关窗 |
| Q4 | Host 是否允许无 Client 的「欢迎页」？ | 允许，非嵌入 Page |
| Q5 | 是否统一自绘标题栏？ | Demo 可先用系统标题栏+Tab |
| Q6 | Wayland 是否进正式路线图？ | 另项评估（附录 B） |
| Q7 | 本期是否引入 gRPC？ | 否；仅 Protobuf 长度前缀帧 |
| Q8 | 多语言烟测语言 | **Python** |
| Q9 | EmbedHelper 是否 Demo 必做？ | Win/X11 建议提供；多语言嵌入可依赖其 C ABI |

---

## 附录 A. 术语

| 术语 | 含义 |
|------|------|
| Host / Shell | 壳进程（C++/Qt） |
| Client | 业务进程或模块（可为多语言） |
| ClientSession | 一次 Client 连接（进程或模块实例） |
| ClientPage | 与一个 Client 主窗对应的嵌入页 |
| ClientTab | 与一个业务子窗对应的标签 |
| EmbedBackend | Host 侧平台挂接实现 |
| EmbedHelper | 可选 C ABI，供多语言 Client 做平台嵌入 |
| Envelope | `shell.ipc.v1` 统一外层消息 |
| wid | 原生窗口凭证（易变） |
| page_id / tab_id | Host 稳定逻辑 ID |

---

## 附录 B. Wayland 远期选项（非 Demo）

| 路线 | 思路 | 成本 | 备注 |
|------|------|------|------|
| B1 放弃嵌入 | 形态 B 多顶层窗 | 低 | 无跨类型同壳 |
| B2 同进程 | 形态 C | 低 | 与 Mac 类似 |
| B3 嵌套 compositor | Host 做小型 Wayland compositor，Client 作 client | 高 | 最接近「正统」 |
| B4 xdg-foreign 等 | 依赖 compositor 扩展 | 中高 | 生态碎片 |
| B5 XWayland | 仍走 XEmbed | 中 | 非纯 Wayland |

---

## 附录 C. 与原文档 / 改进草案的映射

| 原章节 / 草案 | 本版 v2 |
|---------------|---------|
| §0–2 结论与角色 | §0–2，并增 EmbedBackend / 稳定 ID / 多语言 |
| §3 握手 | §3，能力协商改为 Protobuf `Capabilities` |
| §4 平台挂接 | §4，Mac 一等公民；多语言 Mac 限制 |
| §5 控制面协议 | **§5 定为 Protobuf + 长度前缀帧** |
| §6–7 拖出与开关 | §7–8，状态机关联 §6 |
| §8 缺陷 | §9 + 安全/诊断增补 |
| §9 Demo | §12，增加 M0 / M4b；目录含 `proto/` |
| §10 提示词 | §14 |
| §11 对比 | §13 |

---

## 附录 D. IPC 方案对照

| 方案 | 多语言 | Qt 集成 | 本场景适配 | 结论 |
|------|--------|---------|------------|------|
| QLocalSocket + JSON | 弱（无 IDL） | 易 | 短期够用 | 不作为主协议 |
| Protobuf + 长度前缀 + Pipe/UDS | 强 | 易（字节流） | 合适 | **采用** |
| gRPC | 强 | 较重 | 本期过重 | 远期可选 |
| Thrift | 强 | 中 | 可用 | 不采用（避免双 IDL） |
| Mojo | 弱（绑 Chromium） | 难 | 不合适 | 不采用 |

---

## 附录 E. 基线决议

| # | 决议项 | 结论 |
|---|--------|------|
| 1 | 控制面 IPC | Protobuf `shell.ipc.v1` + Pipe/UDS + 长度前缀帧 |
| 2 | 排除项 | 本期不上 gRPC / Thrift / Mojo；不以 JSON 为主协议 |
| 3 | 三端形态 | Windows / Linux(X11)=形态 **A**；macOS=形态 **C**；Wayland 本期不做 |
| 4 | Host / Client(C++) UI | 开源 Qt **6.8+**，兼容基线 **6.8 LTS** |
| 5 | Qt 引用方式 | `QTDIR`；工程用 `%QTDIR%` / `$env:QTDIR` |
| 6 | 多语言烟测 | Python（里程碑 M4b） |
| 7 | 交付顺序 | M0（契约与单测）→ M1…M6（可选 M7） |

**形态摘要**

- **A**：Host/Client 分进程 + 原生窗口挂接（`SetParent` / XEmbed）  
- **C**：Client 以模块跑在 Host 内 + `addWidget`（无跨进程嵌入）

**Qt 约定**

| 项 | 约定 |
|----|------|
| 许可证 | Qt Community / 开源 |
| 兼容基线 | Qt 6.8 LTS |
| 目标版本 | Qt 6.8+ |
| 环境变量 | `QTDIR`；工程用 `%QTDIR%` / `$env:QTDIR` |
| PATH | `%QTDIR%\bin` |
| CMake | `CMAKE_PREFIX_PATH=$QTDIR`；`find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)` |
| API 约束 | 不得依赖仅 6.9+ 才有的 API |
| 模块 | `Core` / `Gui` / `Widgets` / `Network`（`QLocalSocket`） |

---

*End of document (v2).*
