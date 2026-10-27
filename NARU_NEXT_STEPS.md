# Naru Studio — 다음 세션 구축 안내

> **2026-10-27 갱신: 현재 진행 상황과 다음 할 일은 [docs/ROADMAP.md](docs/ROADMAP.md)를 먼저 본다.** 아래는 초기 구축 계획(M0~M5)이며 M0와 M1·M3 일부가 끝났다.

최신 기준: 2026-10-27. 사용자 요청으로 오늘의 구축을 종료하고 이 문서를 다음 시작점으로 남긴다.

## 1. 확정한 목표와 기술

**Godot 등을 감싼 편집기가 아니라, 자체 게임 엔진과 UI 기반 게임 제작툴을 만든다.**

사용자 확정: **엔진 핵심 C++ + 제작 UI Qt + 게임 확장 스크립트 Python**.

- 엔진 핵심: 장면·객체·게임 루프·시간·입력·이벤트·상태·애니메이션·오디오 제어·저장/불러오기·실행/중지를 Naru 코드로 구현한다.
- 제작 UI: Qt로 목록·트리·인스펙터·장면 배치·이벤트 편집·코드 편집·로그·미리보기 화면을 만든다. Qt는 UI 라이브러리로 사용한다.
- 게임 스크립트: RPG Maker의 스크립트/플러그인처럼, 게임 프로젝트 안의 Python 파일을 게임 기능·이벤트에 연결한다. 제작툴 자체 언어와 게임 확장 언어를 구분한다.
- RPG Maker는 기본 제작 기능과 사용자 흐름의 기준이다. Unity·Godot은 유용한 기능과 구조의 조사 대상이다. 이들 엔진을 제품의 실행 기반으로 사용하는 결정은 철회했다.
- 지원 목표는 턴제 RPG와 2D 횡스크롤이다. RPG Maker에 없는 기능도 UI로 쓰기 좋으면 추가 후보로 검토한다.
- Node.js·REA·Ghidra·Java·Godot은 조사에 사용한 도구이며 새 엔진의 필수 실행 의존성으로 정하지 않았다.

이전 Java/JavaFX 추천, GDScript 기반 최종 제품 제안, 웹/Capacitor 전제보다 이 확정 사항을 우선한다.

## 2. 폴더와 저장소 역할

- 게임 기획·대본·에셋: `C:\Users\kjk98\OneDrive\바탕 화면\game`
- 제작툴 개발 기준: `C:\Users\kjk98\OneDrive\바탕 화면\game_tool`
- 제작툴 저장소: `https://github.com/Aristia0930/Naru_Studio.git`
- 다른 PC에서는 저장소를 원하는 위치로 clone해서 개발한다. 위 Windows 절대 경로를 새 코드에 하드코딩하지 않는다.
- 기존 game/engine-tool 자료는 원본 보존용이다. 새 기능 구현과 최신 기록은 game_tool에서만 이어간다.

## 3. 실제 완료 상태

### 조사

| 대상 | 조사와 검증 | 한계 |
|---|---|---|
| RPG Maker | `analysis/rpgmaker/01~07` 과거 문서, `08_REA_재조사.md`, 공개 MV 코어 4파일 REA 정적 분석과 원시 근거 | MZ 에디터 바이너리/동작을 직접 검증한 것은 아님. MV 선택 외 183개 인벤토리 파일과 동적 관계는 미검증 |
| Godot | 4.7.2 공식 배포본·해시 확인, 동일 커밋 C++ 일부 소스, headless 기초 검증 10개, 본체 export·콘솔 래퍼 3함수 Ghidra 검증 | 전체 엔진 바이너리 전수 분석 아님. 일부 소스 미수집 404도 manifest에 기록 |
| Unity | 설치 6000.6.5f1 확인, 공식 문서 비교, 후속 REA 아티팩트/7타입 관측·고정 커밋 공개 C# 소스 조사 (`analysis/unity/02_REA_및_공개소스_조사.md`) | members 상세는 64MiB 한도로 partial. 공개 소스 7000.0.0a12와 설치본의 동일 빌드 매핑은 미확인. 전체 소스/네이티브 동작·빌드 미검증 |

**세 제품의 전수 조사가 끝난 상태로 설명하지 않는다.** 문서 조사·소스 관측·도구 Evidence·실행 관측·우리 설계 제안을 분리한다.

### Godot 실험본 — 최종 제품이 아님

`app/`에는 자체 엔진 목표를 명확히 하기 전에 만든 Godot/GDScript 실험본이 있다. 폐기하지 않고 행동 규격·UI 흐름·테스트 참고로 보존한다.

