# 개발툴 전용 세션 기억 — 2026-10-26

이 파일은 게임 기획/에셋 기록과 별개인 제작툴 전용 기록이다. 개발툴 재개 시 NARU_NEXT_STEPS.md와 이 파일을 먼저 읽고 실제 환경을 확인한다. 최신 사용자 지시가 우선한다.

## 선택지 표시 완료·다음 시작점 — 2026-11-01 (작업 순서 6까지)

- **다음 기능: ROADMAP 순서 7 공통 이벤트.** 순서 0~6은 구현·자동 검증 완료. 이벤트 조건·반복(순서 5)은 사용자 수동 테스트 6항목, 선택지(순서 6)는 수동 테스트 4항목 모두 통과했다고 보고받았다. 사용자 요청으로 여기까지의 구현·검증 결과를 기록하고 커밋·푸시한다.
- **선택지 표시:** 질문·이름(생략 가능), 1~6개 항목, 처음 선택(기본 1번째), 취소 대상(기본 취소 불가). 선택한 항목 아래 명령만 실행하고 다음 공통 명령으로 복귀한다. 선택지·조건 분기를 서로 중첩할 수 있다. JSON `show_choices` / `choices: [{text, commands}]` / `defaultChoice`(0부터) / `cancelChoice`(-1=취소 불가), 기존 이벤트와 같은 깊이 제한 32. 오류 명령 위치는 `0/c1/0`처럼 선택 분기를 포함한다.
- **엔진·게임:** DialogueHost의 요청별 선택 결과를 GameWorld·F6가 공유한다. 다른 대사·선택지가 열려 있으면 선택지는 기다린다. 선택지 위에 다른 이벤트 대사를 덮어쓰지 않는다. 게임의 up/down·confirm·cancel 동작으로 선택한다. 선택지 조작에 쓴 pressed 입력은 스크립트에 전달하지 않고, 결정한 프레임에도 기본 플레이어 조작을 막는다. F8·장면 이동은 자기 선택 요청만 취소한다.
- **편집기:** 선택지 입력 창에서 항목 글자·추가·삭제·순서 편집, 처음 선택·취소 대상 지정. 순서 변경은 분기 명령·처음 선택·취소 대상을 함께 옮긴다. 명령 있는 항목 삭제는 확인한다. 이벤트 트리의 항목 아래에 명령을 넣으며 Undo·저장·재로드를 지원한다. F6는 선택 목록·선택/취소 버튼·Enter/더블클릭·X/Esc를 지원한다.
- **검증:** ctest **10/10 통과**(8.97초). events 새 2건(선택 대기·분기 복귀·중첩 오류 위치·중지·호스트 없음), game 새 3건(기본 선택·순환·취소·입력 소비, 병렬 요청·대사 대기·장면 전환, 결정 프레임 이동 방지), project 새 2건(왕복, 잘못된 글자·개수·번호·형식·과도한 중첩 거부), editor_selftest(항목 순서·대상 보존·Undo·F6 실제 선택/취소 버튼·F8·저장), **player_choices**(실제 SDL 플레이어: 기본 결정·중첩·취소·빈 분기 복귀 4경로). 자동 검사는 `tests/`와 `MainWindow::runSelfTest`에 저장한다.
- **그림·예제:** 사용 설명서·SCRIPT_API·단축키 갱신. 실제 촬영한 그림 19(입력 창)·20(F6)·21(게임)을 확인했다. `docs/examples/choices`는 복사해서 바로 여는 예제 프로젝트다. 촬영 명령은 예제 README와 사용 설명서 부록에 있다.
- **빌드 문제 수정:** 이전 최초 configure가 한글 MSVC의 `/showIncludes` 접두사를 잘못 저장해 헤더를 바꿔도 일부 타깃이 재컴파일되지 않았다. fresh configure + 전체 재빌드로 기존 database/Python/player 실패가 해소됐다. `tools/build.ps1`도 UTF-8을 설정하도록 수정했으며, 이후 전체 검사와 Ninja deps에서 EventData.h 추적을 확인했다. BUILD.md에 복구 절차를 남겼다.
- **사용자 수동 검증 완료 (2026-11-01):** 사용자가 안내한 4항목 모두 통과했다고 보고했다. (1) F6 선택·취소·F8 중지, (2) 게임의 쉬기/이야기 듣기/나가기·중첩·취소 분기, (3) 항목 글자·추가·순서 변경 시 명령 동반 이동·Undo·저장·재열기, (4) 취소 불가 설정 시 F6 취소 버튼 비활성 및 게임 X/Esc로 닫히지 않음. 마지막 항목은 맨 위 선택지 명령의 설정 절차를 추가 안내한 뒤 통과 보고를 받았다.
- **미구현/한계:** 현재 게임 선택지는 키보드 조작이며 마우스 선택은 지원하지 않는다. 숫자/이름 입력·공통 이벤트·대기형 Python 스크립트는 아직 없다. 다음 시작점은 ROADMAP 순서 7 공통 이벤트다.

## 추가 PC 빌드 확인 — 2026-10-31

- 사용자 요청: pull 후 Qt kit 누락으로 실행하지 못한 PC에서 개발 환경을 준비하고 전체 빌드·테스트를 수행했다. 기존 Python 설치는 보존했다.
- 확인한 조합: Visual Studio Community 2026 18.10.3 / MSVC 19.51.36260.0(기존 설치), Windows 11 SDK 10.0.26100.0(추가), Qt 6.8.3 MSVC 2022 x64(추가), Python 3.14.3 x64(별도 설치), CMake 4.4.4, Ninja 1.13.2, aqtinstall 3.3.0, SDL3 3.4.18 / SDL3_ttf 3.2.2. SDL 압축 파일 SHA-256은 BUILD.md 기록과 일치했다.
- Qt는 기본 미러에서 핵심 파일 다운로드가 진행되지 않아 해당 설치 프로세스를 중단하고 다른 공식 미러에서 qtbase·qttools·qtsvg를 설치했다. 첫 configure는 Windows SDK 누락(rc/mt 없음)으로 실패했고 SDK 추가 후 성공했다. 해결 안내는 `docs/BUILD.md`에 추가했다.
- Python·설치 도구·SDL은 사용자 전용 naru-tools 폴더에 두고, Qt는 기본 탐색 위치에 설치했다. 사용자 `NARU_PYTHON_DIR`과 빌드 도구 PATH를 등록했다. 환경변수를 등록하기 전에 열어 둔 터미널은 새로 열어야 한다. 설치 파일·바이너리·로컬 로그는 Git에 포함하지 않는다.
- **검증 완료:** windows-msvc configure·전체 build 성공, ctest **9/9 통과**(editor_smoke·editor_selftest 포함, 9.66초). 결과물 `build/windows-msvc/bin/NaruStudio.exe`, `NaruPlayer.exe`. 기능 소스는 변경하지 않았다.
- **사용자 수동 검증 완료 (2026-10-31):** 아래 이벤트 조건·반복 6항목을 사용자가 직접 테스트하고 모두 통과했다고 보고했다. 조건 편집·Undo·저장/재열기, 조건부 조사, 한 번만 자동 실행, 반복 병렬 처리, 반복 자동 실행 경고, F6 조건 무시 경고·실행을 포함한다. 이후 구현 진행은 이 파일 최상단을 따른다.

## 이전 시작점 — 2026-10-30 (작업 순서 5까지)

- **다음 할 일: [docs/ROADMAP.md](docs/ROADMAP.md)의 "2-0. 작업 순서" 표를 위에서부터 하나씩** (사용자: "순서대로 해보자"). 0~5 완료 → **6 선택지** → 7 공통 이벤트 → 8 키 설정 화면 → 9 실행 중 확인 도구 → 10 에셋 → 11 맵 → 12 캐릭터 애니메이션 → 13 스킬 이펙트 → … 19 내보내기 (설계는 ROADMAP 3장).
- **사용자 확정 대기** (ROADMAP 4장에 제안으로 기록): SDL3_image 3.4.8·SDL3_mixer 3.2.4 추가(순서 10에서 설치), 타일 크기 = 타일셋 설정값·기본 48px, 캐릭터 그림 = 일반 "그림 소스(격자 시트·여러 장·아틀라스) + 클립" 방식. 사용자가 답하기 전 순서 10 이전 작업에는 영향 없음.
- 사용자 결정: **RPG Maker 소재 형식 호환은 필요 없음**(기능·흐름만 참고). **그림은 도트가 아닐 수도 있음**(일러스트·고해상도 지원). 캐릭터는 동작 여러 개(공격·시전 등)를 두고 스킬/공격 때 잠깐 불러오는 방식(설계 C: 평소 동작 자동 + 한 번 재생 후 복귀 + 프레임 신호 + DB 스킬의 사용자 동작·이펙트).
- 사용자는 Python 문법에 익숙하지 않다 — 예제를 보여 줄 때 `ctx`·점(.)·대입 등을 풀어서 설명한다. `docs/SCRIPT_API.md`는 스크립트 공개 이름이 바뀔 때마다 갱신(테스트가 누락을 잡음). 자동으로 불리는 함수는 정해진 훅 4개 이름뿐이라는 점도 자주 헷갈려 함.
- 재개 시: ROADMAP → 이 파일 → `docs/guide/README.md` 순서로 읽고, `dev test`로 상태 확인. 단축키 `docs/SHORTCUTS.md`, 스크립트 값 `docs/SCRIPT_API.md`, 예제 `docs/examples/`.
- Naru Studio가 열려 있으면 에디터 링크가 실패(LNK1168)한다 — 강제로 닫지 말고 사용자에게 닫아 달라고 한 뒤, 그동안은 에디터 외 타깃만 빌드·테스트한다.
- **푸시는 GitHub 계정 Aristia0930으로만** (사용자, 2026-10-28). 이 PC의 전역 git 사용자는 회사 계정이라, 저장소 로컬 설정으로 커밋 작성자를 `Aristia0930 <kjk9858@gmail.com>`으로 지정함. 2026-10-27 커밋들은 회사 이메일로 남아 있음(기록 재작성 안 함).

