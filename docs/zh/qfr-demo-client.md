# Demo Client：QFluentRibbon 嵌入页

> **地位**：说明 Demo Client 页如何用无系统标题栏的 QFR `RibbonWindow` 作为嵌入 HWND。  
> **日期**：2026-07-26

## 关系

| 进程 | 职责 |
|------|------|
| `mps_demo_host` | 壳、Tab、`EmbedContainer::SetParent`（**不**链 QFR） |
| `mps_demo_client` | `qtheme::Engine` + `ThemeBridge` + frameless `RibbonWindow` 页 |

IPC / tear-out 协议不变；Host 仍剥 caption 并强制 `WS_CHILD`。

## 构建依赖

默认 Demo 构建需要旁路源码：

- `../QFluentRibbon`（或 `-DMPS_QFR_SOURCE_DIR=...`）
- QFR 再解析 `../QThemeEngine`（或 `QFR_QTE_SOURCE_DIR`）

见根目录 [`cmake/MPSQFluentRibbon.cmake`](../../cmake/MPSQFluentRibbon.cmake)。QTE/QFR 主题资源编进静态库；运行时仍靠现有 `windeployqt` 部署 Qt。

## Client 页行为

- 标志：`Qt::Window | Qt::FramelessWindowHint` + `WA_NativeWindow`
- 流程：`createWinId` → 建 Ribbon →（Win 上先 `WA_DontShowOnScreen`）→ `SubWindowAdded` → 嵌入后按 HWND 同步逻辑尺寸再首绘
- Ribbon：Home / Insert / View + QAT 钉选；中央「新建窗口」
- 「新建窗口」仍走 `Invoke("demo.request_new_window")`

## 嵌入态注意（冒烟记录）

| 能力 | 嵌入态建议 |
|------|------------|
| Ribbon 命令 / ScreenTip | 首切片验收目标；ScreenTip 已 `install` |
| Light / Dark | Client 进程内 `Engine::setColorScheme`；**不**与 Host 同步皮肤 |
| KeyTip（Alt） | `Qt::Tool` 角标可能被宿主裁切或跑出工作区；嵌入态勿作为硬验收 |
| Backstage | 覆盖中央区，一般可用；若与 Host 焦点抢占冲突可再关 |
| 改 `windowFlags` 于嵌入后 | **禁止**（易重建 HWND，打断 `SetParent`） |

## 另一条产品线

把 QFR 接到 **Host 壳**（中央仍嵌普通 Demo Client）见 QFluentRibbon [`mps-integration.md`](https://github.com/yanxijian/QFluentRibbon/blob/main/docs/zh/mps-integration.md) 的 Host 路径；与本 Client 嵌入路径并列，互不替代。
