# IPC 备选方案（远期参考）

> **English**：[../en/ipc-alternatives.md](../en/ipc-alternatives.md)  
> **状态**：备选清单，**非**当前实现。  
> **当前主路径**：仍见 [demo-ipc.md](demo-ipc.md) 与规格 §5 —— **Protobuf + 长度前缀 Envelope + Named Pipe / UDS**。

本文对照常见 C++ / 跨语言 IPC 方案，并结合 **MultiProcessShell（壳 + 嵌入）** 的约束，整理远期备选，供以后换栈或扩展时参考。  
**未做决议前，不得用本文替代仓库已冻结的 Demo / `.proto` 合约。**

---

## 1. 当前主路径（提醒）

| 项 | 选择 |
|----|------|
| 序列化 / IDL | Protobuf（`shell.ipc.v1`） |
| 帧 | `uint32` BE 长度 + `Envelope` |
| 传输 | Windows Named Pipe；macOS/Linux UDS；Host 用 `QLocalSocket` 作字节流 |
| 明确不做（本期） | gRPC 运行时、Thrift、Mojo、JSON 主协议 |
| 嵌入 | 平台 EmbedBackend（与控制面 IPC **分离**） |

主路径优点：依赖可控、与 Qt 字节流好接、多语言可共用同一 `.proto`（如 M4b Python 烟测）、不绑 Chromium 构建。

---

## 2. 选型时先分清场景

通用产品常按「语言边界 / 通信模式」选栈；落到本仓库时建议再拆一层：

```text
通信双方主要是？
  ├── Host(C++/Qt) ↔ Client(C++/Qt) 壳编排、窗口生命周期
  │     └── 当前：Protobuf 自研帧（已落地）
  │         备选：Cap'n Proto（更在意编解码 CPU）
  ├── C++ 引擎 ↔ 各端原生 UI（Swift / Kotlin / ArkTS / …）
  │     └── 备选优先：gRPC（跨语言 stub 成熟）
  └── 多端异步状态 / 弱网 / 发布订阅
        └── 备选：Zenoh；或自研 EVT 流 + 现有 Envelope
```

**壳项目特点**：控制面消息通常小而频繁（建窗、激活、关 Tab、心跳），不是大块共享内存业务流；另有一条**嵌入通道**（`SetParent` / XEmbed / 同进程）不走 Protobuf。

---

## 3. 底层通道（各端共识，可保留）

无论上层用 Protobuf / Cap'n / gRPC，同机优先：

| 平台 | 优先通道 |
|------|----------|
| Windows | Named Pipe |
| Linux / macOS / Android | Unix Domain Socket (UDS) |
| iOS / 鸿蒙 Next | 沙箱允许的 UDS（如 App Group 共享目录下的 socket 文件） |

说明：

- **不要**把「跨机器 TCP」当壳 Demo 默认路径。  
- 移动端往往**不能自由 fork 子进程**（iOS Extension、鸿蒙 ExtensionAbility 等），多「进程」形态与桌面 Host/Client 不同；若以后做移动壳，需单独写形态规格，不能照搬桌面 `QProcess`。  
- 移动端后台可能被冻结：备选栈必须能**断线重连 / 消息缓存**（或应用层自建）。

这些与当前规格「Win Pipe / *nix UDS」一致，可视为长期不变的底座。

---

## 4. 备选上层方案对照

