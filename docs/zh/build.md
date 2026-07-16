# 构建说明

> **英文文档**：[../en/build.md](../en/build.md)

## 依赖

- Windows：MSVC（x64）、CMake ≥ 3.21、Ninja（推荐）、Python 3.10+、Git（FetchContent）
- Qt：开源 **6.8+**（默认编 Host/Client/Demo 时需要；**只跑 M0 单测可不装 Qt**）
- Protobuf / GoogleTest：CMake **FetchContent**

## 环境变量

| 变量 | 用途 |
|------|------|
| `QTDIR` | Qt 安装前缀（默认 Demo 构建需要） |
| `PATH` | 应包含 `%QTDIR%\bin` |

工程里写 `%QTDIR%` / `$env:QTDIR`。

Windows 请在 **x64 vcvars / Native Tools** 里配置和编译（需要 `cl`、`rc`、`mt`）。

## 默认：编 Demo 并部署

CMake 默认：`MPS_BUILD_SRC=ON`、`MPS_BUILD_DEMOS=ON`、`MPS_BUILD_TESTS=ON`。

```bat
python scripts\build_repo.py
:: 双击运行（无控制台）：
dist\Demo\mps_demo_host.exe
```

Windows 上 `build_repo.py` 成功后会调用 `scripts/deploy_demo.py`（`windeployqt`），把 Qt/CRT 拷到 exe 旁并同步到 `dist/Demo/`。也可单独：

```bat
python scripts\deploy_demo.py
```

Demo 目标为 **Windows GUI** 子系统（`WIN32_EXECUTABLE`），双击不会多出一个控制台。

### 增量编译说明

`build_repo.py` 在 **选项未变** 时会跳过 cmake 重配（打印 `cmake configure: skipped`），避免无意义地重编 FetchContent 的 protobuf/abseil。

- 日常改业务代码：直接再跑 `python scripts\build_repo.py` 即可。  
- 需要强制重配：`--reconfigure`；彻底清空重来：`--fresh`（会重编 protobuf）。  
- 在 **同一 vcvars 环境** 下编译；脚本优先使用 PATH 上的 `cl`，减少工具链路径抖动。

## M0（拼帧 + proto 单测，可不装 Qt）

```bat
python scripts\build_repo.py --no-demos --test
```

说明：`--no-demos` 会同时关掉 `MPS_BUILD_DEMOS` 和 `MPS_BUILD_SRC`。

等价手动 CMake：

```bat
cmake -S . -B build -G Ninja ^
  -DMPS_BUILD_TESTS=ON -DMPS_FETCH_PROTOBUF=ON ^
  -DMPS_BUILD_DEMOS=OFF -DMPS_BUILD_SRC=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

第一次配置会下载 protobuf 与 GoogleTest，需要网络。

## 辅助脚本

```bash
python scripts/build_qt.py --help      # 外置编 Qt → QTDIR
python scripts/build_repo.py --help    # 配置/编译本仓
python scripts/deploy_demo.py --help   # Windows 部署 Qt 运行时
```

## 手动 CMake（含 Demo）

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

相关文档：[产品规格](multiprocess-shell-spec.md)、[Demo 形态](demo-morphology.md)、[Demo IPC](demo-ipc.md)  
仓库 IDL：`proto/shell/ipc/v1/ipc.proto`
