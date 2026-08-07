# Demo 验收清单（Windows 形态 A）

> **English**：[../en/demo-acceptance.md](../en/demo-acceptance.md)  
> **配套**：[demo-morphology.md](demo-morphology.md)、[demo-ipc.md](demo-ipc.md)  
> **运行**：`build-shared/demos/mps_demo_host.exe`（先按 [build.md](build.md) 构建部署）

手工回归用。每条勾选通过后再谈后续打磨（Tab 溢出、REQ 超时、构建脚本等）。

## 环境

- [ ] 双击 Host 可启动；无多余控制台窗
- [ ] 启动后仅一顶层壳 + **Home** Tab

## 创建与标题

- [ ] Home（**demo_host** 注入的客户区）→「创建 Client」→ 出现 `Tab1`，客户区为 **无系统标题栏的 Ribbon ContentView**、铺满
- [ ] Ribbon「New Window」→ `Tab2`，同进程多 Tab
- [ ] Home → Light / Dark → 壳变浅/深；已嵌入 Client Ribbon 同步
- [ ] Ribbon Theme Light/Dark → Host 壳与其他 Client 进程一并切换
- [ ] 先 Dark 再 Create Client → 新进程首屏即为 Dark
- [ ] 同 Client「New Window」无需再切，自动跟随
- [ ] 再回 Home「创建 Client」→ `Tab1`

## 关 Tab / MRU

- [ ] 激活 A→B→C，关当前 C → 回到 B（不是强制 Home）
- [ ] 中键点击 Client Tab → 关闭该 Tab 及对应 Client 子窗（与点 × 相同）
- [ ] 中键点 Home → 无效果
- [ ] 关掉某 Client 全部子窗 Tab 后，可再回 Home 创建

## 同窗重排

- [ ] 同一壳内拖动 Client Tab：Tab 幽灵跟手；其它 Tab **实时让位**（无蓝色竖线）
- [ ] 松手后 Tab 顺序改变；切换各 Tab 嵌入内容仍正确
- [ ] Home 始终最左，无法排到 Home 前
- [ ] Esc 取消时幽灵弹回，顺序不变

## 拖出 / 合入

- [ ] 拖离 Tab 条超过离开阈值 → 出现窗口内容预览；预览 Tab 栏垂直居中包裹 Tab 幽灵（幽灵在上）
- [ ] 窗口预览出现后，源壳原 Tab 空位立刻被相邻 Tab 占住（无需松手）
- [ ] 快速上下拖时，预览与 Tab 幽灵保持包裹对齐、无明显错位
- [ ] 拖回条附近（返回迟滞）→ 窗口预览消失，回到条内让位
- [ ] 拖 Client Tab 到壳外松开 → 新顶层壳出现该 Tab；无明显长时间闪黑
- [ ] 若原壳无剩余 Client Tab（仅 Home）且不是唯一壳 → 原壳销毁
- [ ] 拖到另一壳 Tab / 条末空白 → 合入（让位指示落点）；源壳规则同上
- [ ] 拖到最小化 / 最大化 / 关闭上 → 禁止光标，不合入；**松开不 tear-out**（取消回源）
- [ ] 拖出进行中在 Client 点「新建窗口」→ 松手后再出现新 Tab（Create 排队至拖结束）
- [ ] 拖出/合入后嵌入区铺满，壳可聚焦

## 关壳 / 杀进程

- [ ] 点多余壳右上角 × → 该壳关闭；其 Client Tab 从 UI 移除；Host 不崩
- [ ] 关掉**最后一个**顶层壳 → 整个 Demo 退出
- [ ] 在任务管理器结束某个 `mps_demo_client` → 对应 Tab 消失；Host 不崩；其余 Client 仍可用

## M6 心跳 / 无响应

- [ ] 正常 Client：Tab 标题无「无响应」后缀
- [ ] `MPS_CLIENT_NO_HEARTBEAT=1` 启动 Host 后创建 Client → 约 6s 内 Tab 变为「…（无响应）」；Host 不崩、不自动杀进程
- [ ] 右键无响应 Tab →「终止进程」→ 该会话 Tab 消失（同杀进程清理）
- [ ] 心跳恢复（若曾暂停后恢复）→「无响应」后缀消失（可选抽测）

## 基本稳定

- [ ] 连续：创建两 Client → 各建两窗 → 重排 → 拖出 → 合回 → 关 Tab → 关壳，无崩溃
- [ ] （可选）Alt KeyTip / Backstage：记录是否被宿主裁切；失败不阻断主路径（见 [qfr-demo-client.md](qfr-demo-client.md)）

## M4b（多语言 Hello，可选）

- [ ] `python clients/python/test_frame_envelope.py` 通过
- [ ] `ctest --test-dir build-shared -R M4bPythonHello`（或跑 `mps_tests_m4b_hello`）通过：Python `Hello(EMBED_NONE)` ↔ Host 侧 `HelloAck`