## 이벤트 실행 조건·반복 (작업 순서 5) — 2026-10-30

- 데이터: `EventCondition{variable, compare, value}`, `ObjectEvent::conditions`(모두 맞아야 실행)·`repeat`. 장면 JSON `event.conditions: [{variable, compare, value}]`(빈 이름·모르는 비교·값 없음/null 거부), `repeat: true`(켤 때만 저장). 비교는 `conditionsMet()`(EventCompiler) — 조건 분기와 같은 규칙(없는 변수는 숫자와 비교 시 0), 비교 불가(글자 크기 vs 숫자)는 거짓.
- 실행(`GameWorld`): 조건이 안 맞는 조사·접촉·이벤트 접촉은 없는 것처럼(조사는 다음 후보). 자동 실행·병렬 처리는 `startAutoEvents()`를 장면 진입(on_scene_enter **뒤**로 옮김) + 매 틱 on_update 직후에 호출. `autoStarted_`(장면마다 초기화): 반복 끔 = 조건이 처음 맞을 때 한 번, 반복 켬 = 실행 중/대기 중이 아니면 다시(끝난 다음 틱부터 → 같은 틱 무한 반복 없음). 실행 중 조건이 바뀌어도 그 회차는 끝까지.
- 에디터: 이벤트 편집 화면에 "조건: …" 요약 + **조건...**(`EventConditionsDialog`, 조건 하나는 `CommandDialog`의 조건 분기 폼 재사용, 값 없음 거부) + "끝나면 다시 실행" 체크(자동 실행·병렬 처리에서만). 조건 없는 반복 자동 실행이면 빨간 경고. `EventEditor::setConditions(conditions, repeat)`(Undo). 실행 조건 콤보 문구 수정("병렬 처리 (… 동시에 반복)"은 사실과 달라 "반복" 삭제). F6은 조건 무시, 처음 값으로 안 맞으면 로그 경고.
- **검증:** ctest 9/9 — game 새 4건(조건 안 맞는 조사→밟은 객체 조사·접촉 안 됨, 반복 끔 자동 실행은 스위치 켜질 때 한 번·장면 재진입 시 다시, 반복 켬은 한 틱 한 번·조건 끝나면 조작 복귀, 병렬 반복/한 번/조건부), project(조건·반복 왕복, 잘못된 조건 4종 거부), editor_selftest(조건 지정·Undo·F6 경고 후 실행). 그림 18 추가, 03·04·06 재촬영.
- **사용자 직접 확인 항목 (2026-10-31 사용자 보고: 6항목 모두 통과)** — 새 프로젝트의 상자(obj_002)로:
  1. 조건 창: 상자 → 이벤트 탭 → 조건... → 조건 추가 → `box_opened >= 1` → "조건:" 줄 표시, Ctrl+Z, 저장 후 다시 열어도 유지.
  2. 조사 조건: 변수·스위치에 스위치 `chest_unlocked`(거짓) 추가, 상자 조건 `chest_unlocked == 참` → Ctrl+R에서 Z 눌러도 반응 없음 → 처음 값 참으로 바꾸면 대사 나옴.
  3. 한 번만 자동 실행: 새 객체 자동 실행·조건 `box_opened >= 1`·반복 끔·대사 "상자를 열었구나!" → 처음엔 안 나오고 상자를 열면 바로 나옴, 다시 열어도 안 나옴.
  4. 반복 병렬 처리: 새 객체 병렬 처리·반복 켬·숨기기(이 객체) → 대기 0.5 → 보이기 → 대기 0.5 → 계속 깜빡이고 주인공은 자유롭게 움직임.
  5. 경고: 자동 실행 + 반복 켬 + 조건 없음 → 빨간 경고, 조건 넣으면 사라짐.
  6. F6: 조건이 안 맞는 이벤트에서 F6 → 로그 경고 후 명령은 실행됨.
- **검증/한계:** 조건 창·반복 체크박스와 NaruPlayer 조건부 이벤트는 위 6항목에 대한 사용자 수동 테스트 통과 보고로 확인했다(2026-10-31). 한 객체에 여러 페이지·셀프 스위치는 없음(설명서 9장).

## 공용 변수·스위치 (작업 순서 4) — 2026-10-29

- 엔진: `VariableDef{name, initial(Value), description}`, `Project::variables`·`findVariable`·`initialVariables()`. 종류는 처음 값 형식(int64 정수·double 소수·string 글자·bool 스위치) — 스위치는 별도 타입이 아니라 bool 변수. project.json `"variables": [{name, value, description?}]`, 빈 이름·중복·null/잘못된 값은 불러올 때 오류. `GameWorld` 생성 시 처음 값 적용(장면 전환에도 유지). 새 프로젝트에 `box_opened`(0) 예제.
- 에디터: `VariablesDialog`(게임 → 변수·스위치..., 표: 이름·종류·처음 값·설명, 변수/스위치 추가·삭제·위/아래, OK 때 검사) → `SetVariablesCommand`(Undo). F6(`EventEditor`)·F5(`MainWindow::runScript`) 세션도 처음 값으로 시작. `CommandDialog` 변수 칸을 편집 가능한 콤보로(정의 목록 + 직접 입력), 아래 줄에 종류·처음 값·설명 또는 "목록에 없는 이름". 목록에서 고르면 값 형식을 맞춤(스위치면 =/== 참).
- 목록에 없는 이름은 계속 허용(이전 동작 유지). 사용 중인 변수 삭제 경고·이름 바꾸기 일괄 반영은 없음.
- **검증:** ctest 9/9 — project(네 종류 왕복, 새 프로젝트 예제, 중복·빈 이름·null·배열 값 거부), game(처음 값으로 시작, 스위치 조건, 장면 이동 뒤 유지). 촬영 옵션 `--variables-dialog`, 그림 17 추가, 06·07·11 재촬영(예시 프로젝트에 변수 정의). F6 실제 확인: 복사 프로젝트에 box_opened=5 정의 → 상자 이벤트가 "그렇다면"(이미 열어 본 상자다)로 감(화면 촬영). **미검증:** 변수·스위치 창을 사람이 직접 눌러 본 확인(추가·삭제·순서·검사 메시지).

## 게임 실행 시작 장면 — 2026-10-28

- 사용자 지적: 장면을 추가하면 그 장면이 열려 있어 Ctrl+R이 새 장면부터 시작했다. **Ctrl+R = 시작 장면(▶)부터**(player에 `--scene`을 넘기지 않음), **Ctrl+Shift+R = 현재 장면부터 실행**(게임 메뉴)으로 나눔. 제작툴 자동 검사는 현재 장면부터 실행을 쓴다. 설명서 7·8장, `SHORTCUTS.md` 갱신.
- 장면 트리에서 장면을 오른쪽 클릭하면 "시작 장면으로 지정" 메뉴 (이미 시작 장면이면 비활성). 자동 검사 없음 — 사용자 확인 필요.
- 같은 날 수정: 명령 종류 메뉴에 "장면 이동" 이름이 빠져 빈 줄이던 것, 메뉴 페이드인 중 글자가 흐린 것(`UI_FadeMenu`/`UI_AnimateMenu` 끔).

## 장면 이동 (작업 순서 3) — 2026-10-28

- 명령 `change_scene`(scene·setPosition·x·y·direction, direction ""=그대로). 위치 칸은 setPosition을 꺼도 저장해 값이 사라지지 않게 함.
- 엔진: `GameWorld::Arrival{x, y, facing}`, `enterScene(id, arrival)`, `requestScene(id, arrival)`, `sceneChangePending()`. 도착 위치는 장면 복사 직후 플레이어 객체에 적용하고 그 뒤 `on_scene_enter`·자동 실행. 플레이어 없는 장면이면 경고만. 명령은 요청 후 Waiting으로 남아 장면 전환 때 취소됨 → 뒤 명령 실행 안 함(설명서에 "도착 후 할 일은 새 장면의 자동 실행"). "그대로" 방향은 명령 시점의 facing을 넘김.
- Python `ctx.scene.change(id, x=None, y=None, facing=None)` — x·y는 함께, facing 잘못되면 ValueError. `SCRIPT_API.md` 5장·틱 순서(2번에서 빠졌던 "객체 이동" 단계도 추가).
- 에디터: `CommandDialog`가 `Database*` 대신 `Project*`를 받음(장면 목록). 장면 고르기·도착 위치 지정 체크·X/Y·"플레이어 위치로"(그 장면 플레이어 좌표 채우기, 새 명령은 자동으로 채움)·방향(그대로 포함). 목록 표시 `장면 이동: 여관 안 (600, 640), 위`, 없는 장면은 "(없는 장면)".
- **검증:** ctest 9/9 — game 26건(새 3: 도착 위치·방향·남은 명령 안 함·새 장면 자동 실행, 위치 없음=편집 위치·방향 그대로, 없는 장면 실패), events 11건, project(왕복), python 19건(위치·방향, x만/잘못된 facing ValueError). NaruPlayer 실제 실행: 문지기 자동 실행 → 여관 안 (100, 500) 위를 보고 도착, 남은 로그 없음, 새 장면 자동 실행 로그 확인(화면 촬영). 그림 16 추가.
- **미검증/한계:** 페이드 등 전환 연출 없음. 새 장면 파일을 손으로 만들 때 객체 ID가 프로젝트 전체에서 겹치면 열리지 않음(기존 규칙, 에디터는 자동으로 피함). 입력 창을 사람이 직접 눌러 본 확인은 없음.

