# M5 差距审计（Tab 拖出 / 合入）

> **English**：[../en/m5-gap-audit.md](../en/m5-gap-audit.md)  
> **地位**：对照规格 §7 / §12.4 M5 与 Win Demo 实现的关账审计；**不**替代 [multiprocess-shell-spec.md](multiprocess-shell-spec.md)。  
> **日期**：2026-07-26（关账实现：G1 / G4 已修；G2 / G3 文档化为 Demo 限制）

## 结论

**Win Demo 可关账。** tear-out / merge / reattach 可用；规格 S5「拖出中 Create 排队」与「松手在标题栏按钮上不 tear-out」已落地。`SetDragSuppress` / `NotifyMainWindowReattachment` 的 Client 侧行为明确为 **Demo 接受的 no-op**（Host 仍发 EVT）。

| 统计 | 数量 |
|------|------|
| 已落地（含本次关账） | 11 |
| 未关 Blocker | 0 |
| 文档化为 Demo 限制 | G2、G3 |
| Nice / 后续加深 | G5–G7 |

## 1. M5 范围（规格）

来自 `multiprocess-shell-spec.md` §7、§9 S5、§12.1 / §12.4，以及 `demo-ipc.md`：

- Tab 拖出状态机；壳外松开 → 新壳；命中 Tab 条 → 合入；模型先行再 reattach  
- `SetDragSuppress`、源壳激活上一 Tab、空壳销毁  
- **S5**：拖出进行中对该 session **排队** `CreateSubWindow`  

## 2. 已落地（证据）

| 要求 | 证据 | 备注 |
|------|------|------|
| Tear-out / merge / reattach | `ShellApp::tearOutTab` / `mergeTab` / `EmbedContainer::transferBinding` | mime 仅 tabId |
| Suppress / Reattachment EVT（Host） | `setDragSuppress` / `notifyReattachment` | Client Demo no-op → §3 |
| 空壳销毁 / yield / 迟滞 / Esc | `tab_strip` + Host | 既有验收项 |
| **G1 S5 Create 排队** | `m_deferredCreatesDuringDrag`；`shouldDeferCreateDuringDrag`；`endTabDrag` → `flushCreatesDeferredDuringDrag` | `invokeNewWindow` 门闩 |
| **G4 按钮区松手不 tear-out** | `isReleaseOverWindowButtons` + `shouldCancelTearOutOverWindowButtons` | 与 Forbidden 光标一致 |

## 3. 缺口处理

| ID | 要求 | 严重度 | 关账方式 |
|----|------|--------|----------|
| ~~G1~~ | 拖出中排队 Create | ~~Blocker~~ | **已修** |
| G2 | Client 落实 `SetDragSuppress` | Should | **Demo 限制**：Client 忽略；见 [demo-ipc.md](demo-ipc.md) §8 |
| G3 | Client 响应 Reattachment | Nice | **Demo 限制**：同上 |
| ~~G4~~ | 松手在按钮上不 tear-out | ~~Should~~ | **已修** |
| G5 | Host DnD 自动化测 | Nice | 保持手工 [demo-acceptance.md](demo-acceptance.md) |
| G6 | 编排完全抽纯规则 | Nice | 择机加深（dev-plan） |
| G7 | caps/modal 禁拖 | Nice | 产品后续 |

### 关键事实（更新后）

| 问题 | 答案 |
|------|------|
| 「拖出中 CreateSubWindow 队列化」？ | **已有**（`m_deferredCreatesDuringDrag`） |
| `IEmbedBackend`？ | **仍不存在**（非 M5 Demo 门闩） |
| 纯规则抽取？ | **部分**（含 S5/按钮取消谓词单测） |

## 4. 明确不计入 M5

M6 心跳、多 Backend / `IEmbedBackend`、macOS、QTE、`canMergeInto` 协商、M7。

## 5. 关账检查清单

1. ~~G1 Create 排队~~ **done**  
2. ~~G2 文档化 Demo Client no-op~~ **done**  
3. ~~G4 按钮区松手~~ **done**  
4. 手工跑 [demo-acceptance.md](demo-acceptance.md)「拖出/合入」（含新增两项）  
5. 可选：G5–G7 不阻塞标 M5 完成  

## 6. 相关文档

| 文档 | 角色 |
|------|------|
| [multiprocess-shell-spec.md](multiprocess-shell-spec.md) §7 / §12.4 | 权威要求 |
| [demo-acceptance.md](demo-acceptance.md) | 手工关账清单 |
| [demo-ipc.md](demo-ipc.md) | Suppress / Reattachment / S5 |
| [demo-morphology.md](demo-morphology.md) | 落点与排队行为 |
| [dev-plan.md](dev-plan.md) | 下一优先 M6 |
