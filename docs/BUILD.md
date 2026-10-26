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

Visual Studio IDE, Qt Creator, VS Code는 선택 사항이다. 설치한 도구·SDK는 Git에 넣지 않는다.

## 2. Qt 설치

공식 Qt Online Installer(계정 필요)로 Qt 6.8.3 → MSVC 2022 64-bit를 설치하거나, 계정 없이 aqtinstall을 사용한다.

```powershell
py -3 -m venv $env:LOCALAPPDATA\naru-tools\aqt-venv
& $env:LOCALAPPDATA\naru-tools\aqt-venv\Scripts\python.exe -m pip install aqtinstall pip-system-certs
& $env:LOCALAPPDATA\naru-tools\aqt-venv\Scripts\aqt.exe install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:\Qt
```

`pip-system-certs`는 Python이 Windows 인증서 저장소를 쓰게 한다. 보안 프로그램/프록시가 HTTPS를 검사하는 PC에서 aqt가 `SSLError`로 실패할 때 필요했다.

## 3. 빌드

가장 간단한 방법 (VS 개발자 환경을 자동으로 불러온다):

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
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

PC별 경로를 고정하고 싶으면 `CMakeUserPresets.json`(Git 제외)에 `CMAKE_PREFIX_PATH`, `Python3_ROOT_DIR`을 넣은 preset을 만든다.

## 4. 결과물

`build\windows-msvc\bin\`

| 파일 | 내용 |
|---|---|
| `NaruStudio.exe` | Qt 제작툴. Python 스크립트 편집·문법 검사(F7)·실행(F5), 로그(더블클릭 시 오류 행 이동, 툴팁에 traceback), 실행 결과 변수 |
| `naru_engine_tests.exe` | 창 없는 엔진 테스트 |
| `naru_python_tests.exe` | Python 임베딩·오류 위치·재로드 테스트 |

빌드 후 `windeployqt`가 Qt DLL/플러그인을, CMake가 `python3xx.dll`을 bin 폴더에 복사한다.

**Python 표준 라이브러리 위치:** 개발 빌드는 빌드할 때 찾은 Python 설치 폴더를 사용한다. 실행 시 `NARU_PYTHON_HOME` 환경변수로 바꿀 수 있다. 다른 PC로 실행 파일만 옮기는 배포는 아직 지원하지 않는다(M5).

명령행 옵션: `NaruStudio.exe [script.py] [--run] [--smoke-test <ms>] [--screenshot <png>]`

## 5. 알려진 제한

- Debug 구성은 Python 디버그 라이브러리(`python3xx_d.lib`)가 필요해 기본 preset은 RelWithDebInfo를 사용한다.
- Python 스크립트는 격리되지 않은 사용자 로컬 코드로 실행된다. 무한 루프 중단 정책은 아직 없다.
- `async def run(ctx)`와 대기형 명령(대사·이동 등)은 아직 지원하지 않으며 명확한 오류로 보고한다.