| 方案 | 适合 | 相对本仓库 | 代价 / 风险 |
|------|------|------------|-------------|
| **继续 Protobuf + 自研帧**（现状） | Host↔Client 壳控制面；多语言共用 IDL | **主路径** | 需自管超时、重连、流控 |
| **gRPC**（同 `.proto` 或演进 IDL） | C++ 引擎 ↔ 多语言 UI；要现成 RPC/流/拦截器 | **远期可选**（规格 §5.10 已写） | 体积与依赖重（HTTP/2、TLS 等）；Windows 本地常走 loopback，不如 Pipe 直观 |
| **Cap'n Proto** | 纯 C++↔C++，极在意序列化 CPU / 移动端耗电 | 备选 | 生态与跨语言弱于 Protobuf；需**换 IDL 与生成代码**，与现 `shell.ipc.v1` 不兼容 |
| **Zenoh** | 多端 pub/sub、异步状态、弱网协同 | 备选（偏「状态总线」） | 模型与「壳 REQ/RES 生命周期」不完全同构；引入 Rust 核心或绑定 |
| **FlatBuffers / 类 zero-copy** | 大块只读状态、广播快照 | 局部备选 | 控制面小消息收益有限；勿与主 IDL 双轨并行 |
| **nng / nanomsg 类** | 想要现成 bus/pair 模式、少写传输层 | 备选 | 仍要自定消息体；与「一份 Protobuf IDL」目标可能重叠 |
| **平台总线**（如 Linux D-Bus） | 桌面与系统服务集成 | 仅特定平台胶水 | **不适合**作跨 Win/Mac 的壳主协议 |
| **Chromium Mojo** | 桌面强沙箱、句柄传递极强 | **不建议**中小团队作通用 IPC | 深度绑 GN/Chromium，剥离成本极高 |
| **Thrift / 换行 JSON** | — | **不采用**作控制面主协议 | 双 IDL 或文本脆弱；调试日志可用文本，勿当主路径 |

### 4.1 对本仓库较相关的几条备选

1. **Cap'n Proto（全 C++）**  
   - 仅当剖析证明 Protobuf 编解码成为热点、且 Client 长期纯 C++ 时再评估；迁移成本 = 新 IDL + 双栈过渡期。

2. **gRPC（跨语言 UI）**  
   - 若产品变成「薄壳 / 引擎 + 各端原生壳 UI」，优先认真评估。  
   - 理想路径：**尽量复用现有 `.proto` 语义**（或版本化升级），避免再维护一套平行合约；本期不引入运行时。

3. **Zenoh**  
   - 适合「多端状态同步 / 分布式」，不是壳 Tab 生命周期的第一选择。  
   - 若以后有「多设备协同编辑同一工作区」之类，可作**旁路数据面**，控制面仍建议保持明确的 REQ/RES + 超时。

---

## 5. 额外建议

1. **双通道不要合并**  
   控制面（意图/状态）与嵌入面（几何/焦点/句柄）继续分离。换 gRPC/Cap'n 也只换控制面，不要在 RPC 里「命令对任意 HWND SetParent」。

2. **一份契约优先于换库**  
   换栈前先问：能否用现有 `Envelope` + 更好的重连/背压解决问题？很多痛点在**会话与生命周期**，不在序列化格式。

3. **若上 gRPC：UDS/Pipe 优先于默认 TCP**  
   同机应显式绑本地通道，并保留 token / ACL（与规格 §10 同级安全要求）。

4. **若上 Cap'n：先做 POC 指标**  
   固定场景（建窗 N 次、心跳、拖出合入）对比 CPU/延迟/包体积，再决定；不要仅因「零反序列化」口号换栈。

5. **移动端单独成章**  
   iOS / 鸿蒙等平台对进程与沙箱限制更严：桌面 Demo **不能**假设可直接移植进程模型；某 IPC 库「能编过多端」≠「壳形态在各端都能成立」。

6. **可选增强（仍挂在主路径上）**  
   - 控制面：更好的背压、幂等键、追踪 id（已有 `id` 可加强约定）。  
   - 大载荷：Envelope 外另开 **共享内存 / 文件映射** 传截图或大缓冲，RPC 只传句柄/token（类似「双通道」思想）。  
   - 调试：保留文本日志；禁止 JSON 成为第二主 IDL。

---

## 6. 何时重开选型（建议门槛）

满足任一再启动正式选型评审（并改规格 + Demo 合约）：

- 主 Client / UI 变为**多语言原生栈**，Protobuf 手写帧联调成本显著高于 gRPC stub。  
- 性能剖析证明序列化占壳交互 CPU 的主要部分。  
- 产品范围扩展到**多设备 / 弱网协同**，现有 EVT 模型不够用。  
- 合规或体积要求无法继续带上当前 Protobuf 构建方式（可先考虑**预编译 protobuf** 或裁剪，再考虑换库）。

---

## 7. 相关文档

| 文档 | 角色 |
|------|------|
| [demo-ipc.md](demo-ipc.md) | **当前** Demo 合约（权威落地） |
| [multiprocess-shell-spec.md](multiprocess-shell-spec.md) §5 | 产品 IPC 规格与主路径 |
| `proto/shell/ipc/v1/ipc.proto` | 当前 IDL |
