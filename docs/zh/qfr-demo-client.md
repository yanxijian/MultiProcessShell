# Demo Client：QFluentRibbon 嵌入页

> **地位**：说明 Demo Client 页如何用无系统标题栏的 QFR `RibbonWindow` 作为嵌入 HWND。  
> **日期**：2026-07-26

## 关系

| 进程 | 职责 |
|------|------|
| `mps_demo_host` | 壳、Tab、`EmbedContainer::SetParent`；链接 **QTE**（`ThemeService` 为外观 SSOT）；**不**链 QFR |
| `mps_demo_client` | `qtheme::Engine` + `ThemeBridge` + frameless `RibbonWindow` 页 |

IPC / tear-out 协议不变；Host 仍剥 caption 并强制 `WS_CHILD`。

## 构建依赖

默认 Demo 构建需要旁路源码：

- `../QThemeEngine`（或 `-DMPS_QTE_SOURCE_DIR=...` / `find_package(QThemeEngine)`）— Host 与 Client 均依赖
- `../QFluentRibbon`（或 `-DMPS_QFR_SOURCE_DIR=...`）— 仅 Client；见 [`cmake/MPSQFluentRibbon.cmake`](../../cmake/MPSQFluentRibbon.cmake)

根目录先 `include(MPSQThemeEngine)`，再解析 QFR（复用已加载的 QTE，避免双份 Engine）。QTE/QFR 主题资源编进静态库；运行时仍靠现有 `windeployqt` 部署 Qt。

## Client 页行为

- 标志：`Qt::Window | Qt::FramelessWindowHint` + `WA_NativeWindow`
- 流程：`createWinId` → 建 Ribbon →（Win 上先 `WA_DontShowOnScreen`）→ `SubWindowAdded` → 嵌入后按 HWND 同步逻辑尺寸再首绘
- Ribbon：Home / Insert / View + QAT 钉选；中央「新建窗口」
- 「新建窗口」仍走 `Invoke("demo.request_new_window")`

## 全局 Light / Dark

| 角色 | 行为 |
|------|------|
| Host | `ThemeService` 持有 `ColorScheme`；`QSettings` 持久化；`Engine::apply` 驱动壳 chrome |
| 切换入口 | Home 页 Light/Dark；Client Ribbon Theme 组同等入口 |
| IPC | 双向 `Invoke("theme.set")`，params 仅 `"light"` / `"dark"`；其它值回 `ERROR_PROTOCOL`、不改肤 |
| 握手后 | `HelloAck` 后 Host 立即向该 session 推当前 scheme，避免新 Client 先闪默认肤 |
| 同进程多页 | 一进程一个 `Engine`，一次 `setColorScheme` 全窗生效 |

## 嵌入态注意（冒烟记录）

| 能力 | 嵌入态建议 |
|------|------------|
| Ribbon 命令 / ScreenTip | 首切片验收目标；ScreenTip 已 `install` |
| Light / Dark | **全局同步**（Host SSOT + `theme.set`）；见上表 |
| KeyTip（Alt） | `Qt::Tool` 角标可能被宿主裁切或跑出工作区；嵌入态勿作为硬验收 |
| Backstage | 覆盖中央区，一般可用；若与 Host 焦点抢占冲突可再关 |
| 改 `windowFlags` 于嵌入后 | **禁止**（易重建 HWND，打断 `SetParent`） |

## 另一条产品线

把 QFR 接到 **Host 壳**（中央仍嵌普通 Demo Client）见 QFluentRibbon [`mps-integration.md`](https://github.com/yanxijian/QFluentRibbon/blob/main/docs/zh/mps-integration.md) 的 Host 路径；与本 Client 嵌入路径并列，互不替代。
