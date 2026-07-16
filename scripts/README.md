# scripts/（Python）

> **English**：[../docs/en/scripts.md](../docs/en/scripts.md)

| 脚本 | 作用 |
|------|------|
| `build_repo.py` | 配置/编译本仓（默认要 `QTDIR`；`--no-demos` 除外） |
| `build_qt.py` | 辅助外置编译 Qt（qtbase）到指定前缀 |
| `deploy_demo.py` | Windows：`windeployqt` 到 Demo 旁，并同步 `dist/Demo/` |

## Windows 注意

- 请在 **MSVC x64 Developer / vcvars** 环境中运行（`PATH` 上要有 `cl`、`rc`、`mt`）。  
- 默认会编 Demo + Host/Client；请设置 `QTDIR` 并把 `%QTDIR%\bin` 加入 `PATH`。  
- Demo 编成功后，`build_repo.py` 会自动跑 `deploy_demo.py`。  
- `--no-demos` 同时关掉 `MPS_BUILD_SRC`（只跑协议单测）。  
- **增量**：选项未变时跳过 cmake 重配，避免重编 protobuf；强制重配用 `--reconfigure`，清空用 `--fresh`。

```bat
python scripts\build_repo.py
python scripts\build_repo.py --no-demos --test
python scripts\deploy_demo.py
python scripts\build_qt.py --source <qt-everywhere-src> --build-dir <qt-build> --prefix %QTDIR%
```

可双击运行：`dist\Demo\mps_demo_host.exe`。  
更完整说明见 [docs/zh/build.md](../docs/zh/build.md)。