## 객체 이동·보이기/숨기기 (작업 순서 2) — 2026-10-28

- 사용자 확정(제안 그대로): 이동 중 막히면 그 자리에서 멈추고 다음 명령으로, 속도 단위 px/초·기본 240(플레이어 속도와 같음).
- 데이터: `SceneObject::visible`(JSON `visible`, false일 때만 저장). 명령 `move_object`(target·direction·distance·speed·wait), `set_visible`(target·visible). 대상 `@self`(이벤트가 붙은 객체)·`@player`·객체 ID. 잘못된 방향은 로드 거부.
- 엔진: `CommandContext::selfObjectId`. `GameWorld` 이동 경로(`startObjectMove`/`objectMoving`/`stopObjectMove`, 틱 순서: 기본 조작 → 이동 경로 → 접촉 판정), speed 0이면 즉시 이동, 같은 객체 새 이동은 앞 이동을 대신, 장면 진입 시 모두 정지, 기다리던 명령이 취소되면 그 이동도 정지(기다리지 않는 이동은 계속). `resolveTarget`, `setObjectVisible`. 숨긴 객체: 충돌·조사·접촉 제외, 숨긴 플레이어는 아무것과도 닿지 않음, 자동 실행·병렬은 실행. 게임 밖(F6)에서는 두 명령 모두 로그만.
- Python `obj.visible`(읽기·쓰기), `SCRIPT_API.md` 5-1. player는 숨긴 객체를 그리지 않음.
- 에디터: 명령 메뉴에 두 명령, 입력 창(대상 목록 = 이 객체/플레이어/장면 객체, 지워진 객체는 "(장면에 없음)"으로 유지), 목록 표시 `객체 이동: 문지기 → 오른쪽 96px (속도 120, 끝날 때까지 기다림)`, 인스펙터 "보이기 — 게임에서 보임"(Undo), 캔버스에 숨긴 객체 흐리게 + "(숨김)". 촬영 옵션 `--command-dialog move_object|set_visible`.
- **검증:** ctest 9/9 — game 23건(새 5: 속도대로 걸어가고 도착까지 대기, 기다리지 않음·막히면 멈춤·가장자리, @player·이름 지정·없는 대상 실패, 숨긴 객체 통과·조사 안 됨·접촉 안 됨·병렬은 실행, 보이기/숨기기 명령), events 10건(게임 밖 로그·잘못된 방향 실패), project(새 명령·visible 왕복), python(`visible`). NaruPlayer 실제 실행: 자동 실행 이벤트로 문지기 640→480 이동 → 상자 숨김 → 주인공 순간 이동 400px가 숨긴 상자를 지나 x=720 (화면 촬영으로 확인). 설명서 4장 "객체 이동·보이기/숨기기" 절, 그림 14·15 추가.
- **미검증/한계:** 에디터 입력 창·인스펙터 체크 상자를 사람이 직접 클릭한 확인은 없음(촬영으로 화면만 확인). 이동 루트(여러 단계)·이동 중 방향 그림·대각선 이동 없음. 에디터 자기 검사에 새 명령 단계는 추가하지 않음.

## 2026-10-27 커밋 기록

| 커밋 | 내용 |
|---|---|
| `518dac4` | M0: CMake·엔진·Python 임베딩·Qt 창·테스트 |
| `00a874e` | 프로젝트 작업 흐름·장면 편집·이벤트 편집·그림 설명서 |
| `59c93ba` | 사용자 정의 데이터베이스·실행 조건 5종·로드맵 |
| `118e900` | `dev.cmd` (빌드→테스트→실행) |
| `b75d2ec` | SDL3 NaruPlayer·게임 실행(Ctrl+R)·플레이어 이동/충돌·조사/접촉·단축키 모음 |
| `53f5c20` | 전역 스크립트·반환값 → 변수·`ctx.input/player/scene`·`SCRIPT_API.md`(+누락 검사 테스트) |
| `fd8c70c` | 스크립트로 다른 객체 조작·이벤트 접촉 완성·경로 충돌 검사·로드맵 19단계 + 게임 기능 설계(3장) |
| `ad0bc16` | `docs/examples` 예제 4개(따라오기·순찰·밀기·문 열기) + 예제 검사 테스트 |

## 스크립트 예제 — 2026-10-27

- 사용자 요청으로 다른 객체 조작 예제를 만들어 `docs/examples/`에 넣음: `chase.py`(전역), `patrol.py`(전역, 모듈 전역 변수로 방향 기억), `push.py`(플레이어 접촉 이벤트에서 호출, 바라보는 방향으로 48px, 움직였는지 반환), `door.py`(조사 이벤트에서 호출 → passable·color, 게임 변수 `door_opened`, 전역 등록 시 `on_scene_enter`로 다시 열기), `README.md`.
- 이름 앞에 `CHASE_`/`PATROL_`/`PUSH_`/`DOOR_`를 붙여 모두 전역 등록해도 겹치지 않게 함.
- **검증:** 새 프로젝트에서 player로 실제 실행 — 순찰(640→800→720 지점), 밀기(부딪힐 때마다 640→688→…→928), 따라오기(주인공 옆 x=384에서 멈추고 이벤트 접촉 대사). 문 열기는 같은 기능을 python 테스트로만. python 테스트 18건(예제 문법·함께 전역 등록 시 이름 겹침 없음). ctest 9/9.
- 질의응답 기록: 정해진 훅 이름(`on_update` 등)만 자동 호출, 그 밖 함수는 이벤트·코드가 불러야 함 / 이벤트에서 이름으로 쓰는 것은 `ctx.variables`의 게임 변수뿐(파이썬 변수 아님) / 다른 객체는 ID(`obj_002`)로 `ctx.scene.object`.

## 스크립트로 다른 객체 조작 (작업 순서 1) — 2026-10-27

- 커밋 `53f5c20`(전역 스크립트) 푸시 후 진행. 사용자: "순서대로" — ROADMAP 2-0 표의 0·1 완료.
- 엔진: 이동·충돌을 모든 객체로 일반화(`moveAxes`/`moveAxis`가 막은 객체 목록 반환). 규칙: 통과 불가인 것끼리만 막음(통과 가능 객체는 막히지도 막지도 않음 — 플레이어도). **경로 검사**(이전~새 위치를 합친 사각형)로 큰 이동이 벽을 건너뛰던 버그 수정 — 새 테스트가 잡음. `moveObject`/`setObjectPosition`/`setObjectPassable`/`setObjectColor`/`object`. 이벤트 접촉: 객체가 움직여 플레이어에 부딪히면(틱 끝 판정, 새로 부딪힐 때만) 또는 겹치면 실행, 플레이어 접촉 객체는 객체 쪽 접근으로는 실행 안 함. `queueBlocking` 중복 방지.
- Python: `naru.SceneObject`(id·name·x·y·width·height·passable·color·is_player·move), `ctx.scene.object/find/objects`, 없는/예전 장면 객체는 KeyError, color 형식 검사.
- 문서: `SCRIPT_API.md` 5-1장·충돌/접촉 규칙 갱신(문서 검사 테스트가 누락 이름 6개를 먼저 잡음), 설명서 4장 이벤트 접촉·6장 객체 예제. ROADMAP: 작업 순서 10~19(에셋·맵·캐릭터 애니메이션·스킬 이펙트·연출·저장/메뉴·RPG 시스템·전투·횡스크롤·내보내기) 추가, **3장 "게임 기능 설계"**(A~L 무엇을 어떻게) 신설 — 사용자 요청.
- **검증:** ctest 9/9(에디터 포함 — 사용자가 창을 닫은 뒤 재실행), 에디터 자기 검사 36단계, game 18건, python 17건. 새 프로젝트에서 스크립트로 상자를 주인공 쪽으로 움직이고 상자를 이벤트 접촉으로 바꿔 player 실행 → 상자가 주인공 옆(x=384)에서 멈추고 대사 실행 확인(화면 촬영).

## 전역 스크립트 — 2026-10-27

