# tests/

> **中文主文档**: [`../tests/README.md`](../tests/README.md)

M0: framing + `Envelope` protobuf round-trip (`mps_ipc_tests`).

```bash
python scripts/build_repo.py --no-demos --test
```

Or manually:

```bash
cmake -S . -B build -G Ninja -DMPS_BUILD_TESTS=ON -DMPS_FETCH_PROTOBUF=ON -DMPS_BUILD_DEMOS=OFF -DMPS_BUILD_SRC=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

First configure downloads protobuf + GoogleTest via FetchContent (needs network).
