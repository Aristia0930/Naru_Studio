# Naru Studio

**C++ 자체 게임 엔진 + Qt 제작 UI + Python 게임 확장 스크립트**를 만드는 프로젝트입니다.

RPG Maker의 데이터베이스·맵·이벤트·전투 등 제작 기능을 출발점으로 삼고, Unity·Godot의 유용한 기능/작업 방식을 참고해 UI로 쉽게 제작할 수 있도록 합니다. 지원 목표는 턴제 RPG와 2D 횡스크롤입니다. 기존 엔진을 감싼 편집기가 목표가 아닙니다.

## 지금 시작할 곳

현재 기능을 Python으로 연결하려면 [Python 전체 가이드](docs/guide/PYTHON_COOKBOOK.md)를 보세요. 배경/객체/플레이어 이미지부터 변수·DB·입력·이동·장면·공통 이벤트·전역 훅·로그까지 [완성 코드](docs/examples/python_guide.py)와 [전역 연결 코드](docs/examples/guide_global.py)를 제공합니다.

DB를 중심으로 스킬·Python·이미지·음원·프레임 이펙트를 연결하고 투명화 도구를 추가하는 방법은 [DB·에셋·스킬 연결 계획](docs/DATABASE_ASSETS_PLAN.md)에 있습니다.

현재 작업 순서는 [로드맵](docs/ROADMAP.md), 실제 2D 게임 완성을 위한 추가 목록·구현 방법·검증 기준은 [상세 개발 계획](docs/DEVELOPMENT_PLAN.md)에 있습니다. 최신 결과는 [세션 기록](SESSION_STATE.md)을 따릅니다.

1. **[다음 세션 구축 안내](NARU_NEXT_STEPS.md)** — 확정한 목표, 완료/미완료, 설치 준비, 소스 구조, 단계별 구현과 검증 기준.
2. [세션 기록](SESSION_STATE.md), [작업 안내](AGENTS.md).
3. [기능·UI 설계 초안](TOOL_FOUNDATION.md). 최상단의 최신 C++/Qt/Python 갱신을 우선합니다.

## 현재 상태

**객체 이미지·장면 배경(10②)**: 인스펙터 팝업에서 이미지·배치·기준점·회전·뒤집기·불투명도·순서를 설정합니다. 충돌 영역은 독립이며 배경 다섯 표시 방식·사용처/제거 보호와 Python `obj.image`·`ctx.scene.background_image`를 지원합니다. [사용법·예제·수동 확인](docs/guide/IMAGES.md).

**에셋 목록(순서 10①)**: 독립 팝업에서 PNG/JPG/BMP·WAV·TTF/OTF를 관리합니다. **에셋 추가... / 파일 교체... / 파일 내보내기... / 목록에서 제거...**와 그룹 추가·이름 변경·삭제·여러 에셋 그룹 이동을 지원합니다. 썸네일·알파 미리보기·WAV 미리듣기·그룹/종류/검색 필터·Undo·저장/재열기도 제공합니다. [사용법·그림·수동 확인](docs/guide/ASSETS.md), [예제](docs/examples/assets/README.md).

