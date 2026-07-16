# Demo 形态讨论（最小窗口嵌入）

> **状态**：讨论稿（尚无定稿，`demos/` 暂不实现代码）  
> **English**: [`../en/demo-morphology.md`](../en/demo-morphology.md)

## 目标

`demos/` 首期只证明一件事：**Host 容器能挂接另一个进程的原生窗口（Windows 形态 A）**，并走通最小 Protobuf 握手。

它**不是**完整 Tab 拖出/多类型验收套件；完整能力按规格里程碑后续交付。

## 建议的最小形态（待确认）

```text
┌─ demo_host (Qt) ─────────────────────────┐
│  QMainWindow                             │
│  ┌─ EmbedContainer (native) ───────────┐ │
│  │  ← SetParent(client_hwnd)           │ │
│  └─────────────────────────────────────┘ │
│  Status: connected / embedded / failed   │
└──────────────────────────────────────────┘
          │ Pipe + Envelope
          ▼
┌─ demo_client (Qt) ───────────────────────┐
│  无边框/可嵌入的 QWidget 顶层窗            │
│  纯色背景 + 标题文字（便于肉眼确认）        │
└──────────────────────────────────────────┘
```

### 验收（建议）

1. 启动 `demo_host`，自动或按钮拉起 `demo_client`。  
2. 完成 `Hello` → `HelloAck` → `MainWindowAdded(wid)`。  
3. Host 对 Client 主窗 `SetParent` 进容器，缩放 Host 时 Client 几何跟随。  
4. 结束 Client 进程后 Host 不崩，并显示断开状态。

### 明确不做（首个 Demo）

- TabBar / 多 Tab / 拖出合入  
- 多 Client 类型（alpha/beta）  
- 心跳 UI（可后补）  
- macOS / Linux 嵌入  

## 待讨论问题

1. **启动方式**：Host 内 `QProcess` 拉起 Client，还是手动先起 Client 再 Attach？  
2. **IPC 范围**：仅握手 + `MainWindowAdded`，还是顺带 `CreateWindow`？  
3. **Client UI**：纯色窗是否足够，是否要按钮回传事件证明双向 IPC？  
4. **与 `src/` 关系**：Demo 直接链 `src/common` + `src/host/embed/win`，还是 Demo 内短期自包含、稳定后再上收？  
5. **失败可视**：嵌入失败时 Host 显示错误文案 / 重试按钮？

确认后更新本文与 `demos/README.md`，再实现代码。
