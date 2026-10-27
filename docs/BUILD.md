# Naru Studio 개발 환경·빌드 가이드 (Windows)

다른 PC에서 clone한 뒤 이 순서대로 configure → build → test 한다. 버전은 2026-10-27에 실제 빌드를 확인한 조합이다.

## 1. 검증한 도구 조합

| 도구 | 버전 | 설치 방법 |
|---|---|---|
| Visual Studio 2022 Build Tools | MSVC x64 (C++ 데스크톱 개발 워크로드 + Windows SDK) | `winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"` |
| CMake | 4.4.4 (최소 3.25) | `winget install --id Kitware.CMake -e` |
| Ninja | 1.13.2 | `winget install --id Ninja-build.Ninja -e` |
| Qt | **6.8.3 LTS**, `win64_msvc2022_64` kit, Widgets | 아래 2절 |
| Python | **3.14.3 x64** (python.org 배포본/Python Install Manager, 3.13 이상 필요) | 설치 폴더에 `include\Python.h`, `libs\python3xx.lib`가 있어야 한다 |
| SDL3 | **3.4.18** (`SDL3-devel-3.4.18-VC.zip`) | 아래 2-1절. 게임 실행 화면(NaruPlayer)의 창·그리기·입력 |
| SDL3_ttf | **3.2.2** (`SDL3_ttf-devel-3.2.2-VC.zip`) | 아래 2-1절. 대사 글자 그리기 |

Visual Studio IDE, Qt Creator, VS Code는 선택 사항이다. 설치한 도구·SDK는 Git에 넣지 않는다.

## 2. Qt 설치

공식 Qt Online Installer(계정 필요)로 Qt 6.8.3 → MSVC 2022 64-bit를 설치하거나, 계정 없이 aqtinstall을 사용한다.

```powershell
py -3 -m venv $env:LOCALAPPDATA\naru-tools\aqt-venv
& $env:LOCALAPPDATA\naru-tools\aqt-venv\Scripts\python.exe -m pip install aqtinstall pip-system-certs
& $env:LOCALAPPDATA\naru-tools\aqt-venv\Scripts\aqt.exe install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:\Qt
```

`pip-system-certs`는 Python이 Windows 인증서 저장소를 쓰게 한다. 보안 프로그램/프록시가 HTTPS를 검사하는 PC에서 aqt가 `SSLError`로 실패할 때 필요했다.

## 2-1. SDL3 설치 (게임 실행 화면)

공식 GitHub 릴리스의 Visual C++ 개발 패키지를 `%LOCALAPPDATA%\naru-tools`에 풀면 `dev.cmd`가 자동으로 찾는다. 다른 위치에 두면 `NARU_SDL3_DIR`, `NARU_SDL3_TTF_DIR` 환경변수로 지정한다.

```powershell
$d = "$env:LOCALAPPDATA\naru-tools"; New-Item -ItemType Directory -Force $d | Out-Null
Invoke-WebRequest https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-VC.zip -OutFile $d\SDL3.zip
Invoke-WebRequest https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-devel-3.2.2-VC.zip -OutFile $d\SDL3_ttf.zip
Expand-Archive $d\SDL3.zip $d; Expand-Archive $d\SDL3_ttf.zip $d
```

2026-10-27에 받은 파일의 SHA-256:

| 파일 | SHA-256 |
|---|---|
| SDL3-devel-3.4.18-VC.zip | `78D84602AE616CFE26B33A73B7D3B9B6B56E8A6650E53D92C9C6CA938F747ACD` |
| SDL3_ttf-devel-3.2.2-VC.zip | `67805C5BABFC49CA0C56882DC9B8CABBCDD1E6F9EDDE10DDAC91DDB38F3AFB8C` |

SDL3·SDL3_ttf는 zlib 라이선스다. 빌드 후 `SDL3.dll`, `SDL3_ttf.dll`이 bin 폴더에 복사된다. SDL3 없이 빌드하려면 `-DNARU_BUILD_PLAYER=OFF` (이 경우 제작툴의 "게임 실행"은 실행 파일이 없다고 알린다).

