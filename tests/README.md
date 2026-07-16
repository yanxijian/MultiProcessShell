# tests/

> **English**：[../docs/en/tests.md](../docs/en/tests.md)

M0：拼帧 + `Envelope` protobuf 往返（`mps_ipc_tests`）。

```bash
python scripts/build_repo.py --no-demos --test
```

或手动：

```bash
cmake -S . -B build -G Ninja -DMPS_BUILD_TESTS=ON -DMPS_FETCH_PROTOBUF=ON -DMPS_BUILD_DEMOS=OFF -DMPS_BUILD_SRC=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

第一次配置会通过 FetchContent 下载 protobuf + GoogleTest（需要网络）。