- 커밋 `b75d2ec`(SDL3 player·플레이어 조작)를 푸시한 뒤 진행.
- 사용자 요구: 전역 스크립트를 **지정할 방법**이 있어야 함 / 이벤트의 스크립트 호출로 결과를 받거나 **전역 스크립트의 전역 변수를 수정**할 수 있어야 함 / **전역 스크립트끼리 변수 이름이 같으면 오류**.
- 데이터: `Project::globalScripts`(project.json `globalScripts`, 순서 보존, 잘못된 이름·중복 거부). 새 프로젝트에 `scripts/game.py`(Shift=dash, `combo`/`add_combo` 예제) 등록.
- 엔진: `InputState`를 동작 이름(문자열) 기반으로, `InputMap`(동작→키 이름, 별칭 Enter/Esc/Shift/Ctrl/Alt). `GameScripts` 인터페이스(load→오류 수, call→Ok/Missing/Failed), `GameWorld::startGame`·훅 순서(on_game_start → enterScene: on_scene_exit/on_scene_enter, 틱마다 on_update → 막는 이벤트 → 기본 조작 → 접촉 → 병렬 → 예약 장면 이동). 스크립트용 API: `setDefaultControl`, `movePlayer`(충돌·접촉), `setPlayerPosition`, `interact`, `requestScene`, `input()`(새로 누름은 프레임 첫 틱만, 대사를 넘긴 결정 키는 consume). 접촉 판정을 틱 끝 한 번으로 모음(기본 조작·스크립트 이동 합산). `CommandContext::game`.
- 스크립트 호출: `ScriptRunner`가 `ScriptCallResult{error, returned}` 반환, 명령의 `variable`(반환값 → 변수, JSON `variable`). 편집 창에 칸 추가, 목록 표시 `스크립트 호출: v = file · fn()`.
- Python: `loadModule`(sys.modules에 파일 이름으로 등록 → `import game`), `callModuleFunction`, `unloadModules`, `runFile(..., returned)`(반환값 변환, 컴파일 오류도 로그). `checkGlobalScripts`(ast로 최상위 대입 이름 수집 — import·함수·_ 제외, 겹치면 두 위치, 표준 모듈 이름, 문법 오류). `ctx.input`/`ctx.player`/`ctx.scene`/`ctx.busy`(게임 밖은 None/False). `PythonGameScripts`(스크립트·훅별 실패 시 그 쌍만 중지).
- player: 키 이름 집합 → 매 프레임 InputMap으로 동작 계산(스크립트가 바꾼 배치 즉시 반영), `--hold`에 임의 동작 이름, 전역 스크립트 로드, 이벤트 호출이 전역 모듈이면 그 모듈 함수 실행.
- 에디터: 스크립트 → 전역 스크립트… 창(체크·순서·실시간 검사), 목록 오른쪽 클릭 토글, `[전역 N]` 굵게, `SetGlobalScriptsCommand` Undo, F7에서 전역 검사, `--global-scripts-dialog` 촬영 옵션.
- **검증:** ctest 9/9 — game 15건(훅 순서·없는 훅 재호출 안 함·실패 집계·훅이 변수 공유·기본 조작 끄고 스크립트 이동/접촉·사용자 동작 held/pressed), python 15건(반환값·게임 밖 None·훅+모듈 전역 변수 유지+이벤트에서 모듈 함수 호출+import·dash 키 배치·이름 겹침 두 위치/표준 모듈 이름·오류 훅 1회 보고와 다른 스크립트 계속), project 13건(globalScripts 왕복·잘못된 이름/중복 거부). 에디터 자기 검사 36단계(전역 등록·해제 Undo, 겹침 오류, NaruPlayer에서 on_game_start·on_scene_enter 로그 수신). 새 프로젝트 예제를 player로 실제 실행: 30프레임 오른쪽 이동 x=440(걷기) vs x=530(dash 유지) 확인. 설명서 6장 재작성, 그림 13장(13-global-scripts 추가, 07 반환값 칸 반영).
- **미검증/한계:** 사람이 Shift를 실제로 눌러 본 확인은 없음(키 이름 매칭은 단위 테스트, 동작은 --hold로). 전역 스크립트 창·오른쪽 클릭 메뉴는 모달이라 클릭 자체는 자동 검사하지 않고 같은 함수 경로로 검사. 함수 안에서 `global`로 새로 만드는 이름은 겹침 검사 대상이 아님(최상위 대입만). 스크립트에서 다른 객체 조작·async 대기 없음. ASan 빌드는 다시 돌리지 않음.
## 플레이어 조작 — 2026-10-27

- 사용자 결정: 입력은 **A안 — 엔진이 기본 처리(코딩 없이 이동·조사), 필요하면 스크립트로 끄거나 덧붙임** (RPG Maker 방식). 스크립트 쪽은 P2 전역 훅과 함께.
- 데이터: `Scene::playerObjectId`(JSON `player`, 없는 객체면 로드 거부), `SceneObject::passable`(JSON `passable`, true일 때만 저장). 새 프로젝트의 "주인공"이 플레이어.
- 엔진 `GameWorld`: 방향 입력으로 이동(240px/s, 대각선 정규화, 축별 이동 후 충돌 보정), 막는 객체·장면 가장자리, 처음부터 겹친 객체에는 막히지 않음. 바라보는 방향(Facing). 조사 = 결정 키 새로 누름 + 바라보는 쪽 16px 안의 조사 이벤트(가장 많이 겹친 것, 없으면 밟고 있는 통과 가능 객체). 접촉 = 부딪힘/밟음이 새로 생길 때만(밀고 있거나 서 있는 동안 반복 안 함). 이벤트 접촉은 지금 플레이어 접촉과 동일. 조사·접촉·자동 실행은 하나의 막는 큐로 순서대로, 실행 중 이동 없음. 대사를 넘긴 결정 키·이벤트 중 누른 키로 조사하지 않음.
- 제작툴: 인스펙터 "플레이어(방향키로 조작)"·"통과(지나갈 수 있음)" 체크(Undo 가능), 트리·캔버스 ★, 통과 객체 점선. 플레이어 객체 삭제 시 지정 해제, Undo 시 복원(RemoveObjectCommand).
- player: 바라보는 쪽 흰 표시, `--hold <동작>:<시작>-<끝>` 자동 입력.
- **검증:** ctest 9/9 (game 11건 — 이동·가장자리·충돌·통과·접촉 1회·조사·대사 중 정지·대사 넘긴 키 재조사 안 함, project 12건 — player/passable 왕복·잘못된 값 거부), self-test 32단계(플레이어 지정 Undo, 삭제 복원, NaruPlayer에서 오른쪽 이동 → 상자 앞 결정 키 → 상자 이벤트 로그 수신). 설명서 그림 12장 재촬영(12-game: 이동 후 조사 대사).
- **미검증/한계:** 사람이 키보드로 직접 조작한 확인은 하지 않음(자동 입력으로만). 인스펙터 체크 상자 클릭 자체는 자동 검사하지 않고 Undo 명령 경로로 검사. 클릭 조사, 키 설정, 반복 조건, 객체 이동 없음. ASan 빌드는 다시 돌리지 않음.
## 게임 실행 화면 1단계 + 단축키 모음 — 2026-10-27

- 사용자 요청: 단축키 모음 MD 파일 → `docs/SHORTCUTS.md` (가이드 8장에서 링크). 확인 중 `QKeySequence::Quit`이 Windows에서 비어 있어 Ctrl+Q가 동작하지 않던 것을 직접 지정으로 수정.
- 하위 기반 SDL3 결정(사용자가 다음 작업 실행에 동의). SDL3 3.4.18, SDL3_ttf 3.2.2 VC 개발 패키지를 `%LOCALAPPDATA%\naru-tools`에 설치(해시는 BUILD.md). `dev.cmd`가 자동 탐색, preset의 CMAKE_PREFIX_PATH에 `NARU_SDL3_DIR`/`NARU_SDL3_TTF_DIR` 추가, `NARU_BUILD_PLAYER` 옵션. dev.cmd는 이제 매번 `cmake --preset`을 실행한다(새 의존성 반영).
- 엔진: `naru/runtime/Input.h`(동작 이름 기반 InputState: held/pressed), `GameWorld`(프로젝트 사본, 장면 진입, 자동 실행 큐(하나씩, 조작 차단), 병렬 처리(동시), DialogueHost 구현, 이벤트별 로거가 "객체이름 ▸ 메시지"로 게임 로그 전달, 실패 이벤트 개수). 각 이벤트는 장면 진입 시 1회 — 반복은 스위치/조건과 함께 설계 예정.
- `player/main.cpp` → `NaruPlayer.exe`(WIN32, SDL_main): 장면 크기 논리 해상도 레터박스, 색 사각형, 대사 창(맑은 고딕, 없으면 SDL 디버그 글꼴), 임시 키 배치(Z/Enter/Space 결정 등). 로그는 stdout `NARU\t수준\t명령ID\t파일\t행\t메시지\t부가정보`. `--frames/--auto-confirm/--screenshot`(SDL_SavePNG)로 자동 검사·촬영. 종료 코드 0/1/3.
- 제작툴: 게임 메뉴, 툴바 "게임 실행"(Ctrl+R: 저장 → 현재 장면 `--scene`), 게임 중지(terminate → 2초 후 kill), QProcess stdout을 로그 패널로(`[게임]` 접두). 창 소멸 시 신호를 끊고 player 종료(과거 use-after-free와 같은 유형 예방).
- **검증:** ctest 9/9 (새 game 6건), self-test 27단계 — 새 단계가 실제로 NaruPlayer를 띄워 종료 코드·로그 수신을 확인하다 **trimmed()가 빈 마지막 필드를 지워 로그 파싱이 실패하던 버그**를 잡아 수정. player 직접 실행으로 자동 실행 대사·분기 로그(`2/e/0`) 확인, 한글 대사 화면 촬영 확인. 설명서 7장 "게임 실행" 추가, 그림 12장 전부 재촬영.
- **미검증/한계:** 사람이 키보드로 대사를 넘기는 실제 조작은 자동 확인만(--auto-confirm). 게임 중지 버튼, 창 크기 조절, 고DPI 모니터는 직접 확인하지 않음. `tools/build.ps1`은 SDL3 경로를 아직 모른다. ASan 빌드는 이번에 다시 돌리지 않음. 병렬 이벤트 두 개가 동시에 대사를 띄우면 나중 것이 덮어쓴다.
## 커스텀 데이터베이스 — 2026-10-27

