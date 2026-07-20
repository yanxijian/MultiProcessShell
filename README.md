# MultiProcessShell

> **English**：[docs/en/README.md](docs/en/README.md)

MIT 许可的 C++/Qt 多进程壳：Host 壳框 + Client 原生窗嵌入 + Protobuf IPC。

首期平台：**Windows（形态 A）**；macOS / Linux 目录占位。

## 文档

| 内容 | 中文（主） | English |
|------|------------|---------|
| 产品技术规格 | [docs/zh/multiprocess-shell-spec.md](docs/zh/multiprocess-shell-spec.md) | [docs/en/multiprocess-shell-spec.md](docs/en/multiprocess-shell-spec.md) |
| Demo 形态 | [docs/zh/demo-morphology.md](docs/zh/demo-morphology.md) | [docs/en/demo-morphology.md](docs/en/demo-morphology.md) |
| Demo 验收清单 | [docs/zh/demo-acceptance.md](docs/zh/demo-acceptance.md) | [docs/en/demo-acceptance.md](docs/en/demo-acceptance.md) |
| Demo IPC 合约 | [docs/zh/demo-ipc.md](docs/zh/demo-ipc.md) | [docs/en/demo-ipc.md](docs/en/demo-ipc.md) |
| IPC 备选（远期） | [docs/zh/ipc-alternatives.md](docs/zh/ipc-alternatives.md) | [docs/en/ipc-alternatives.md](docs/en/ipc-alternatives.md) |
| 构建说明 | [docs/zh/build.md](docs/zh/build.md) | [docs/en/build.md](docs/en/build.md) |

**约定**：日常以中文文档为准；英文为同步译本。Demo 权威 IDL 为 `proto/shell/ipc/v1/ipc.proto` + [Demo IPC](docs/zh/demo-ipc.md)。若长文规格草图与 `.proto` 冲突，以 `.proto` / Demo 合约为准。

## 目录

```text
MultiProcessShell/
  cmake/           Qt / Protobuf 辅助
  proto/           shell.ipc.v1 IDL
  src/             Host / Client / common / ipc_qt
  demos/           Demo（mps_demo_host / mps_demo_client）
  tests/           M0 协议单测
  clients/python/  M4b 烟测（后续）
  docs/zh|en/      中英文文档
  scripts/         build_repo / build_qt / deploy_demo
  dist/Demo/       Windows 可双击包（生成物，不入库）
```

## 快速开始（Windows）

1. 打开 **x64 Native Tools / vcvars** 环境。  
2. 设置 `QTDIR` 为 Qt **6.8+** 前缀，并把 `%QTDIR%\bin` 加入 `PATH`。  
3. 编译并部署：

```bat
python scripts\build_repo.py
```

4. 双击运行（无额外控制台）：

```text
dist\Demo\mps_demo_host.exe
```

默认会编 Host/Client Demo；Windows 上会自动跑 `scripts/deploy_demo.py`（`windeployqt`），把 Qt/CRT 拷到 exe 旁。

仅跑 M0 单测（可不装 Qt）：

```bat
python scripts\build_repo.py --no-demos --test
```

## 现状

- 规格、Demo 形态/IPC、脚本已入库。  
- **M0 完成**：拼帧 + `shell.ipc.v1` 生成 + `mps_ipc_tests`。  
- **Windows Demo 完成**：Home Tab、创建 Client、同 Client 新建窗口、关 Tab 激活历史、拖出/合入钩子、`SetParent` 嵌入、GUI 子系统（无控制台）。

## 许可

[MIT](LICENSE)