추가할 때 파일별 **에셋 ID**를 직접 입력하고, 기존 항목은 **ID 변경...**으로 수정할 수 있습니다. 중복·형식을 검사하며 객체·배경 참조와 Undo를 함께 처리합니다. Python 코드에 직접 적은 ID 문자열은 직접 수정합니다. [ID 규칙·예제](docs/guide/ASSETS.md#id-직접-지정변경).

**실행 중 확인 도구(순서 9)**: 게임 실행 중 변수·스위치를 읽기 전용 패널에서 보고, 오류 로그를 더블클릭하면 해당 객체·공통 이벤트의 명령으로 이동합니다. 게임 로그는 실행마다 파일로 남깁니다. [사용법·예제·그림](docs/guide/DEBUGGING.md). 자동 테스트는 17종이며 이미지·ID 편집·Python 가이드 실행/문서 검사까지 포함합니다. 다음은 게임 소리 재생입니다.

**키 설정은 화면과 Python이 양방향 동기화됩니다.** 게임 → 키 설정에서 기본 조작 ON/OFF·속도·동작별 키를 바꾸거나 같은 전용 설정 파일을 편집합니다. **동시 키 조합 추가**로 Shift+E 같은 2~4키 입력도 연결합니다. [설정 안내](docs/guide/INPUT_SETTINGS.md), [이벤트 명령 전체](docs/guide/EVENTS.md), [스크립트 작성·연결](docs/guide/SCRIPTING.md)을 참고하세요. 새 기능·동작 변경마다 이 문서들도 함께 갱신합니다.

**프로젝트 단위 제작툴의 첫 버전이 동작합니다.** `NaruStudio.exe`를 실행하면 시작 화면에서 프로젝트를 새로 만들거나 열고, 그 프로젝트 안에서 장면 배치·속성 편집·Python 스크립트 편집/실행을 합니다. 자동 테스트 17종(엔진·이벤트·데이터베이스·게임·프로젝트·Python·에디터·실제 플레이어 검사)이 통과합니다. 객체마다 RPG Maker식 **이벤트**(대사·대기·변수·조건 분기·데이터 읽기·스크립트 호출)를 편집하고 바로 테스트 실행할 수 있습니다. **데이터베이스**는 데이터 종류와 속성을 직접 정의하는 방식입니다(캐릭터·아이템·몬스터는 고칠 수 있는 템플릿). **게임 실행(Ctrl+R)** 으로 SDL3 기반 `NaruPlayer`가 장면과 대사 창을 띄웁니다. 플레이어로 지정한 객체를 방향키로 움직이고 결정 키로 조사하며, 실행 조건(조사·접촉·자동 실행·병렬 처리)이 동작합니다. **선택지 표시**로 질문·최대 6개 항목을 만들고 게임 화면·F6 미리보기에서 고른 분기를 실행합니다. **공통 이벤트** 탭에서 여러 장면이 함께 쓸 명령을 편집하고 이벤트 명령 또는 Python `ctx.run_event`로 호출합니다. 이미지·WAV·글꼴 [가져오기·목록·미리보기](docs/guide/ASSETS.md)를 지원합니다. 객체 이미지·장면 배경을 표시하며 게임 소리·배포는 아직 없습니다. **전역 스크립트**(여러 개 등록·순서, `on_game_start`/`on_scene_enter`/`on_update` 훅, 키 배치·기본 조작 변경)로 게임 전체 동작을 Python으로 덧붙입니다. 단축키: [docs/SHORTCUTS.md](docs/SHORTCUTS.md) · 스크립트에서 쓰는 엔진 값: [docs/SCRIPT_API.md](docs/SCRIPT_API.md) · 스크립트 예제: [docs/examples](docs/examples/README.md). **사용 설명서(그림 포함): [docs/guide](docs/guide/README.md)** · 빌드: [docs/BUILD.md](docs/BUILD.md) · **다음 할 일: [docs/ROADMAP.md](docs/ROADMAP.md)**.

| 위치 | 내용 |
|---|---|
| `engine/` | Qt 비의존 엔진: 로그, 변수, 이벤트 명령 실행기, 고정 틱 Session, **프로젝트 모델·JSON 저장/불러오기·검증** |
| `scripting/python/` | CPython 임베딩: `run(ctx)` 호출, `ctx.variables`/`ctx.log`/`ctx.db`/`ctx.input`/`ctx.player`/`ctx.scene`, print→로그, 오류 파일·행·traceback, 전역 스크립트 모듈(`PythonGameScripts`) |
| `player/` | SDL3 게임 실행 화면 `NaruPlayer.exe`: 장면 그리기, 대사 창, 입력 → 엔진 `GameWorld` |
| `editor/` | Qt Widgets 제작툴: 시작 화면(새로 만들기/열기/최근), 장면 트리, 2D 캔버스(드래그 배치·휠 확대), 인스펙터, Undo/Redo, 스크립트 탭, 로그 |
| `tests/` | 엔진·프로젝트·Python 테스트와 테스트용 스크립트 |
| `third_party/` | nlohmann/json 3.11.3 (MIT) |

프로젝트 폴더 구조: `project.json`(이름·형식 버전·시작 장면·장면 순서), `scenes/<id>.json`, `scripts/*.py`, `assets/`.
| `analysis/rpgmaker/` | RPG Maker 과거 분석 01~07, MV 코어 4파일 REA 재조사 08, 요약/원시 Evidence |
| `analysis/godot/` | Godot 소스 관측·실행 검증·REA export·Ghidra 콘솔 래퍼 분석 |
| `analysis/unity/` | 설치 확인·문서 비교, REA 메타데이터/7타입 관측과 인스펙터·직렬화·Undo·프리팹 공개 소스 조사 |
| `analysis/rea-connection/` | REA 설치·MCP 연결·Ghidra 검증 근거 |
| `analysis/foundation/` | 파일 해시·행 번호를 포함한 소스 관측 |
| `refs/` | MV 공개 코어·mkxp·Godot 핵심 소스 일부와 각각의 라이선스 |
| `probes/`, `tools/` | 조사와 검증에 사용한 코드 |
| `app/` | **최종 제품이 아닌 Godot/GDScript 실험본**과 통합 검증 코드 |

세 제품 전체를 전수 분석한 상태는 아닙니다. 문서/소스 조사와 제한된 도구·실행 검증의 범위는 각 조사 README를 참고하세요.

## 다른 컴퓨터에서 이어가기

```powershell
git clone https://github.com/Aristia0930/Naru_Studio.git
cd Naru_Studio
```

이후 [docs/BUILD.md](docs/BUILD.md)대로 VS 2022 Build Tools·CMake·Ninja·Qt 6.8.3·Python 3.13+·SDL3/SDL3_ttf를 준비합니다. 그다음 **`dev.cmd`를 더블클릭하면 빌드 후 바로 실행**되고, `dev check`는 테스트까지 통과해야 실행합니다. 개발 도구와 SDK를 Git에 묶어 올리는 대신 설치 방법/버전을 기록합니다.

Godot·Ghidra·REA는 이전 조사 도구이며 새 엔진의 필수 실행 환경이 아닙니다. 참고 자료를 조사할 때만 별도로 준비합니다. PC별 경로·캐시·자격증명을 공유하지 않습니다.

## 이전 실험본을 실행하려면

`app/`은 39개 통합 검증을 통과한 UI/행동 참고용 Godot 프로젝트입니다. 장면·이벤트·코드 연결·즉시 실행·로그·저장·음원/이동 연출을 실험했습니다. 제품 구현을 여기에 이어 붙이지 않습니다.

Godot 4.7.2 standard Windows x64를 [공식 배포처](https://github.com/godotengine/godot-builds/releases/tag/4.7.2-stable)에서 받아 `refs/godot-4.7.2/bin/`에 압축을 풀면 기존 `Start Studio.cmd`로 실행할 수 있습니다. 런처 이름과 화면의 과거 Studio 표기는 실험본에 남아 있습니다. 엔진 바이너리·ZIP·캐시·테스트 산출물은 Git에서 제외합니다.

실험본 검증: Godot 콘솔 실행 파일에 `--headless --path app --script tests/integration.gd --log-file <절대경로>`를 전달합니다. 정확한 로컬 경로를 바꾸어 사용하세요. 실험본의 `.gd` 확장은 최종 엔진의 Python 스크립트 API가 아닙니다.

## 참고 소스

외부 참고 소스의 LICENSE/COPYING과 provenance를 유지합니다. 일부 `refs`는 다른 라이선스의 조사용 소스이므로 제품 코드로 복사하지 않습니다. 원시 Evidence의 절대 경로는 조사 당시 위치의 기록이며, 그 경로가 새 PC에 존재해야 한다는 뜻이 아닙니다.
