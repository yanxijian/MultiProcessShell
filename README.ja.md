# MultiProcessShell

**日本語** · [English](README.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md)

<!-- translated-from: README.md sha256:d6e25b36267b -->

[![CI](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml/badge.svg)](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

C++/Qt のマルチプロセスシェル：**Host の外枠 + Client のネイティブウィンドウ埋め込み + Protobuf IPC**。

第 1 フェーズの対象：**Windows（形態 A）**。macOS / Linux のディレクトリはプレースホルダーです。

## 特長

- **マルチプロセス UI シェル**：タブとライフサイクルは Host が持ち、Client はネイティブ HWND として埋め込まれる（`SetParent`）
- **タブの切り離し**：外へ出す／戻す、MRU によるタブのクローズ、空シェルの規則（純粋な規則モジュール＋単体テスト）
- **Protobuf IPC**：正となる IDL は `proto/shell/ipc/v1/ipc.proto`
- **`wid` は埋め込みの境界の内側**：タブモデルは `tabId` しか知らず、プラットフォームのハンドルは `EmbedContainer` / `TabEmbedMap` が持つ
- **ワンクリックのデモ配置**：Windows では `windeployqt` が自動で走り、`build-shared/demos/mps_demo_host.exe` がそのまま動く

## 動作要件

| 項目 | 内容 |
|------|------|
| Qt | **6.8 以降**（デモ／Host のビルドに必要。プロトコルだけの単体テストなら Qt は不要） |
| ツールチェーン | CMake、Ninja、Python 3。Windows では MSVC x64（`vcvars`） |
| 依存 | Protobuf **v35.1**（FetchContent）。推奨は [AbseilPin](https://github.com/yanxijian/AbseilPin) `20260107.1`（`-DMPS_ABSEIL_PIN_PREFIX=`、隣接ディレクトリから自動検出） |
| 任意 | ローカルの書式チェック用に `clang-format` 20 |

## クイックスタート（Windows）

1. **x64 Native Tools / vcvars** の環境を開く。
2. `QTDIR` を Qt **6.8 以降**のプレフィックスに設定し、`%QTDIR%\bin` を `PATH` に足す。
3. 推奨：[QThemeEngine](https://github.com/yanxijian/QThemeEngine)（QTE）→ [QFluentRibbon](https://github.com/yanxijian/QFluentRibbon)（QFR）→ MPS の順にローカルのプレフィックスへ入れる（共有ライブラリ、ビルドツリーは `build-shared`）：

```bat
:: QTDIR = Qt 6.8+ のプレフィックス、PREFIX = インストール先（省略時は 3 リポジトリに隣接する prefix/）
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
```

4. 実行する（余計なコンソールは開かない）：

```text
build-shared\demos\mps_demo_host.exe
```

このリポジトリだけをビルドする場合（出力先は既定で `build-shared`。Windows では `deploy_demo.py` / `windeployqt` が自動で走る）：

```bat
python scripts\build_repo.py
```

プロトコルとタブ規則の単体テストだけを走らせる場合（Qt は不要）：

```bat
python scripts\build_repo.py --no-demos --test
```

さらに細かいビルドオプションは [build.md](docs/en/build.md)、CI は [ci.md](docs/en/ci.md) を参照してください。

## リポジトリ構成

```text
cmake/            Qt / Protobuf の補助
proto/            shell.ipc.v1 の IDL
src/              Host / Client / common / ipc_qt
demos/            mps_demo_host / mps_demo_client
tests/            プロトコルとタブ帯の単体テスト
scripts/          install_stack / build_repo / build_qt / deploy_demo
clients/python/   M4b Python Hello のスモーク
docs/zh|en/       中国語と英語のドキュメント
```

## ドキュメント

| 内容 | English | 简体中文 |
|------|---------|----------|
| 製品仕様 | [multiprocess-shell-spec.md](docs/en/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](docs/zh/multiprocess-shell-spec.md) |
| 開発計画 | [dev-plan.md](docs/en/dev-plan.md) | [dev-plan.md](docs/zh/dev-plan.md) |
| デモの形態 | [demo-morphology.md](docs/en/demo-morphology.md) | [demo-morphology.md](docs/zh/demo-morphology.md) |
| デモの受け入れ | [demo-acceptance.md](docs/en/demo-acceptance.md) | [demo-acceptance.md](docs/zh/demo-acceptance.md) |
| デモの IPC | [demo-ipc.md](docs/en/demo-ipc.md) | [demo-ipc.md](docs/zh/demo-ipc.md) |
| IPC の代替案 | [ipc-alternatives.md](docs/en/ipc-alternatives.md) | [ipc-alternatives.md](docs/zh/ipc-alternatives.md) |
| ビルド | [build.md](docs/en/build.md) | [build.md](docs/zh/build.md) |
| CI | [ci.md](docs/en/ci.md) | [ci.md](docs/zh/ci.md) |

長文の設計メモは英語と簡体字中国語の 2 言語のみで管理しています。この README が 4 言語で同期を保つ文書です。

**方針**：長文の製品スケッチと `.proto` が食い違う場合は、`.proto` とデモの IPC を正とします。

## ステータス

| 機能 | 状態 |
|------|------|
| フレーミング + `shell.ipc.v1` + `mps_ipc_tests`（M0） | 完了 |
| タブ切り離しの規則テスト | 完了 |
| Windows デモ（ホーム／Client 生成／切り離し／埋め込み） | 完了（M5 は締められる） |
| 埋め込み境界の内側の `wid` | 完了 |
| M4b Python Hello | 完了 |
| M6 ハートビート／UI 無応答 | 完了 |
| マルチバックエンド／任意の M7 | 仕様と [dev-plan](docs/en/dev-plan.md) を参照 |

## ライセンス

[MIT License](LICENSE) で公開しています。
