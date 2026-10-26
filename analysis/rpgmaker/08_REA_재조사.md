# RPG Maker 재조사 — REA 근거와 툴 기초 설계

2026-10-26. RPG Maker의 기능·제작 흐름을 기본으로 하고 Unity·Godot 장점을 통합한다는 사용자 방향을 반영한다. 이전 01~07은 과거 분석/초안이며 이번에 전부 REA로 검증한 것은 아니다.

## 직접 분석한 범위

공개 MV corescript 커밋 `182e31449707ba7e406db0485c44c2a9d11e2dcd`의 보관 사본에서 다음 4개 파일을 REA `analyze_javascript_application`으로 정적 분석했다.

- `js/rpg_managers/DataManager.js`
- `js/rpg_managers/BattleManager.js`
- `js/rpg_objects/Game_Event.js`
- `js/rpg_objects/Game_Interpreter.js`

[요청](rea-foundation-request.json), [요약](rea-foundation-summary.json), [전체 Evidence](rea-foundation-evidence-20261026.json)를 저장했다. 소스 원본은 실행하지 않았다.

- 부모 Evidence `ev_2e0e5db07912ba97b9d5c942b30b68d671d24a4d3949e685950b55c46f9c535b`
- 요약 Evidence `ev_5771da6126b888c7e24f045362249816be5ba203a40c98d79990b0537e72f35c`
- 파일 4개, 파싱 실패 0, AST 15,191개, 읽은 텍스트 100,534바이트.
- 인벤토리 187개 중 183개 파일은 선택 범위 밖이다. 동적 속성 호출과 외부 파일을 지나는 관계는 미해결이며 전체 그래프 coverage는 partial이다.
- 애플리케이션 unknown 183개, semantic unknown 5,917개. 내보낸 번들의 최상위 unknowns 0은 이 분석 내부 unknown이 없다는 뜻이 아니다.

## 소스와 대조한 결과

다음 해석은 REA 결과와 실제 원문을 대조한 분석자 해석이다. [파일 해시·행·발췌](../foundation/source-observations.json)를 보관했다. 행은 보관 중인 MV 버전 기준이다.

| 위치 | 관측 | 개선 설계 |
|---|---|---|
| DataManager:45,62 | 데이터 종류와 JSON 파일 목록을 순회해 로드 | 초보자는 종류별 탭/목록으로 입력하고 내부 참조 검증은 공통 처리 |
| Game_Interpreter:75,116 | 자식 이벤트·대기 중에는 루프를 중단, 대사·이동·영상 등 대기 모드 구분 | 명령 목록 UI 유지, 대기·완료·취소를 명시적인 실행 상태로 만들기 |
| Game_Interpreter:168 | `command`와 숫자 코드를 합쳐 동적으로 함수 조회 | 타입 이름·입력 스키마·실행 함수를 등록해 폼 자동 생성; REA 정적 호출 그래프가 모두 해결됐다고 해석하지 않기 |
| Game_Event:188,199 | 마지막 페이지부터 조건을 검사, 스위치·변수·소지품·동료 등의 조건 판정 | 조건별 상태를 화면에 보여주고 어느 페이지가 선택됐는지 미리보기 |
| BattleManager:114,284 | phase별 업데이트·행동 순서·턴 종료, 로그 창/화면 갱신 직접 참조 | 계산 로직과 표시를 분리하고 '한 턴 실행'·재현 시드·상태 조회 제공 |
| DataManager:431 | system/screen/timer/switches/variables/actors/party/map/player 등의 상태 객체를 세이브 contents에 넣음 | 정의 데이터와 실행 상태 분리, 세이브 버전·이벤트 재개 정책 추가 |

**과거 초안 보정:** MV 세이브를 'ID만 저장'한다고 일반화할 수 없다. 이 함수는 여러 게임 상태 객체를 저장 대상으로 모은다. 우리 세이브에서 정의 ID와 변경된 상태를 저장하자는 것은 개선 제안이며 MV 구현을 그대로 설명한 것이 아니다. 또한 MV BattleManager가 이미 화면과 완전히 분리됐다는 해석도 피한다.

## MZ 공식 기능 비교

[Database 공식 도움말](https://rpgmakerofficial.com/product/MZ_help-en/01_08.html)은 캐릭터·직업·스킬·아이템·무기·방어구·적·적 편성·상태 등 종류별 데이터 편집을 설명한다. [Events 공식 도움말](https://rpgmakerofficial.com/product/MZ_help-en/01_09.html)은 맵·전투·공통 이벤트를 구분한다. MZ 에디터와 런타임을 이번에 내려받거나 직접 실행한 것은 아니며 MV 관측을 MZ 내부 구현으로 단정하지 않는다.

## 제작툴에 반영할 기준

RPG Maker의 데이터베이스·맵·이벤트·전투·사운드·연출·메뉴·세이브·테스트를 기능 비교의 기준으로 삼는다. 장비·상점·자동 조우·타일맵을 과거 07 초안의 제외 목록만으로 최종 제거하지 않는다. UI에 쉽게 노출할 수 있는 Unity·Godot의 장면 재사용·타임라인·카메라·충돌 편집·레이아웃 등도 후보로 추가한다.

기능 상태는 '검토 후보 / 설계 / 구현 / 검증'을 나누고, 소스 조사·공식 문서·실행 관측·우리 설계 제안을 구분한다. 다음 작업 기준은 [TOOL_FOUNDATION.md](../../TOOL_FOUNDATION.md)다.