- 커밋 `00a874e`(프로젝트·장면·이벤트·설명서)를 origin/main에 푸시한 뒤 진행.
- 사용자 방향: 아이템/캐릭터/몬스터 고정 구조가 아니라 **속성을 직접 정의하는 데이터베이스**.
- 엔진 `naru/data/Database.h`: DataType(키·이름·FieldDef 목록·DataRecord 목록), FieldType 6종(정수·실수·문자열·참거짓·선택지·데이터 참조), 기본값·최소/최대·선택지·참조 종류. 키 규칙(종류/항목 ID: 영문·숫자·_·-, 속성 키: 식별자, id/name 예약). checkValue/convertValue/emptyValueFor, validate()(끊긴 참조·범위·정의 안 된 값 → 경고), referencesTo, 템플릿(빈/캐릭터/아이템/몬스터 — 고칠 수 있는 정의). 저장 `data/<key>.json` + project.json `dataTypes`, 구조 오류는 열기 거부, 값 문제는 경고.
- 이벤트: `get_data` 명령(데이터 → 변수), CommandContext.database, Session::setDatabase. 실행 조건에 이벤트 접촉·병렬 처리 추가(사용자가 붙여 준 RPG Maker 트리거 목록 반영, 저장만 됨).
- Python: `ctx.db.get(type, id)`(dict: id·name·모든 속성, 없으면 None, 없는 종류는 KeyError), `ctx.db.ids(type)`, `ctx.db.types()`. 읽기 전용.
- 에디터: "데이터베이스" 탭(F9) — 종류/항목/값 편집·속성 정의 3단, ValueEditor(형식별 입력 칸), FieldDialog, 템플릿 메뉴, 검색, 복제, 경고 표시줄, 모든 편집 SetDatabaseCommand로 Undo(같은 칸 연속 변경 병합). 형식 변경 시 값 변환·불가 개수 확인. 참조 중인 종류 삭제 차단. 새 프로젝트에 아이템 2·몬스터 1 예제.
- **검증:** ctest 8/8 (engine 7, events 9, database 7, project 11, python 9, editor smoke, self-test 26단계). ASan 빌드에서 전 테스트·self-test·DB 탭 오류 없음. 설명서 5장 + 그림 4장 추가(08~11), 전체 그림 재촬영.
- **미검증/한계:** 데이터베이스 편집 대화상자(종류/항목 추가, 속성 편집, 삭제 확인)는 모달이라 자동 검사는 편집 API 경로로만 했다. 목록/묶음/이미지 속성, 표 보기, 항목 ID 변경, 실행 중 DB 변경 없음. 이벤트 GetData가 참조하는 종류/속성을 지워도 경고하지 않는다.
- 사용자 질문(답변 예정/완료): 트리거와 스크립트 연결, **전역 스크립트** — 현재 `scripts/`는 이미 프로젝트 공용. 없는 것은 공통 이벤트와 게임 수명주기 훅(on_start, 매 프레임 등). 런타임(게임 실행 화면) 단계에서 함께 설계.
- 사용자 참고 링크: https://namu.wiki/w/RPG%20Maker%20XP#s-2 (RPG Maker XP 정보, 아직 읽지 않음).

## 이벤트 편집기 — 2026-10-27

- 엔진: `EventData`(명령 7종: say/wait/set_variable/if_variable/log/call_script/comment, 실행 조건 action/touch/autorun), `EventCompiler`(조건 분기를 점프로 펼침, 명령 ID = 편집 위치 "1/t/0"), `DialogueHost`(대사는 넘길 때까지 대기), EventRunner 점프·즉시 명령 1만 개 한도. 없는 변수는 숫자 연산/비교에서 0. 스크립트 호출은 ScriptRunner 콜백으로 엔진이 Python에 직접 의존하지 않음.
- 저장: 객체 `event` 필드. JSON 읽기는 nlohmann 예외 대신 필드별 한국어 오류("'x'은(는) 숫자이어야 합니다"). 중첩 32단계 제한.
- 에디터: "이벤트" 탭(EventEditor) — 명령 트리, 빈 ◆ 줄 더블클릭 추가, CommandDialog 입력 폼, 삭제·이동, 모든 편집 Undo. F6 테스트 실행(QTimer 실시간, 현재 명령 노란 강조, 대사 미리보기 "다음 ▶"), F8 중지, 실패 명령 빨간 선택. 객체 더블클릭/Enter로 열기. 새 프로젝트의 "상자"에 예제 이벤트.
- **버그 수정:** 창 종료 시 QGraphicsScene 정리 중 선택 변경 신호가 이미 삭제된 인스펙터에 전달되던 use-after-free(이전 커밋부터 있었음, 객체 선택 상태로 종료 시). ASan으로 찾아 SceneView 소멸자에서 신호 차단.
- **검증:** ctest 7/7 (engine 7, events 9, project 9, python 8, editor smoke, editor self-test 16단계). 별도 ASan 빌드(`build/asan`, 커밋 안 함)에서 전 테스트·self-test 오류 없음. 이벤트 탭 스크린샷 확인.
- **원인 미확정:** ASan 빌드에서만 nlohmann type_error가 ProjectIO 안에서 던져지면 프로세스가 0xC0000409로 종료됐다(단독 재현은 정상). 필드 형식 직접 검사로 바꿔 nlohmann 예외를 쓰지 않게 했으므로 현재 코드는 영향 없음. 디버거가 없어 근본 원인은 확인 못 함.
- **미검증/한계:** 명령 추가 메뉴·입력 대화상자는 모달이라 자동 검사하지 않음(편집 API 경로로 검사). 테스트 실행의 변수는 매번 비어 있음(프로젝트 변수 정의 없음). 실행 조건은 저장만 되고 게임 런타임이 없어 아직 동작하지 않음. 선택지·장면 이동·이미지/음원 명령 없음.
- 사용자 질문: 고정된 아이템/캐릭터/몬스터 대신 **속성을 커스텀할 수 있는 데이터베이스** 방향 논의 중 (답변에 스키마 기반 설계 제안).

## 프로젝트 단위 제작툴 + 장면 편집 — 2026-10-27

- 사용자 방향: **프로젝트 단위로 작업한다. exe에서 프로젝트를 만들거나 불러와 그 안에서 작업.** 첫 UI는 장면 편집(사용자 선택).
- 엔진: `engine/project/` Project·Scene·SceneObject 모델, ProjectIO(create/load/save). 폴더 `project.json` + `scenes/<id>.json` + `scripts/` + `assets/`. formatVersion 1, 새 버전·깨진 JSON·ID 불일치·경로 문자 ID·중복 객체 ID·필드 형식 오류를 파일 이름과 함께 거부. 임시 파일 후 교체 저장, 삭제한 장면 파일 정리. JSON은 `third_party/nlohmann` 3.11.3 (MIT, 해시 기록).
- 에디터: StartDialog(새로 만들기/열기/최근 — QSettings), ProjectDocument(QUndoStack), UndoCommands(장면 추가/삭제/속성/시작 장면, 객체 추가/삭제/속성·이동, 숫자 칸 연속 변경 병합), SceneView(QGraphicsView 드래그 배치·휠 확대·창 맞춤), Inspector, ScriptEditor 탭, 프로젝트 닫기→시작 화면.
- **검증:** ctest 6/6 — engine 7, project 8, python 8, editor_smoke, editor_selftest 10단계(객체 추가·선택·이동·Undo·저장·재로드 동일·삭제·main.py 실행). 프로젝트 창 스크린샷 확인. 빌드 경고 0.
- **미검증:** 시작 화면·새 프로젝트 대화상자는 모달이라 자동 검사/스크린샷을 하지 않았다(사용자 실행으로 확인 필요). 마우스로 실제 드래그하는 동작은 자동 검사가 아니라 이동 신호 경로로만 검사했다.
- **아직 없음:** 객체 이미지/에셋 가져오기, 이벤트(명령 목록) 편집, 객체에 스크립트·이벤트 연결, 복사/붙여넣기, 격자 스냅, 장면 실행 미리보기(게임 화면), 대기형 스크립트, 변수 정의 데이터.
- 다음 후보: (1) 에셋 가져오기 + 객체 이미지 표시, (2) RPG Maker식 이벤트 편집기와 객체 연결, (3) 실시간 실행 루프·F8 중지.

## M0 완료 — 2026-10-27 (다른 PC: DKSYSTEMS)

