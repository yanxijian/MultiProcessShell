# CI (GitHub Actions)

> **中文**：[../zh/ci.md](../zh/ci.md)

Workflow: [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)

| Job | Runner | What |
|-----|--------|------|
| `clang-format` | ubuntu | clang-format 20 (PyPI) + `scripts/format_source.py --check` |
| `protocol + tab_strip` | ubuntu | No Qt: demos/src off; run IPC + tab_strip tests |
| `Windows MSVC + Qt` | windows-latest | Qt 6.8.3 + checkout sibling QTE/QFR + `build_repo.py --test -D MPS_DEV_EMBED_QTE=ON -D MPS_DEV_EMBED_QFR=ON` |

Triggers: push/PR to default branches, plus `workflow_dispatch`.  
Dependabot updates Actions weekly.
