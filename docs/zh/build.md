# 构建说明

> **English**：[../en/build.md](../en/build.md)

## 架构：框架独立，Demo 组合

| 库 | 依赖 |
|----|------|
| `qtheme_engine` | 仅 Qt |
| `qfluentribbon` | 仅 Qt（本地 `ribbon_tokens`；不链 QTE） |
| `mps_*` | Qt + protobuf（不链 QTE/QFR） |

QTE / QFR 仅由 **Demo** 链接：`mps_demo_host`→QTE，`mps_demo_client`→QFR+QTE，`qfr_gallery`→QFR+QTE。

## 推荐：本地 prefix

```bat
set QTDIR=D:\Codes\Qt6.8.4
python scripts\install_stack.py --prefix D:\Codes\prefix
:: 运行：
MultiProcessShell\build-shared\demos\mps_demo_host.exe
```

## 依赖

- Windows：MSVC x64、CMake ≥ 3.21、Ninja、Python 3.10+
- Qt 6.8+
- 编 Demo 时：已安装的 QThemeEngine + QFluentRibbon（`CMAKE_PREFIX_PATH`）
- Protobuf：FetchContent（`MPS_BUILD_SHARED` 时为共享 `libprotobuf.dll` + `abseil_dll.dll` + `utf8_validity.dll`）。生成的 `Envelope` 仍只编进 `mps_ipc.dll`，收包用 `EnvelopePtr` 在同模块内析构。

## 旁路源码（仅 Demo）

```bat
cmake -S . -B build-embed -G Ninja -DCMAKE_PREFIX_PATH=%QTDIR% ^
  -DMPS_DEV_EMBED_QTE=ON -DMPS_DEV_EMBED_QFR=ON -DMPS_INSTALL=OFF
```

## 辅助脚本

```bash
python scripts/install_stack.py --help
python scripts/build_repo.py --help
```

相关：[qfr-demo-client.md](qfr-demo-client.md)、[CI](ci.md)