- 저장소를 새 PC에 clone해 이어감. 이 PC에는 MSVC·CMake·Qt가 없어서 사용자 승인 후 설치: VS 2022 Build Tools(MSVC 14.44, C++ 워크로드), CMake 4.4.4, Ninja 1.13.2(winget), **Qt 6.8.3 LTS msvc2022_64**(aqtinstall → `C:\Qt`). Python은 기존 **3.14.3 x64**(헤더·lib 포함) 사용.
- 이 PC는 Python HTTPS가 `SSLError`(인증서 검사 환경)로 실패했다. aqt venv에 `pip-system-certs`를 넣어 해결. curl은 정상.
- 새 소스: `CMakeLists.txt`, `CMakePresets.json`(windows-msvc / engine-only), `engine/`(Log, Value/Variables, Command·EventRunner·기본 명령 Log/Wait/SetVariable/Call, 고정 틱 Session), `scripting/python/`(PythonRuntime: 격리 config, `_naru` 내장 모듈 + `naru.Context`, print→로그, 오류 파일·행·traceback, 매 실행 파일 재로드), `editor/`(Qt Widgets `NaruStudio.exe`), `tests/`, `docs/BUILD.md`, `tools/build.ps1`.
- **검증:** RelWithDebInfo 빌드 경고 0(/W4). ctest 3/3 통과 — 엔진 7케이스, Python 8케이스, 에디터 스모크. 에디터에 `runtime_error.py --run`을 열어 오류 행 선택·로그 위치 표시를 스크린샷으로 확인.
- 테스트가 실제 버그 1건을 잡음: `PyErr_Format`에 한글 서식 문자열 → ValueError로 바뀜. `PyErr_SetString`으로 수정.
- **미검증:** `tools/build.ps1`은 이 세션에서 실행 정책 우회가 차단되어 직접 실행하지 못했다(같은 단계를 수동으로 실행해 검증). 다른 PC에서의 clone→빌드 재현, Debug 구성(python314_d.lib 필요)도 미검증.
- **아직 없음:** async/대기형 스크립트(명확한 NotImplementedError로 보고), 실시간 게임 루프 연결, 장면·이벤트·인스펙터 UI, JSON 프로젝트 저장, 렌더링/오디오(SDL3 등 미정), 배포.
- 다음: M1 나머지 — Project/Scene/Object 데이터와 JSON 왕복 저장, 에디터에서 Session을 QTimer 실시간 루프로 돌리고 F8 중지, 대기형 Python 명령(awaitable) 설계.

## 이전 종료·재개 기준 — 2026-10-27

- 새 `game_tool` 폴더로 548개 파일을 복사해 SHA-256 대조 완료. 후속 Unity REA/공개 소스 조사와 최신 문서를 추가 동기화했다. 원본 game/engine-tool은 보존용이며 구현은 새 폴더에서만 진행한다. 새 폴더의 MIGRATION_MANIFEST.json/HANDOFF_MANIFEST.json이 각각 초기/추가 이관의 근거다. 원격 푸시 결과는 Git HEAD와 origin/main 대조로 별도 확인한다.

- 후속 사용자 요청으로 Unity REA 분석을 추가했다. `analysis/unity/02_REA_및_공개소스_조사.md` 참고. 설치 UnityEditor.dll 기본 메타데이터 검사 complete, 7개 타입 관측, members 상세는 64MiB 용량 한도로 partial. 공개 UnityCsReference 커밋 `bf6028387a4fc2ea6d41aa7d8998e774655aedf6`의 7개 C# 파일을 직접 읽어 인스펙터·직렬화·Undo·프리팹·플레이 상태를 설계에 연결했다. 공개 소스 버전 7000.0.0a12는 설치본 6000.6.5f1과 동일 빌드로 확인되지 않았다. 원시 번들/요약/타입 페이지/소스 출처·해시를 저장했다. 전체 분석 완료는 아니다.

- 사용자가 취침을 위해 구축을 멈추고, 새 폴더에 조사 자료·소스·개발/실행 안내·다음 구현 순서를 저장해 푸시하도록 요청했다. 오늘은 새 설치나 C++ 구현을 시작하지 않는다.
- **확정 목표: 자체 게임 엔진과 제작툴. 기술은 C++ 엔진 + Qt 제작 UI + Python 게임 확장 스크립트.** Godot·Unity·RPG Maker는 조사/기능 참고 대상이며 기존 엔진을 감싼 제품을 만드는 것이 아니다.
- Python 스크립트는 RPG Maker처럼 게임 프로젝트의 이벤트/게임 기능과 연결하는 확장 코드다. 제작툴 언어와 혼동하지 않는다. 즉시 실행/중지·선택 명령 테스트·오류 파일/행/traceback/로그도 요구사항이다.
- 게임 기획·에셋은 기존 `game/`에 둔다. 제작툴 개발 기준은 `C:\Users\kjk98\OneDrive\바탕 화면\game_tool`, 저장소는 `https://github.com/Aristia0930/Naru_Studio.git`이다. 다른 PC에서는 clone 후 경로를 고정하지 않고 이어간다.
- **다음 세션 첫 문서: [NARU_NEXT_STEPS.md](NARU_NEXT_STEPS.md)**. 목표·조사 커버리지·실험본/제품 구분·설치 후보·새 소스 구조·M0~M5 순서·검증 기준을 기록했다. 새 저장소용 안내는 REPOSITORY_AGENTS.md를 AGENTS.md로 복사한다.
- `app/`에 Godot/GDScript 실험본 구현 완료. 저장/undo/redo·에셋·8개 이벤트 명령·코드 호출·즉시 실행/중지·로그. 통합 검사 39개 통과, 실제 UI 스크린샷 확인, 엔진을 포함하지 않은 export 프로젝트의 별도 실행 검증 완료. 실제 자체 엔진 제품이나 정식 배포 EXE 검증으로 취급하지 않는다.
- **C++/Qt/Python 소스·빌드 골격·실행 파일은 아직 없다.** Java/JavaFX 코드는 작성하지 않았다. C++/Qt 개발 도구 설치도 안 했다. PATH 확인만으로 PC 전체 미설치를 단정하지 않는다.
- Unity·Godot·RPG Maker 조사 문서·참고 소스·REA 원시 Evidence·검증 코드를 이관한다. 전체 제품 전수 조사 완료는 아니며 조사 범위와 남은 부분은 NARU_NEXT_STEPS.md와 각 조사 문서에 구분했다.
- 대형 Godot/Ghidra 설치 바이너리와 캐시는 Git에 올리지 않는다. Ghidra 기존 설치는 Codex MCP 설정이 기존 경로를 참조하므로 옮기거나 삭제하지 않는다. 원시 Evidence의 예전 절대 경로는 당시 provenance다.
- 이관/푸시의 실제 완료 여부는 아래 후속 결과 기록과 새 폴더의 MIGRATION_MANIFEST.json, Git 원격 커밋 대조로 확인한다. 이 항목 작성 시에는 아직 복사/푸시 전이었다.

## 최신 게임 제작툴 방향·3종 조사·기초 설계 — 2026-10-26

- 사용자 최신 방향: **RPG Maker의 제작 기능을 기준으로 하고 Unity·Godot의 더 나은 기능/작업 방식을 모아 사용하기 쉬운 UI의 새 툴을 만든다. RPG Maker에 없는 기능도 UI로 쓰기 좋으면 추가한다.** RPG Maker식 화면만 흉내 내는 범위가 아니다.
- 턴제·2D 횡스크롤 모두 유지. 음악·효과음 삽입과 재생 설정, 이미지/캐릭터 애니메이션, 타임라인·카메라·충돌·UI 레이아웃을 후보에 포함한다. 기존 초안의 장비·자동 조우 등 제외는 최종 결정이 아니다. 실제 상세 범위·우선순위·전투 규칙은 미정이다.
- Godot 4.7.2 standard Windows x64를 공식 릴리스에서 다운로드, 공식 asset digest와 SHA-256 대조 완료. `refs/godot-4.7.2/bin/`에 실행 파일, 같은 커밋의 핵심 소스 일부와 LICENSE·수집 manifest 보관. 전체 소스 다운로드는 아니다.
- Godot headless 연구 프로젝트 `probes/godot-foundation/`: 씬·리소스 왕복 저장, 소유권, 두 씬 인스턴스 독립, 시그널 연결/해제, 수평 이동·벽 충돌, 카메라 부착 등 **10개 통과**, 최종 실행 코드 0·stderr 비어 있음. 시각적 UI·오디오·애니메이션·실제 카메라 추적·Android 빌드는 미검증.
- REA로 MV 공개 코어 DataManager/BattleManager/Game_Event/Game_Interpreter 4파일 재조사. 파싱 실패 0, AST 15,191개, 나머지 183파일 미선택, 전체 그래프 partial. `analysis/rpgmaker/08_REA_재조사.md`와 요약/전체 Evidence 저장.
- Ghidra 12.1.4로 Godot 콘솔 래퍼 진입점·초기화·실행 처리 3함수 분석. 본체 실행·로그 핸들 전달·종료 코드 수집 정적 근거 저장, 내보내기 뒤 close_binary 완료. Godot 에디터 본체 전체 분석으로 기록하지 않는다.
- Unity는 설치된 `C:\Program Files\Unity\Hub\Editor\6000.6.5f1\Editor\Unity.exe`, ProductVersion `6000.6.5f1_3ff58d469c8a` 확인. AndroidPlayer/WebGLSupport/windowsstandalonesupport 폴더 존재. 공식 6.6 문서와 비교했으며 GUI 실행·새 프로젝트·빌드·바이너리 디컴파일은 하지 않았다.
- **최신 설계 초안: [TOOL_FOUNDATION.md](TOOL_FOUNDATION.md)**. 기능 비교표·UI 구성·음악/애니메이션·공통/장르 구조·저장 형식·툴 EXE와 게임 EXE/APK 생성 관계·기반 후보·첫 검증 순서 정리. 기존 `07_기초뼈대_설계.md`보다 최신 사용자 방향과 이 문서를 우선한다.
- Godot 기반 시제품을 우선 후보로 추천했으나 **사용자가 최종 실행 엔진을 채택한 것은 아니다**. 웹/Capacitor·Unity 기반과 비교 유지. 제작툴 자체를 독립 앱으로 할지 기존 엔진의 확장으로 할지도 미정.
- 다음: 이 기초 설계를 바탕으로 프로젝트 열기→장면/데이터/이벤트 UI→저장·재로드→음악/연출 미리보기의 작은 시제품. 첫 구현 분량과 기반 선택을 구체화한다. 다운로드·MCP 설치 검증·기존 게임 대본 작업을 다시 시작하지 않는다.
- 상세 근거: `analysis/godot/README.md`, `analysis/unity/README.md`, `analysis/rpgmaker/08_REA_재조사.md`, `analysis/foundation/source-observations.json`.

