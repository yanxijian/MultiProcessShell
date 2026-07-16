# 构建说明 / Build

## 中文

### 依赖

- Windows：MSVC（x64）、CMake ≥ 3.21、Ninja（推荐）、Python 3.10+
- Qt：开源 **6.8+**（兼容基线 6.8 LTS），通过环境变量 **`QTDIR`** 引用
- Protobuf：由 CMake **FetchContent** 拉取（开启 `-DMPS_FETCH_PROTOBUF=ON`；M0 起默认建议开启）

### 环境变量

| 变量 | 用途 |
|------|------|
| `QTDIR` | Qt 安装前缀 |
| `PATH` | 应包含 `%QTDIR%\bin` |

工程引用：`%QTDIR%` / `$env:QTDIR`。

### 辅助脚本（Python）

```bash
# 辅助编译外置 Qt（仅 qtbase 等最小集，安装到 QTDIR）
python scripts/build_qt.py --help

# 配置并编译本仓库（out-of-source: build/）
python scripts/build_repo.py --help
```

### 手动 CMake

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

当前阶段：顶层 CMake 可配置；`src/` 与 `demos/` **尚未实现**，默认不编译业务目标。

权威规格：[`docs/zh/multiprocess-shell-spec.md`](zh/multiprocess-shell-spec.md)

---

## English

### Dependencies

- Windows: MSVC (x64), CMake ≥ 3.21, Ninja (recommended), Python 3.10+
- Qt: open-source **6.8+** (compat baseline 6.8 LTS) via **`QTDIR`**
- Protobuf: CMake **FetchContent** (no system install required)

### Environment

| Variable | Purpose |
|----------|---------|
| `QTDIR` | Qt install prefix |
| `PATH` | should include `%QTDIR%\bin` |

Projects use `%QTDIR%` / `$env:QTDIR`.

### Scripts

```bash
python scripts/build_qt.py --help
python scripts/build_repo.py --help
```

### Manual CMake

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

At this stage the top-level project configures; **`src/` and `demos/` are not implemented yet**.

Spec: [`docs/en/multiprocess-shell-spec.md`](en/multiprocess-shell-spec.md)
