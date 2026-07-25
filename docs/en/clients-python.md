# clients/python

> **中文主文档**: [`../clients/python/README.md`](../clients/python/README.md)

Milestone **M4b** Python smoke client: same length-prefixed `Envelope` wire format as C++, `Hello` / `HelloAck` with `EMBED_NONE`.

```bash
python -m pip install -r requirements.txt
python test_frame_envelope.py
python hello_client.py --endpoint <name> --server-path <QLocalServer::fullServerName>
```

Regenerate stubs after `.proto` changes: `python scripts/gen_python_proto.py`.
