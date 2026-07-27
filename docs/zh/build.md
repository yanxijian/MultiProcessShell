# 构建说明

> **English**：[../en/build.md](../en/build.md)

## 架构：框架独立，Demo 组合

| 库 | 依赖 |
|----|------|
| `qte_engine` | 仅 Qt |
| `qfr_ribbon` | 仅 Qt（本地 `ribbon_tokens`；不链 QTE） |
| `mps_*` | Qt + protobuf（不链 QTE/QFR） |

QTE / QFR 仅由 **Demo** 链接：`mps_demo_host`→QTE，`mps_demo_client`→QFR+QTE。

## 推荐：本地 prefix

本地共享库惯例使用构建目录 **`build-shared`**（`install_stack.py` / `build_repo.py` 默认）。CI 仍可能用 `-B build`。

```bat
:: QTDIR = Qt 6.8+ 前缀；PREFIX = 安装根（可选，默认三仓同级的 prefix/）
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
:: 运行（在本仓根目录）：
build-shared\demos\mps_demo_host.exe
```

仅编本仓：

```bat
python scripts\build_repo.py
:: 等价于 --build-dir build-shared；Windows 上会自动 deploy_demo / windeployqt
```

## 依赖

- Windows：MSVC x64、CMake ≥ 3.21、Ninja、Python 3.10+
- Qt 6.8+
- 编 Demo 时：已安装的 QThemeEngine + QFluentRibbon（`CMAKE_PREFIX_PATH`）
- Protobuf：FetchContent（`MPS_BUILD_SHARED` 默认 ON → 共享 `libprotobuf.dll` + `abseil_dll.dll` + `utf8_validity.dll`）。生成的 `Envelope` 仍只编进 `mps_ipc.dll`，收包用 `EnvelopePtr` 在同模块内析构。

## 旁路源码（仅 Demo / CI）

默认推荐 **prefix + find_package**。旁路 embed 为可选：

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
