# MultiProcessShell

**한국어** · [English](README.md) · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

[![CI](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml/badge.svg)](https://github.com/yanxijian/MultiProcessShell/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

C++/Qt 멀티 프로세스 셸: **Host 외곽 + Client 네이티브 창 임베드 + Protobuf IPC**.

1단계 대상: **Windows(형태 A)**. macOS / Linux 디렉터리는 자리만 잡아 둔 상태입니다.

## 기능

- **멀티 프로세스 UI 셸**: 탭과 수명 주기는 Host가 맡고, Client는 네이티브 HWND로 임베드된다(`SetParent`)
- **떼어낼 수 있는 탭**: 밖으로 빼기/합치기, MRU 기준 탭 닫기, 빈 셸 규칙(순수 규칙 모듈 + 단위 테스트)
- **Protobuf IPC**: 기준 IDL은 `proto/shell/ipc/v1/ipc.proto`
- **`wid`는 임베드 경계 뒤에**: 탭 모델은 `tabId`만 알고, 플랫폼 핸들은 `EmbedContainer` / `TabEmbedMap`이 가진다
- **원클릭 데모 배포**: Windows에서는 `windeployqt`가 자동으로 돌아 `build-shared/demos/mps_demo_host.exe`가 바로 실행된다

## 요구 사항

| 항목 | 내용 |
|------|------|
| Qt | **6.8 이상**(데모/Host 빌드에 필요. 프로토콜만 보는 단위 테스트는 Qt 없이 가능) |
| 툴체인 | CMake, Ninja, Python 3. Windows에서는 MSVC x64(`vcvars`) |
| 의존성 | Protobuf **v35.1**(FetchContent), 권장 [AbseilPin](https://github.com/yanxijian/AbseilPin) `20260107.1`(`-DMPS_ABSEIL_PIN_PREFIX=`, 이웃 디렉터리에서 자동 탐지) |
| 선택 | 로컬 서식 검사용 `clang-format` 20 |

## 빠른 시작(Windows)

1. **x64 Native Tools / vcvars** 환경을 연다.
2. `QTDIR`을 Qt **6.8 이상** 프리픽스로 지정하고 `%QTDIR%\bin`을 `PATH`에 추가한다.
3. 권장: [QThemeEngine](https://github.com/yanxijian/QThemeEngine)(QTE) → [QFluentRibbon](https://github.com/yanxijian/QFluentRibbon)(QFR) → MPS 순서로 로컬 프리픽스에 설치한다(공유 라이브러리, 빌드 트리는 `build-shared`):

```bat
:: QTDIR = Qt 6.8+ 프리픽스, PREFIX = 설치 위치(생략하면 세 저장소에 이웃한 prefix/)
set QTDIR=<Qt-6.8+-prefix>
set PREFIX=<install-prefix>
python scripts\install_stack.py --prefix %PREFIX%
```

4. 실행한다(추가 콘솔 창 없이):

```text
build-shared\demos\mps_demo_host.exe
```

이 저장소만 빌드(출력은 기본적으로 `build-shared`. Windows에서는 `deploy_demo.py` / `windeployqt`가 자동 실행):

```bat
python scripts\build_repo.py
```

프로토콜과 탭 규칙 단위 테스트만 실행(Qt 불필요):

```bat
python scripts\build_repo.py --no-demos --test
```

더 자세한 빌드 옵션은 [build.md](docs/en/build.md), CI는 [ci.md](docs/en/ci.md)를 보세요.

## 저장소 구조

```text
cmake/            Qt / Protobuf 보조
proto/            shell.ipc.v1 IDL
src/              Host / Client / common / ipc_qt
demos/            mps_demo_host / mps_demo_client
tests/            프로토콜과 탭 스트립 단위 테스트
scripts/          install_stack / build_repo / build_qt / deploy_demo
clients/python/   M4b Python Hello 스모크
docs/zh|en/       중국어와 영어 문서
```

## 문서

| 주제 | English | 简体中文 |
|------|---------|----------|
| 제품 사양 | [multiprocess-shell-spec.md](docs/en/multiprocess-shell-spec.md) | [multiprocess-shell-spec.md](docs/zh/multiprocess-shell-spec.md) |
| 개발 계획 | [dev-plan.md](docs/en/dev-plan.md) | [dev-plan.md](docs/zh/dev-plan.md) |
| 데모 형태 | [demo-morphology.md](docs/en/demo-morphology.md) | [demo-morphology.md](docs/zh/demo-morphology.md) |
| 데모 인수 기준 | [demo-acceptance.md](docs/en/demo-acceptance.md) | [demo-acceptance.md](docs/zh/demo-acceptance.md) |
| 데모 IPC | [demo-ipc.md](docs/en/demo-ipc.md) | [demo-ipc.md](docs/zh/demo-ipc.md) |
| IPC 대안 | [ipc-alternatives.md](docs/en/ipc-alternatives.md) | [ipc-alternatives.md](docs/zh/ipc-alternatives.md) |
| 빌드 | [build.md](docs/en/build.md) | [build.md](docs/zh/build.md) |
| CI | [ci.md](docs/en/ci.md) | [ci.md](docs/zh/ci.md) |

긴 설계 문서는 영어와 중국어 간체 두 언어로만 관리합니다. 이 README가 네 언어 사이에서 동기화를 유지하는 문서입니다.

**원칙**: 긴 제품 설명과 `.proto`가 어긋나면 `.proto`와 데모 IPC를 기준으로 삼습니다.

## 현재 상태

| 기능 | 상태 |
|------|------|
| 프레이밍 + `shell.ipc.v1` + `mps_ipc_tests`(M0) | 완료 |
| 탭 분리 규칙 테스트 | 완료 |
| Windows 데모(홈 / Client 생성 / 분리 / 임베드) | 완료(M5 마무리 가능) |
| 임베드 경계 뒤의 `wid` | 완료 |
| M4b Python Hello | 완료 |
| M6 하트비트 / UI 무응답 | 완료 |
| 멀티 백엔드 / 선택 사항 M7 | 사양과 [dev-plan](docs/en/dev-plan.md) 참조 |

## 라이선스

[MIT License](LICENSE)로 배포합니다.
