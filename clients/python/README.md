# clients/python

> **English**：[../docs/en/clients-python.md](../docs/en/clients-python.md)

里程碑 **M4b**：Python 烟测客户端，走与 C++ 相同的长度前缀 `Envelope` + `Hello` / `HelloAck`（`EMBED_NONE`）。

## 依赖

```bat
python -m pip install -r requirements.txt
```

`shell/ipc/v1/ipc_pb2.py` 已入库；改 `.proto` 后重新生成：

```bat
python ..\..\scripts\gen_python_proto.py --protoc <protoc>
```

## 离线单测（无 Host）

```bat
python test_frame_envelope.py
```

## 对接 Host / 烟测 harness

需本机有一个 `QLocalServer` 在 `--endpoint` 上监听，并在收到 `Hello` 后回 `HelloAck`。  
CMake 目标 `mps_tests_m4b_hello`（需 Qt）会拉起本脚本并完成握手。

手动：

```bat
python hello_client.py --endpoint mps-demo-<token> --server-path <fullServerName>
```

成功时打印 `HelloAck ok ...` 并以退出码 `0` 结束；后续若 Host 再发 `CreateSubWindow` 会被忽略（烟测不嵌入）。