## 현재 Codex MCP의 Ghidra 연결 확인 완료 — 2026-10-26

- 현재 Codex에서 `mcp__rea__binary_session({expected_package_version: "6.8.0"})` 직접 호출 성공. Ghidra provider 후보의 availability는 `available`이며 구성·플랫폼·아키텍처·설치·버전·headless·네이티브 디컴파일러·Java 검사 8개 모두 통과했다.
- 실행 중인 MCP가 Ghidra 12.1.4 설치 루트와 `C:\Program Files\Java\jdk-21`(21.0.8)을 인식함을 확인했다. REA 6.8.0, 클라이언트 codex-mcp-client 0.164.0, alignment `aligned`, 도구 229개다.
- 열린 대상 없음(`open: false`), 정리 대기 없음(`cleanup_pending: false`). 대상별 지원 여부는 아직 `unknown`이며 provider는 선택되지 않았다. 이번 확인은 환경 반영 확인이며 새 대상 분석이나 실행은 하지 않았다. 실제 디컴파일 검증은 아래 기존 CLI Evidence를 재사용한다.
- 원시 응답: [codex-ghidra-session-check-20261026.json](analysis/rea-connection/codex-ghidra-session-check-20261026.json). 설정 변경·재설치 없이 확인 완료. 다음 작업은 Unity·Godot의 필요한 기능과 근거 조사다.

## 이전 Ghidra 설정·실제 분석 검증 완료 — 2026-10-26

- 사용자가 Ghidra 12.1.4 PUBLIC 실행 배포본을 추가했다. 설치 루트는 `ghidra_12.1.4_PUBLIC_20260921/ghidra_12.1.4_PUBLIC`이다.
- 해당 루트와 `JAVA_HOME=C:\Program Files\Java\jdk-21`로 REA 6.8.0 검사 통과(`healthy: true`, Ghidra 범위 모든 검사 정상). `environment_healthy: false`는 선택하지 않은 다른 분석기 상태도 포함하므로 Ghidra 실패로 해석하지 않는다.
- setup dry-run 검토 뒤 `setup --client codex --yes --format json` 적용 완료(`status: ready`, `configured_codex`). Codex REA 환경에 두 경로 저장. 시스템 전역 JAVA_HOME은 변경하지 않았다.
- 변경 대상 `C:\Users\kjk98\.codex\config.toml`; 최초 `.rea.backup`을 보존하고 변경 직전 `.rea.backup.aea8f6e9b21eb5b2c2008733b18a32c85734d7e7c5dfe5c41b8588449ddf6e21.recovery` 생성 계획을 확인했다. REA 실행기는 기존 npx 명령에서 검증에 사용한 Node·캐시의 rea.mjs 직접 실행 경로로 setup이 갱신했다. 대응 스킬 교체 계획은 없었다.
- 실제 CLI 검증: Windows `hostname.exe`의 SHA-256 동일 사본을 정적 분석. Ghidra 12.1.4가 `0x140001300` 진입 함수의 의사 코드를 반환했고 종료 코드 0, stderr 비어 있음. 대상 프로그램을 실행하지 않았다.
- Evidence: `ev_4413830bf2325b260898e1336eeda0c69a8b20d5d0a6852283ec8367e899e719`. 원시 결과와 요청은 `analysis/rea-connection/ghidra-native-result.json`, `ghidra-native-request.json`에 저장. 재현 코드 `tools/verify_ghidra.py`.
- 당시 남은 작업인 현재 MCP의 Ghidra 환경 반영 확인은 위 후속 직접 호출로 완료했다. 이 CLI 분석과 현재 MCP의 환경 확인은 별도 검증이며 큰 분석은 반복하지 않았다.
- 사용법: [GHIDRA_SETUP.md](GHIDRA_SETUP.md). 12.2 검사 결과는 아래 과거 기록이다.

## 이전 Ghidra 12.2 검사 — 2026-10-26

- 사용자가 실행 배포본 `ghidra_12.2_PUBLIC_20261016/ghidra_12.2_PUBLIC`을 추가했다. 소스 폴더와 별개이며 headless 실행기·Windows x64 디컴파일러 존재 확인.
- 해당 폴더를 `GHIDRA_INSTALL_DIR`, 기존 JDK 21을 `JAVA_HOME`으로 지정한 일회성 셸에서 REA 6.8.0 `doctor --provider ghidra --format json` 실행. 샌드박스 권한 오류 이후 승인된 환경에서 재검사 완료.
- 재검사 결과: REA는 Ghidra 12.1.x만 허용하여 12.2 거부. Ghidra 12.2는 JDK 25 이상을 요구하며 기존 JDK는 21.0.8이다. 실행 배포본 압축 해제 문제는 아니다.
- 원시 기록: [ghidra-12.2-doctor.json](analysis/rea-connection/ghidra-12.2-doctor.json). 지속 환경변수·Codex 설정·Java 설치는 변경하지 않았다.
- 다음 준비: 현재 REA와 사용할 Ghidra 12.1.4 PUBLIC 배포본 확보. 12.2를 사용하려면 이를 지원하는 REA 확인과 JDK 25 이상이 모두 필요하다.

## 현재 Codex 세션 통합 연결 확인 완료 — 2026-10-26

- 현재 세션의 실제 도구 목록에 REA 도구 229개 노출 확인.
- 현재 Codex에서 `mcp__rea__binary_session({expected_package_version: "6.8.0"})` 직접 호출 성공 (`isError` 없음).
- 서버 `rea` 6.8.0, 클라이언트 `codex-mcp-client` 0.164.0, 프로토콜 `2025-06-18`, 정렬 상태 `aligned` 확인.
- 열린 대상 없음(`open: false`), 정리 대기 없음(`cleanup_pending: false`). 분석·원본 실행·재설치·설정 변경 없이 연결 검증 완료.
- 원시 응답: [codex-session-check-20261026.json](analysis/rea-connection/codex-session-check-20261026.json).
- Ghidra는 현재 서버에서 `not_configured`이며 네이티브 분석은 별도 미검증이다. 도구 노출 수가 모든 분석기의 사용 가능 상태를 뜻하지 않는다.

## 이전 작업 결과 — REA 설치·별도 서버 통신 검증

- Node.js 24.22.1 / npm 11.19.1 / Codex CLI 0.164.0 실행 확인 완료.
- 공식 `rea-agents@6.8.0` setup에서 Codex만 지정해 설치 완료. 기존 `~/.codex/config.toml` 보존과 `.rea.backup` 백업 존재 확인.
- `~/.agents/skills/reverse-engineer-anything` 설치 완료 (스킬 metadata version 49, 도구 229개). 설치된 스킬·연결 복구 안내를 읽음. 다음 턴부터 스킬 사용 가능.
- `doctor --client codex` 정상(`healthy: true`, registration/skill `aligned`). Ghidra 설치나 JAVA_HOME 수정은 하지 않음.
- 별도 검증 클라이언트에서 등록된 것과 같은 stdio 명령으로 MCP 초기화·도구 229개 조회·실제 분석 호출·Evidence 내보내기 성공.
- 기존 MV 참고 소스의 `Game_Interpreter.js` 한 파일 정적 분석 성공. 원본 실행 없음, 파싱 실패 0. 전체 관계 그래프는 partial. 기존 수동 분석 전체를 REA 분석 완료로 취급하지 않음.
- 최초 전체 응답이 10MiB 전송 한도를 초과해 `detail: summary` 응답과 전체 Evidence 파일 내보내기로 해결. 한도·기존 MCP 정책은 추가 변경하지 않음.
- 당시 대화에는 REA 도구가 노출되지 않았으나, 후속 세션에서 위 직접 호출로 현재 Codex 통합 연결 확인을 완료했다.
- 검증 기록: [analysis/rea-connection/README.md](analysis/rea-connection/README.md), 코드: [tools/verify_rea_mcp.py](tools/verify_rea_mcp.py).

