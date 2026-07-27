# MultiProcessShell 开发计划

> **English**：[../en/dev-plan.md](../en/dev-plan.md)  
> **地位**：近中期投入与架构姿态的备忘；产品愿景与里程碑仍以 [multiprocess-shell-spec.md](multiprocess-shell-spec.md) 为准，Demo 合约以 [demo-ipc.md](demo-ipc.md) / `.proto` 为准。  
> **更新**：2026-07-26（M6：心跳 / 无响应 UI）

---

## 1. 当前姿态

| 结论 | 说明 |
|------|------|
| **不做整仓重构** | Win Demo 主路径已可验收；`frame` / `EnvelopeChannel` / `tab_strip` / `TabEmbedMap` 已是可用的深模块缝 |
| **主线按规格里程碑推进** | **M4b / M5 / M6 已落地**（心跳 2s、超时 6s、Tab「无响应」+ 终止）；其后多 Backend / 可选 M7 |
| **加深绑在下一功能上** | tear-out 规则模块化（G6）与 REQ 超时择机 |

已有好模式：`src/common/tab_strip.hpp`、`tab_embed_map.hpp`、`envelope_builder.hpp`（纯规则 / 薄 helper + Host·Client / 单测共用）。

---

## 2. 加深候选（已记录，待择机讨论）

强度来自 2026-07-25 架构扫描（热点：`shell_window` / `shell_app` / embed）。

| 强度 | 候选 | 要点 |
|------|------|------|
| **Strong（首选）** | `wid` 收到 embed seam 后面 | **已落地**：`TabInfo` 无 wid；`EmbedContainer` + `TabEmbedMap` 以 `tabId` 绑定/激活/交接；握手瞬间仍经 Session 信号传 wid |
| **Strong** | 按 `tab_strip` 模式抽出 tear-out / merge / 壳生命周期规则 | 缩小 `ShellApp` / `ShellWindow` 相对 Interface 的浅层感 |
| Done | Session 侧 Envelope helper | **`envelope_builder.hpp`**：`makeEnvelope` / `makeResponse`；Host/Client 发送路径已改用 |
| Worth exploring | 落地 `IEmbedBackend`（可先仅 Win adapter） | **与 wid 迁移捆绑**；单独抽接口时仍是假想缝（one adapter）— 待第二 Backend 时再做 |

**扫描 Top recommendation：** ~~先做 wid → embed~~ **完成**；改 tear-out 时顺带拆规则模块。

---

## 3. 明确暂不做

- 为换 IPC 栈（gRPC / Cap’n / Zenoh）预重构（见 [ipc-alternatives.md](ipc-alternatives.md)）  
- 空抽第二平台 Backend 而无第二 Adapter 计划  
- 把 Host UI 与进程生命周期一次性「大拆」成规格全文对象树（`ClientPage` 等）——按需加深，不一次对齐纸面

---

## 4. 与规格里程碑的衔接（备忘）

| 方向 | 建议姿态 |
|------|----------|
| ~~M4b Python Hello~~ | **完成**：`hello_client.py` + 离线 `test_frame_envelope.py` + Qt 烟测 `mps_tests_m4b_hello` |
| M5 拖出 / 合入 | **可关账**（G1/G4 已修；G2/G3 Demo Client no-op）— [m5-gap-audit.md](m5-gap-audit.md) |
| ~~M6 心跳 / 无响应 UI~~ | **完成**：`heartbeat_policy.hpp`；Client 2s EVT；Host 6s Unhealthy；Tab 后缀 + 右键终止 |
| x11 / inproc / 多类型 Client | 前：~~wid 后置 +~~ `IEmbedBackend`（wid 已后置） |
| 接入 QThemeEngine | **集成当天再谈** Host 如何接 `qtheme::Engine`；现在不为集成预重构 |

---

## 5. 相关文档

| 文档 | 角色 |
|------|------|
| [multiprocess-shell-spec.md](multiprocess-shell-spec.md) | 产品愿景与里程碑 |
| [demo-morphology.md](demo-morphology.md) / [demo-acceptance.md](demo-acceptance.md) | Demo 形态与验收 |
| [demo-ipc.md](demo-ipc.md) | Demo IPC 合约 |
| [ipc-alternatives.md](ipc-alternatives.md) | 远期 IPC 备选 |
