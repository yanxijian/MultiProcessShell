# MultiProcessShell 开发计划

> **English**：[../en/dev-plan.md](../en/dev-plan.md)  
> **地位**：近中期投入与架构姿态的备忘；产品愿景与里程碑仍以 [multiprocess-shell-spec.md](multiprocess-shell-spec.md) 为准，Demo 合约以 [demo-ipc.md](demo-ipc.md) / `.proto` 为准。  
> **更新**：2026-07-25（`codebase-design` + `improve-codebase-architecture` 评审结论入库；细节待下次讨论）

---

## 1. 当前姿态

| 结论 | 说明 |
|------|------|
| **不做整仓重构** | Win Demo 主路径已可验收；`frame` / `EnvelopeChannel` / `tab_strip` 已是可用的深模块缝 |
| **主线按规格里程碑推进** | 见规格 §12.4（M4b 多语言、多 Backend、心跳/超时、可选 M7 等） |
| **加深绑在下一功能上** | 在 M4b / 第二挂接 / 健康面之前，做 2～3 个针对性 deepening，而不是单独「重构季」 |

已有好模式：`src/common/tab_strip.hpp`（纯规则 + Host 与单测共用 Interface）。

---

## 2. 加深候选（已记录，待择机讨论）

强度来自 2026-07-25 架构扫描（热点：`shell_window` / `shell_app` / embed）。

| 强度 | 候选 | 要点 |
|------|------|------|
| **Strong（首选）** | `wid` 收到 embed seam 后面 | Tab 模型以 `tabId` 为主；与规格「wid 仅 Backend」对齐；挡住 Host 测与多 Backend |
| **Strong** | 按 `tab_strip` 模式抽出 tear-out / merge / 壳生命周期规则 | 缩小 `ShellApp` / `ShellWindow` 相对 Interface 的浅层感 |
| Worth exploring | Session 侧 Envelope / RPC helper | 收掉 `ClientSession` / `ClientApp` 手搓 pb，利于 M4b / 心跳 |
| Worth exploring | 落地 `IEmbedBackend`（可先仅 Win adapter） | **与 wid 迁移捆绑**；单独抽接口时仍是假想缝（one adapter） |

**扫描 Top recommendation：** 先做 **wid → embed**；改 tear-out 时顺带拆规则模块。

---

## 3. 明确暂不做

- 为换 IPC 栈（gRPC / Cap’n / Zenoh）预重构（见 [ipc-alternatives.md](ipc-alternatives.md)）  
- 空抽第二平台 Backend 而无第二 Adapter 计划  
- 把 Host UI 与进程生命周期一次性「大拆」成规格全文对象树（`ClientPage` 等）——按需加深，不一次对齐纸面

---

## 4. 与规格里程碑的衔接（备忘）

| 方向 | 建议姿态 |
|------|----------|
| M4b Python Hello | 前或同期：Envelope helper 更有杠杆 |
| x11 / inproc / 多类型 Client | 前：wid 后置 + `IEmbedBackend` |
| M6 心跳 / 无响应 UI | 协议逻辑进 SessionRpc 类模块，避免再散落调用方 |
| 接入 QThemeEngine | **集成当天再谈** Host 如何接 `qtheme::Engine`；现在不为集成预重构 |

---

## 5. 相关文档

| 文档 | 角色 |
|------|------|
| [multiprocess-shell-spec.md](multiprocess-shell-spec.md) | 产品愿景与里程碑 |
| [demo-morphology.md](demo-morphology.md) / [demo-acceptance.md](demo-acceptance.md) | Demo 形态与验收 |
| [demo-ipc.md](demo-ipc.md) | Demo IPC 合约 |
| [ipc-alternatives.md](ipc-alternatives.md) | 远期 IPC 备选 |
