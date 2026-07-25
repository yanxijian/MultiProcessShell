# CI（GitHub Actions）

> **English**：[../en/ci.md](../en/ci.md)

工作流：[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)

| Job | Runner | 内容 |
|-----|--------|------|
| `clang-format` | ubuntu | `clang-format` 20（PyPI）+ `scripts/format_source.py --check` |
| `protocol + tab_strip` | ubuntu | 无 Qt：`MPS_BUILD_DEMOS/SRC=OFF`，跑 `mps_ipc_tests` + `mps_tab_strip_tests` |
| `Windows MSVC + Qt` | windows-latest | 安装 Qt 6.8.3，跑 `build_repo.py --test`（含 Demo） |

触发：`push`/`pull_request` 到默认分支，以及 `workflow_dispatch`。  
Dependabot 每周刷新 Actions 版本（[`.github/dependabot.yml`](../../.github/dependabot.yml)）。
