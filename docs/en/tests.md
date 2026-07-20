# tests/

> **中文主文档**: [`../tests/README.md`](../tests/README.md)

- **M0** `mps_ipc_tests`: framing + `Envelope` protobuf round-trip.  
- **Detachable tab strip** `mps_tab_strip_tests`: activate/MRU, strip yield + Home pin, tear-out hysteresis/slot claim, merge/empty-shell rules (pure logic, no Qt UI).

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

Rules live in `src/common/tab_strip.hpp` (shared by Host and tests). Prefer adding a failing test there before changing Widget/DnD code.
