# tests/

M0: framing + `Envelope` protobuf round-trip (`mps_ipc_tests`).

```bash
cmake -S . -B build -G Ninja -DMPS_BUILD_TESTS=ON -DMPS_FETCH_PROTOBUF=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

First configure downloads protobuf + GoogleTest via FetchContent (needs network).