## 3. 빌드

### 개발 중: 빌드하고 바로 실행 — `dev.cmd`

저장소 폴더의 **`dev.cmd`를 더블클릭**하거나 터미널에서 실행한다. VS 개발자 환경·Qt·Python·SDL3 경로를 자동으로 찾고, 매번 CMake 설정을 갱신한 뒤 빌드한다.

| 명령 | 하는 일 |
|---|---|
| `dev` (더블클릭) | 빌드 → **마지막으로 연 프로젝트로 Naru Studio 실행** |
| `dev check` | 빌드 → 전체 테스트 → 모두 통과하면 실행 (기능을 고친 뒤 점검할 때) |
| `dev test` | 빌드 → 전체 테스트만 |
| `dev build` | 빌드만 |
| `dev clean` | 빌드 폴더를 지우고 처음부터 다시 빌드 → 실행 |

빌드나 테스트가 실패하면 실행하지 않고 멈춘다(더블클릭한 경우 창이 열린 채로 기다린다). 처음 실행하면 CMake 설정부터 하므로 몇 분 걸린다.

IDE(Visual Studio, VS Code CMake Tools)에서는 CMake 타깃 **`run`**을 실행해도 된다: `cmake --build --preset windows-msvc --target run`.

### 직접 빌드

PowerShell 도우미 (VS 개발자 환경을 자동으로 불러온다):

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

- Qt 경로: `-QtDir` → `NARU_QT_DIR` → `C:\Qt\6.*\msvc2022_64` 자동 탐색
- Python 경로: `-PythonDir` → `NARU_PYTHON_DIR` → `py -3`의 `sys.base_prefix`
- `-SkipTests`, `-Clean`, `-Preset windows-msvc-engine-only`(Qt·Python 없이 엔진만)

직접 실행하려면 **x64 Native Tools Command Prompt for VS 2022**에서:

```bat
set NARU_QT_DIR=C:\Qt\6.8.3\msvc2022_64
set NARU_PYTHON_DIR=C:\경로\Python314
set NARU_SDL3_DIR=%LOCALAPPDATA%\naru-tools\SDL3-3.4.18
set NARU_SDL3_TTF_DIR=%LOCALAPPDATA%\naru-tools\SDL3_ttf-3.2.2
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

PC별 경로를 고정하고 싶으면 `CMakeUserPresets.json`(Git 제외)에 `CMAKE_PREFIX_PATH`, `Python3_ROOT_DIR`을 넣은 preset을 만든다.

## 4. 결과물

`build\windows-msvc\bin\`

| 파일 | 내용 |
|---|---|
| `NaruStudio.exe` | Qt 제작툴 (아래 사용법) |
| `NaruPlayer.exe` | 게임 실행 화면 (SDL3). 제작툴의 Ctrl+R이 띄운다. 직접: `NaruPlayer <프로젝트 폴더> [--scene <id>] [--font <ttf>] [--frames <n>] [--auto-confirm] [--hold <동작>:<시작>-<끝>]... [--screenshot <png>]` (`--hold right:0-80`처럼 프레임 구간 동안 키를 누른 것으로 친다. 동작: up down left right confirm cancel) |
| `naru_game_tests.exe` | 게임 실행 상태(장면 진입·자동 실행·병렬 처리·대사 대기·오류 기록·플레이어 이동/충돌·조사/접촉) 테스트 |
| `naru_engine_tests.exe` | 창 없는 엔진 테스트 |
| `naru_project_tests.exe` | 프로젝트 생성·저장·불러오기·오류 검증 테스트 |
| `naru_database_tests.exe` | 데이터베이스 템플릿·값 검사·형식 변환·검증·데이터 → 변수 명령 테스트 |
| `naru_event_tests.exe` | 이벤트 명령 실행(대사 대기·변수·조건 분기·스크립트 호출·오류 위치) 테스트 |
| `naru_python_tests.exe` | Python 임베딩·오류 위치·재로드 테스트 |

빌드 후 `windeployqt`가 Qt DLL/플러그인을, CMake가 `python3xx.dll`을 bin 폴더에 복사한다.

### 제작툴 사용법

1. `NaruStudio.exe` 실행 → 시작 화면에서 **새 프로젝트 만들기**(이름·저장 위치 입력) 또는 **프로젝트 열기**(`project.json`이 있는 폴더), 최근 프로젝트 더블클릭.
2. 왼쪽 **장면** 트리에서 장면/객체 선택, 가운데 캔버스에서 객체를 드래그해 배치, 마우스 휠로 확대/축소.
3. 오른쪽 **인스펙터**에서 이름·위치·크기·색(객체) 또는 이름·크기·배경색(장면) 편집.
4. 툴바: 새 장면, 객체 추가(Ctrl+N), 삭제(Del, 캔버스/트리에서), 실행 취소(Ctrl+Z)/다시 실행(Ctrl+Y), 저장(Ctrl+S).
5. **스크립트**: 목록 더블클릭으로 탭 열기, 새 스크립트, 문법 검사(F7), 실행(F5 — 스크립트 탭이면 그 파일, 장면 탭이면 `scripts/main.py`).
6. **이벤트**: 객체를 더블클릭(또는 선택 후 Enter)하면 이벤트 탭이 열린다.
   - 실행 조건: 조사 / 플레이어 접촉 / 자동 실행 / 이벤트 접촉 / 병렬 처리. 모두 게임 실행 화면에서 동작한다 (이벤트 접촉의 "객체가 움직여 닿을 때"는 객체 이동 기능 이후).
   - 명령 추가: 빈 `◆` 줄을 더블클릭하거나, 명령을 선택하고 "명령 추가"(선택한 명령 위에 들어감). 종류: 대사 표시, 대기, 변수 조작(= += -=), 조건 분기(그렇다면/아니라면, 중첩 가능), 스크립트 호출(scripts/의 파일·함수), 로그, 주석.
   - 명령 더블클릭: 편집. 삭제·위로·아래로 버튼. 모든 편집은 Ctrl+Z로 되돌린다.
   - 테스트 실행(F6): 실행 중인 명령이 노란색으로 표시되고, 대사는 아래 미리보기 창에서 "다음 ▶"으로 넘긴다. 중지(F8). 오류가 난 명령은 빨간색으로 선택된다. 변수는 매 테스트마다 비어 있는 상태로 시작한다.
7. **게임 실행(Ctrl+R)**: 저장한 뒤 현재 장면부터 NaruPlayer 창이 뜬다. 인스펙터에서 **플레이어**로 지정한 객체를 방향키로 움직이고 Z/Enter/Space로 조사한다. 게임 로그는 로그 패널에 `[게임]`으로 표시된다.
8. 파일 → 프로젝트 닫기: 시작 화면으로 돌아간다.

**Python 표준 라이브러리 위치:** 개발 빌드는 빌드할 때 찾은 Python 설치 폴더를 사용한다. 실행 시 `NARU_PYTHON_HOME` 환경변수로 바꿀 수 있다. 다른 PC로 실행 파일만 옮기는 배포는 아직 지원하지 않는다(M5).

명령행 옵션: `NaruStudio.exe [프로젝트폴더] [--last] [--new-project <dir>] [--event <objectId> [--run-event]] [--database <type>[/<record>] [--fields-tab]] [--smoke-test <ms>] [--screenshot <png>] [--self-test]`
설명서 촬영용: `--start-screen`, `--command-dialog <type>`, `--field-dialog` (모두 `--screenshot`과 함께). 자세한 사용법은 [사용 설명서](guide/README.md).
(`--smoke-test`·`--self-test` 실행은 최근 프로젝트 목록에 기록하지 않는다.)

## 5. 알려진 제한

- Debug 구성은 Python 디버그 라이브러리(`python3xx_d.lib`)가 필요해 기본 preset은 RelWithDebInfo를 사용한다.
- Python 스크립트는 격리되지 않은 사용자 로컬 코드로 실행된다. 무한 루프 중단 정책은 아직 없다.
- `async def run(ctx)`와 대기형 명령(대사·이동 등)은 아직 지원하지 않으며 명확한 오류로 보고한다.