- 장면/오브젝트 목록, 배치·속성 편집, 캐릭터 데이터, 이벤트 편집, 코드 편집/문법 검사.
- 대사·대기·이동 애니메이션·음원 재생/정지·변수·코드 호출의 8가지 명령.
- JSON 저장/재로드·백업, 이미지/음원 가져오기, undo/redo.
- F5 장면 실행, F6 선택 명령부터 실행, F8 중지. 실행 상태와 편집 원본 분리.
- 로그 패널·JSONL 파일·장면/이벤트 위치·오류에서 해당 명령 선택.
- 통합 검증 **39개 통과**, 실제 창 스크린샷 확인. 별도로 내보낸 런타임 프로젝트 실행 성공.
- 파일: `app/tests/integration.gd`, `app/tests/visual_smoke.gd`, `app/test-output/latest.json`, `app/test-output/studio-overview.png`(생성 결과는 Git 제외 가능).
- `.gd` 파일 가져오기는 프로젝트 JSON 안 코드의 사본을 실행하는 방식이다. 외부 Python 파일 연결/자동 재로드는 미구현이다.
- 음원 테스트는 로딩·채널·중지 처리 검증이며 청취 품질 검증은 아니다.
- export 코드에는 개발용 Godot 본체 복사 경로가 있지만, 통합 검증은 엔진 미포함 프로젝트 export를 사용했다. 정식 독립 실행 파일 배포가 검증됐다고 기록하지 않는다.
- 이 실험본을 확장하여 최종 제품으로 삼지 않는다. Qt/C++에서 동작을 다시 구현한다.

### 자체 엔진

**2026-10-27 갱신: M0 완료.** 소스 골격·CMake 빌드·테스트가 동작한다. 상세는 `SESSION_STATE.md` 최상단과 `docs/BUILD.md`. 아래 4절의 1~4번은 완료했고 다음 시작점은 7절 M1의 나머지(데이터 모델·JSON 저장·실시간 루프·F8 중지)와 M2의 대기형 스크립트다.

## 4. 다음에 가장 먼저 할 일

1. 이 문서와 `AGENTS.md`, `README.md`, `SESSION_STATE.md`를 읽고 game_tool에서 작업한다.
2. 개발 PC의 실제 C++ 빌드 환경을 확인한다. PATH에서 cmake/ninja/cl/g++/Qt를 찾지 못했지만 PC 전체 미설치로 단정하지 않는다. Visual Studio 설치·Qt kit·CMake·Python 개발 헤더/라이브러리를 확인한다.
3. 설치할 버전과 컴파일러 조합을 문서로 고정한 다음 필요한 것만 설치한다. CMakePresets와 환경별 로컬 설정으로 경로를 관리한다.
4. 최소 CMake 프로젝트를 만들고 **화면 없는 엔진 테스트 → Qt 빈 제작툴 창 → Python 스크립트 호출**을 각각 빌드/실행한다.
5. 처음부터 Godot UI를 다시 고치거나 REA 설치를 반복하지 않는다. 아래 단계별 기능을 자체 엔진에 연결한다.

## 5. 권장 설치·빌드 구성 — 세부 버전은 다음에 검증

- Windows C++ 컴파일러: Visual Studio 2022 Build Tools의 C++ 데스크톱 개발 도구와 Windows SDK를 우선 후보로 확인한다.
- Qt 6: 선택한 MSVC와 맞는 **동일한 x64 kit**. Qt Widgets를 첫 데이터/이벤트 편집 UI 후보로 둔다. QML/Qt Quick 채택은 별도 판단한다.
- CMake와 Ninja(또는 선택한 빌드 생성기). C++20을 우선 표준 후보로 잡는다.
- Python: CPython 개발 헤더·라이브러리까지 포함한 배포본. 기존 Python은 3.8이 확인됐으므로 새 임베딩용 버전을 검증/고정한다. 기존 Python을 임의로 교체하지 않는다.
- Python 바인딩: CPython C API 직접 사용 또는 pybind11. 먼저 작은 임베딩 실험을 하고 한 방식으로 고정한다.
- 렌더링/입력/오디오 하위 기반: SDL3 등은 후보이며 **아직 확정/설치하지 않았다**. 엔진 상태·씬·이벤트 로직과 백엔드 API를 분리한다.
- Git. Qt Creator/Visual Studio/VS Code 중 편한 IDE는 선택 사항으로 안내한다.