## 다음에 바로 할 일 (갱신)

1. 3종 첫 비교 조사와 기초 설계 초안은 완료했다. `TOOL_FOUNDATION.md`를 읽고 작은 UI 시제품의 기반·구현 분량을 구체화한다. 상세 기능 전부 조사/구현 완료는 아니며 첫 구현 범위와 최종 기술은 여전히 미정이다.
2. REA 연결 확인은 완료했으므로 설치·대규모 검증을 반복하지 않는다. 향후 도구가 사라질 때만 클라이언트 시작 오류와 등록된 실행 환경을 확인한다.

## 설치 전 재개 계획 (과거 기록)

아래는 이번 설치 전의 계획이며, 위 최신 결과를 우선한다. Node.js 업데이트와 REA 설치를 반복하지 않는다.

1. 새 셸에서 `node -v`, `npm -v`, `codex --version`으로 업데이트 후 실행 상태를 확인한다. Codex CLI 설치 완료는 사용자 확인을 받았지만 설치 후 버전·실행은 아직 직접 확인하지 않았다.
2. REA 공식 저장소의 최신 설치 안내와 CLI 옵션을 확인하고, 사용할 클라이언트인 Codex에 MCP와 대응 스킬을 설치한다. 블로그 예시는 `npx rea-agents@latest setup --client opencode`이며 우리 환경에 opencode용 명령을 그대로 쓰지 않는다. 공식 기본 진입점은 `npx rea-agents setup`이다. Codex 지정 옵션은 실행 전 최신 도움말로 확인한다.
3. 기존 Codex 설정을 보존하며 설치 변경 내용을 확인한다. MCP 등록과 `reverse-engineer-anything` 스킬 설치를 각각 확인한다. 설정 파일 내용만으로 실제 연결 성공으로 기록하지 않는다.
4. 필요하면 사용자에게 Codex 재시작을 안내한 뒤 REA 도구 목록·실제 호출로 연결을 확인한다. 재시작 전에 기록을 갱신한다.
5. 작은 분석 1건으로 검증한다. 기존 `refs/rpgmaker-mv-corescript/`의 이벤트 실행 구조는 검증 후보이며, 분석 대상·근거·한계를 남긴다. 기존 수동 분석 문서를 REA 분석 완료로 취급하지 않는다.

설치·다운로드·사용자 설정 변경에는 실행 환경의 승인 절차를 따른다. 이번 종료 요청은 기록 요청이므로 REA 설치까지 진행한 상태가 아니다.

## 합의한 툴 방향

- 「이면기록」 게임과 게임 제작용 툴을 함께 만든다. 툴 관련 기록·코드는 `engine-tool/`에서 관리한다.
- **턴제 RPG와 2D 횡스크롤 게임을 모두 만들 수 있는 UI 기반 툴**이다. 이전의 타일맵·필드 이동 일괄 제외 방침은 철회했다.
- RPG Maker의 데이터베이스·이벤트 명령·턴제 뼈대, Unity의 인스펙터·드래그 앤 드롭·즉시 테스트, Godot의 씬·이벤트 연결·텍스트 리소스 구조를 참고한다.
- 횡스크롤용 씬 편집·이동·카메라·충돌은 설계 범위다. 타일맵·점프·중력·액션 전투의 상세 범위는 아직 미정이다.
- 게임 자체의 현재 카드 수집형 RPG 기획을 횡스크롤로 변경한 것은 아니다.
- 기존 문서의 ‘Unity 확정’보다 최신 툴 방향을 우선한다. 웹/TypeScript/Canvas·WebGL/Capacitor는 제안이며 최종 채택 전이다.
- REA를 MCP와 스킬로 연결하여 기존 툴의 필요한 기능을 분석하고, 근거를 바탕으로 우리 툴을 설계·구현한다. UI 사용성도 별도 설계한다.

## 설치 전 환경 확인 및 완료 사항 (과거 기록)

2026-10-26 이 세션에서 직접 확인한 상태:

| 항목 | 확인 결과 |
|---|---|
| Node.js | nvm으로 **24.22.1 LTS 설치·전환 완료**, 실행 확인 |
| npm / npx | **11.19.1** 실행 확인 |
| 기존 Node.js | **20.19.0 보존**, nvm 목록에서 확인 |
| Codex CLI | 이전 Node 20 환경에 `@openai/codex@0.164.0` 설치돼 있었음. 전환 후 사용자가 `npm install -g @openai/codex` 설치 완료를 알림. 새 설치 버전·실행은 미검증 |
| 기타 전역 도구 | Node 20의 Gemini CLI·pnpm은 Node 24로 자동 이전되지 않았음. 재설치 여부는 미확인 |
| Java / javac | **21.0.8** 실행 확인 |
| JAVA_HOME | `C:\Program Files\Java\jdk-17` — 실행 Java와 불일치. 수정 안 함 |
| JDK 폴더 | `C:\Program Files\Java\jdk-21` 존재 확인 (1.8·11·17도 있음) |
| Ghidra | 환경변수 미설정. 확인한 기본 위치에 설치 폴더를 찾지 못함. 전체 PC 미설치를 단정하지 않음 |
| REA | 점검 당시 전역 명령·Codex MCP 등록·확인한 기본 스킬 경로에 설치를 찾지 못함. 설치·실제 연결 검증은 미진행 |

- 현재 REA 공식 package.json에서 Node 요구사항은 `^22.23.4 || ^24.22.1 || >=26.12.1`로 확인했다. Node 24.22.1은 이를 충족한다. 다음 설치 때 최신 요구사항도 확인한다.
- sandbox 셸에서 npm이 `EPERM ... lstat C:\Users\kjk98`로 실패했지만, 승인된 실행에서는 정상 동작했다. 이를 npm 설치 손상으로 오인하지 않는다.
- Git 원격 main을 `68f6c8a` → `2706b6e`로 fast-forward pull했다. 그때 작업 트리는 깨끗했다. 이후 이 세션의 문서 수정이 있으므로 종료 시 변경이 없는 상태는 아니다. 커밋·푸시는 하지 않았다.
- 종료 시 `engine-tool/ghidra-Ghidra_12.2_build.zip`이 미추적 파일로 발견됐다. 이번 세션에서 다운로드한 파일이 아니며 출처·내용·설치용 배포본 여부는 미확인이다. 이름만 보고 설치 완료나 실행 가능한 Ghidra 배포본으로 판단하지 않는다. 삭제·압축 해제하지 않았다.

## 블로그에서 사용자가 전달한 내용

- 글: 멍개의 연구소, 「REA 에이전트 기반 리버스 엔지니어링」, 2026-10-13.
- 링크: https://m.blog.naver.com/pjt3591oo/224440156330
- 브라우징 도구로 본문 접근이 안 되어 사용자가 대화에 본문을 붙여 주었고 이를 읽었다. 다음 세션에 다시 본문을 요청할 필요 없다.
- 사례: REA MCP·스킬과 Ghidra로 Windows 그림판 기능을 분석한 뒤 JS·HTML Canvas로 자체 구현. 분석 근거(Evidence)와 구현 대응을 문서화했다. 블로그 사례를 우리 환경에서 재현·검증한 것은 아니다.
- 셋업 후 MCP·스킬이 실제 등록됐는지 확인하는 점이 중요하다. 글의 opencode·로컬 모델 환경은 작성자 환경이며 우리 환경의 필수 조건이 아니다.
- 네이티브 바이너리 분석은 Ghidra 등 분석기가 필요하고, 정적 JavaScript 분석은 Ghidra 없이 시작 가능하다. 바이너리 분석 준비 시 JAVA_HOME을 JDK 21 경로와 맞추고 Ghidra 배포본과 요구사항을 확인한다.

## 현재 자료와 미정 사항

- `analysis/rpgmaker/01~06`: RPG Maker 분석 문서가 이미 존재한다.
- `analysis/rpgmaker/07_기초뼈대_설계.md`: 종합 설계 **초안**, 횡스크롤 범위 추가 설계 필요. 사용자 최종 승인·구현 완료가 아니다.
- `refs/`: RPG Maker MV corescript·mkxp 참고 소스 보관.
- 다음 분석 대상은 Unity·Godot. REA 연결 후 작은 검증을 먼저 한다.
- 첫 구현 대상으로 에피소드 편집기(이미지 배치→대사 입력→재생)를 제안했으나 사용자가 확정한 순서는 아니다.
- 최종 기술·에디터 실행 형태·턴제/횡스크롤 모듈 구성·첫 구현 범위·전투 규칙·Android 내보내기는 미정이다.
- 게임 대본은 파트 1~11 승인 완료, CG는 파트 6까지 구성 완료이며 다음 게임 에셋은 파트 7이다. 이 작업과 개발툴의 다음 작업을 혼동하지 않는다.

## 관련 파일과 공식 출처

- [툴 개발 계획](ENGINE_TOOL_PLAN.md)
- [뼈대 설계 초안](analysis/rpgmaker/07_기초뼈대_설계.md)
- [REA 공식 저장소](https://github.com/morluto/rea)
- [REA 공식 패키지 요구사항](https://github.com/morluto/rea/blob/main/package.json)
