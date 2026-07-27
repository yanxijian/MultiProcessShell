# Build guide

> **中文主文档**: [`../zh/build.md`](../zh/build.md)

## Architecture

Framework DLLs are independent: `qte_engine`, `qfr_ribbon`, and `mps_*` do **not** link each other. Demos compose them (`mps_demo_host`→QTE, `mps_demo_client`→QFR+QTE).

```bat
set QTDIR=D:\Codes\Qt6.8.4
python scripts\install_stack.py --prefix D:\Codes\prefix
```

Demo-only sibling embed: `-DMPS_DEV_EMBED_QTE=ON -DMPS_DEV_EMBED_QFR=ON -DMPS_INSTALL=OFF`.