공식 시작점: [Qt+CMake](https://doc.qt.io/qt-6/cmake-get-started.html), [CPython 임베딩](https://docs.python.org/3/extending/embedding.html), [SDL3 후보 자료](https://wiki.libsdl.org/SDL3/FrontPage). 이 링크의 latest/stable 버전은 바뀌므로 실제 채택 시 버전을 고정한다.

## 6. 새 소스 구조 제안

```text
Naru_Studio/
  CMakeLists.txt, CMakePresets.json    # 다음 구현 때 생성
  engine/                           # Qt에 의존하지 않는 게임 로직
    core/                           # ID·시간·입력·상태·로그
    scene/                          # 씬·객체·변환·리소스 참조
    events/                         # 명령 등록·실행·대기·취소
    animation/                      # 속성 트랙·보간·완료 이벤트
    battle/                         # 턴제 기능
    platformer/                     # 이동·충돌·카메라 기능
  editor/                           # Qt 제작 UI
  player/                           # 자체 엔진을 실행하는 게임 플레이어
  scripting/python/                 # 임베딩·게임 API·오류 처리
  examples/                         # 별도 라이선스 문제 없는 연구용 프로젝트
  tests/                            # 엔진·저장·스크립트·실행/중지 테스트
  docs/                             # 설치·빌드·API·개발 가이드
  analysis/, refs/, probes/, tools/  # 이관한 조사 자료
  app/                              # 이전 Godot 실험본; 제품 기반 아님
```

이 트리에서 C++ 관련 항목은 **만들 예정인 구조**이며 이미 있는 구현으로 보고하지 않는다.

## 7. 구축 순서와 통과 기준

### M0 — 재현 가능한 개발 환경

같은 저장소를 다른 PC에 clone한 뒤 문서대로 configure→build→test할 수 있어야 한다. 사용 버전·kit·x64·설치 링크·실행 명령을 README에 기록한다. 비밀값·개인 절대 경로·설치 바이너리·캐시는 Git에 넣지 않는다.

### M1 — 자체 엔진의 작은 실행 단위

- 안정적인 ID, Project/Scene/Object/Asset/Character 데이터, 형식 버전, JSON 왕복 저장과 참조 검증.
- 엔진 Session과 편집 Document를 분리하고, 실행에는 데이터 복사본을 사용한다.
- 고정 간격 게임 업데이트와 표시 업데이트, 시작·중지·재시작·오류 상태.
- 명령 등록과 `running/waiting/completed/failed/cancelled` 상태, 대사·대기·변수·대상 표시·속성 이동.
- 창 없는 테스트에서 저장/재로드, 같은 입력의 실행 순서, 대기 중 취소, 실패 시 다음 명령 정지, 원본 불변성을 검증한다.

### M2 — Python 게임 스크립트

- 프로젝트 `scripts/*.py`를 이벤트의 코드 명령/기능 확장에 연결한다. 이것이 사용자가 말한 RPG Maker의 스크립트 개념이다.
- 엔진이 제공할 GameContext API: 변수 읽기/쓰기, 엔티티 조회, 대사 요청, 이동/애니메이션 요청, 음원 요청, 로그, 오류 보고.
- 스크립트 실행 전 문법 검사, 오류의 파일·행·traceback·장면·명령 ID를 기록한다.
- 편집 후 재실행 시 최신 코드를 반영한다. 문서에 저장된 사본과 외부 파일의 충돌/재로드 정책을 정한다.
- 대사/대기/애니메이션은 프레임 루프를 막지 않는 awaitable/명령 큐 정책으로 설계한다. 중지 시 연결과 예약 작업을 해제한다.
- Python은 사용자 로컬 코드로 실행되며 자동으로 격리되는 것은 아니다. 실행 중 무한 루프·외부 작업 취소 정책은 별도 설계한다.

원하는 API 형태의 **설계 예시**(현재 미구현):

```python
async def run(ctx):
    count = ctx.variables.get("visits", 0) + 1
    ctx.variables.set("visits", count)
    ctx.log(f"방문 횟수: {count}")
    await ctx.say("안내", "코드와 이벤트가 연결되었습니다.")
    await ctx.move("sample_actor", x=500, y=180, seconds=0.5)
```

### M3 — Qt 제작툴과 즉시 실행

장면/오브젝트 목록 + 2D 배치 + 인스펙터 + 이벤트 목록 + 캐릭터 데이터 + Python 코드 편집 + 로그 패널을 연결한다. Ctrl+S·Undo/Redo·F5 현재 장면·선택 명령부터 실행·중지→편집 복귀를 제공한다. 미리보기와 최종 플레이어는 같은 engine 라이브러리를 사용한다.

### M4 — 음악·애니메이션과 장르 최소 검증

이미지·음원 가져오기, 채널/음량/반복/페이드, 속성 애니메이션·동시 연출·완료 대기·취소를 연결한다. 수평 이동·충돌·카메라와 작은 턴제 테스트를 각각 검증한다. 점프·중력·액션 전투·턴 순서 등 실제 게임 규칙을 임의 확정하지 않는다.

### M5 — 배포 연결

먼저 Windows의 자체 player + 데이터 + 에셋 + 필요한 DLL/Python 런타임을 묶어 다른 PC에서 검사한다. Android는 초기에 작은 빌드를 점검하되 지원 완료로 앞서 표기하지 않는다. Godot exe를 복사하는 기존 실험본 export를 새 엔진 배포로 쓰지 않는다.

## 8. 다음 세션에서 반복하지 않을 것

- REA/Node/Java/Ghidra 설치와 연결 검증을 다시 시작하지 않는다.
- Godot 기반 초안이 최종 엔진이라는 전제로 개발하지 않는다.
- Java/JavaFX 또는 Python 단독 UI 선택으로 되돌아가지 않는다. C++/Qt/Python은 사용자 확정이다.
- 첫 게임의 대본·CG·음원·Unity 프로젝트 작업을 제작툴 구축과 섞지 않는다.
- 근거가 없는 전체 전수 분석 완료·C++ 엔진 완성·Android 지원 완료를 주장하지 않는다.

다음 첫 작업은 **개발 환경/버전 확인과 CMake 최소 엔진·Qt 창·Python 임베딩의 빌드 연결**이다.
