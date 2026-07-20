# tests/

> **English**：[../docs/en/tests.md](../docs/en/tests.md)

- **M0** `mps_ipc_tests`：拼帧 + `Envelope` protobuf 往返。  
- **可撕出 Tab 条** `mps_tab_strip_tests`：激活/MRU、条内让位与 Home 钉死、撕出迟滞与占位、合入/空壳销毁规则（纯逻辑，无 Qt UI）。

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

规则实现：`src/common/tab_strip.hpp`（Host 与测试共用）。改 Tab 交互时优先补这里的用例，再改 Widget。
