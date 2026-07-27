# demos/

> **English**：[../docs/en/demos.md](../docs/en/demos.md)

| 目标 | 作用 |
|------|------|
| `mps_demo_host` | 浏览器式可撕出 Tab 壳（Home、Create Client、Tab、嵌入） |
| `mps_demo_client` | Client 进程（由 Host 拉起；「新建窗口」） |

形态 / IPC：[docs/zh/demo-morphology.md](../docs/zh/demo-morphology.md)、[docs/zh/demo-ipc.md](../docs/zh/demo-ipc.md)。

```bat
:: 在 vcvars x64 环境，并设置 QTDIR
:: 推荐整栈：set QTDIR=... 后 python scripts\install_stack.py（--prefix 可选）
python scripts\build_repo.py
:: 双击（GUI 子系统，无控制台）：
build-shared\demos\mps_demo_host.exe
```

Windows 上 `build_repo.py` 默认输出到 `build-shared`，并自动跑 `scripts/deploy_demo.py`（`windeployqt`）。

## 当前体验

- 固定 **Home** Tab（不可关、不可拖出）；「创建 Client」在 Home 页。  
- Client Tab：点 × **或中键**关闭；激活走 MRU 历史（不强制回 Home）。  
- 同 Client「新建窗口」：`Invoke("demo.request_new_window")` → Host `CreateSubWindow`。  
- Tab 拖拽：跟手 Tab 幽灵 + 条内实时让位；拖离条后窗口预览按 Tab **居中包裹**对齐，源壳空位立刻被占住；Esc 取消弹回；合入热区不含窗口按钮。