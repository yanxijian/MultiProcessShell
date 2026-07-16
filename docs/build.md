# 构建说明 / Build

## 中文

### 依赖

- Windows：MSVC（x64）、CMake ≥ 3.21、Ninja（推荐）、Python 3.10+、Git（FetchContent）
- Qt：开源 **6.8+**（默认编译 Host/Client/Demo 时需要；**仅跑 M0 协议单测可不装 Qt**）
- Protobuf / GoogleTest：CMake **FetchContent**

### 环境变量

| 变量 | 用途 |
|------|------|
| `QTDIR` | Qt 安装前缀（默认 Demo 构建需要） |
| `PATH` | 应包含 `%QTDIR%\bin` |

工程引用：`%QTDIR%` / `$env:QTDIR`。

Windows 请在 **x64 vcvars / Native Tools** 环境中配置与编译（需要 `cl`、`rc`、`mt`）。

### 默认：编译 Demo + 部署

CMake 默认：`MPS_BUILD_SRC=ON`、`MPS_BUILD_DEMOS=ON`、`MPS_BUILD_TESTS=ON`。

```bat
python scripts\build_repo.py
:: 双击运行（无控制台）：
dist\Demo\mps_demo_host.exe
```

Windows 上 `build_repo.py` 成功后会调用 `scripts/deploy_demo.py`，用 `windeployqt` 把 Qt/CRT 拷到 exe 旁并同步到 `dist/Demo/`。也可单独：

```bat
python scripts\deploy_demo.py
```

Demo 目标为 **Windows GUI** 子系统（`WIN32_EXECUTABLE`），双击不会弹出额外控制台。

### M0（拼帧 + proto 单测，可不装 Qt）

```bat
python scripts\build_repo.py --no-demos --test
```

说明：`--no-demos` 同时关闭 `MPS_BUILD_DEMOS` 与 `MPS_BUILD_SRC`（两者当前一起开关）。

等价手动 CMake：

```bat
cmake -S . -B build -G Ninja ^
  -DMPS_BUILD_TESTS=ON -DMPS_FETCH_PROTOBUF=ON ^
  -DMPS_BUILD_DEMOS=OFF -DMPS_BUILD_SRC=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

首次配置会下载 protobuf 与 GoogleTest，需要网络。

### 辅助脚本

```bash
python scripts/build_qt.py --help      # 外置编译 Qt → QTDIR
python scripts/build_repo.py --help    # 配置/编译本仓
python scripts/deploy_demo.py --help   # Windows 部署 Qt 运行时
```

### 手动 CMake（含 Demo）

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

权威规格：[`docs/zh/multiprocess-shell-spec.md`](zh/multiprocess-shell-spec.md)  
Demo 形态 / IPC：[`demo-morphology.md`](zh/demo-morphology.md)、[`demo-ipc.md`](zh/demo-ipc.md)  
仓库 IDL：`proto/shell/ipc/v1/ipc.proto`

---

## English

### Dependencies

- Windows: MSVC (x64), CMake ≥ 3.21, Ninja (recommended), Python 3.10+, Git (FetchContent)
- Qt: open-source **6.8+** (required for default Host/Client/Demo; **optional for M0-only**)
- Protobuf / GoogleTest: CMake **FetchContent**

Use an **x64 vcvars / Native Tools** shell on Windows.

### Default: Demo + deploy

Defaults: `MPS_BUILD_SRC=ON`, `MPS_BUILD_DEMOS=ON`, `MPS_BUILD_TESTS=ON`.

```bat
python scripts\build_repo.py
:: double-click (no console):
dist\Demo\mps_demo_host.exe
```

On Windows, `build_repo.py` runs `scripts/deploy_demo.py` after a successful build. Demo binaries use the **Windows GUI** subsystem.

### M0 (no Qt)

```bat
python scripts\build_repo.py --no-demos --test
```

`--no-demos` also sets `MPS_BUILD_SRC=OFF`.

### Environment

| Variable | Purpose |
|----------|---------|
| `QTDIR` | Qt install prefix |
| `PATH` | should include `%QTDIR%\bin` |

### Scripts

```bash
python scripts/build_qt.py --help
python scripts/build_repo.py --help
python scripts/deploy_demo.py --help
```

Spec: [`docs/en/multiprocess-shell-spec.md`](en/multiprocess-shell-spec.md)  
Demo: [`demo-morphology.md`](en/demo-morphology.md), [`demo-ipc.md`](en/demo-ipc.md)  
IDL: `proto/shell/ipc/v1/ipc.proto`
