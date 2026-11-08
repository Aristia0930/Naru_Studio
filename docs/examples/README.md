# 스크립트 예제

**전체 연결 방법:** [Python 전체 가이드](../guide/PYTHON_COOKBOOK.md). [python_guide.py](python_guide.py)는 현재 공개 기능별 이벤트 함수, [guide_global.py](guide_global.py)는 전역 훅·키 입력·모듈 공유 예제입니다. 두 파일을 scripts에 복사하며, 이벤트 함수만 쓰면 전역 등록 불필요, 전역 예제는 python_guide → guide_global 순서로 둘 다 등록합니다.

[객체 이미지·장면 배경 예제](images/README.md)는 그림과 충돌 영역 분리, 배경 다섯 맞춤 방식과 이벤트의 Python 호출로 낮/밤 변경을 확인합니다.

[공통 이벤트 예제 프로젝트](common-events/README.md)는 같은 회복 처리를 이벤트 명령과 Python `ctx.run_event()`로 각각 부릅니다.

코딩 없이 실행하는 [선택지 예제 프로젝트](choices/README.md)도 있습니다. 폴더를 복사해 Naru Studio에서 열면 선택지·중첩 분기를 바로 확인할 수 있습니다.

복사해서 바로 쓸 수 있는 예제입니다. 프로젝트의 `scripts/` 폴더에 넣고, 파일 맨 위 주석의 "쓰는 법"대로 연결하세요.
각 파일의 `obj_002`는 새 프로젝트의 "상자"입니다. 쓰려는 객체를 선택하면 인스펙터 맨 위에 보이는 **ID**로 바꾸세요.
문법 설명은 [사용 설명서 6장](../guide/README.md#6-python-스크립트), 쓸 수 있는 값 전체는 [SCRIPT_API.md](../SCRIPT_API.md)에 있습니다.

| 파일 | 하는 일 | 연결 방법 |
|---|---|---|
| [python_guide.py](python_guide.py) | 배경·객체·플레이어 이미지, 변수/스위치·DB·이동·장면·입력·공통 이벤트·로그/오류 전체 | GUIDE_* ID를 맞추고 이벤트의 스크립트 호출에서 원하는 함수 이름 지정. F5는 안내 run |
| [guide_global.py](guide_global.py) | 네 전역 훅·G 배경 전환·8 이동·선택 달리기·import | python_guide 먼저, guide_global 다음 순서로 등록. 게임 → 키 설정에서 guide_toggle_background→G, guide_move_up→8 |
| [key_combinations.py](key_combinations.py) | Shift+E 동시 입력 또는 F로 한 번씩 로그 실행 | `special_action`에 `Shift + E`와 `F` 배치 후 전역 등록. [동시 입력 안내](../guide/INPUT_SETTINGS.md#6-여러-키를-동시에-눌러-실행하기) |
| [key_move_face.py](key_move_face.py) | 8을 새로 누르면 위로 48px 즉시 이동한 뒤 오른쪽 보기 | 키 설정에서 `step_up_face_right`→`8` 추가 후 전역 등록. [연결 순서](../guide/INPUT_SETTINGS.md#5-8을-눌러-위로-이동한-뒤-오른쪽-보기) |
| [reward.py](reward.py) | 환영 보상을 게임 변수로 한 번만 지급하고 참/거짓 반환 | 객체 이벤트: 스크립트 호출 `reward.py · give` → 반환값 변수 `rewarded`, [연결 설명](../guide/SCRIPTING.md#2-객체-이벤트에-python-함수-연결) |
| [chase.py](chase.py) | 객체가 플레이어 쪽으로 다가온다 (실행 조건 "이벤트 접촉"과 함께 쓰면 붙잡힐 때 이벤트 실행) | 전역 스크립트로 사용 |
| [patrol.py](patrol.py) | 객체가 두 지점 사이를 왔다 갔다 한다 | 전역 스크립트로 사용 |
| [push.py](push.py) | 부딪힌 상자를 바라보는 방향으로 한 칸 민다, 움직였는지 반환 | 상자 이벤트: "플레이어 접촉" + 스크립트 호출 `push.py · push` |
| [door.py](door.py) | 조사하면 문이 열린다 (통과 가능 + 색 변경), 장면에 다시 들어와도 열린 채로 | 문 이벤트: "조사" + 스크립트 호출 `door.py · open_door` (+ 전역 스크립트로도 등록하면 유지) |

- 2026-10-27에 새 프로젝트에서 실제로 실행해 확인: 순찰(640→800→돌아옴), 밀기(부딪힐 때마다 48px씩), 따라오기(주인공 옆에서 멈추고 이벤트 접촉 실행). 문 열기는 같은 기능을 자동 테스트로 확인.
- 한 객체에 따라오기와 순찰을 함께 쓰면 서로 반대로 움직입니다. 객체마다 하나만 쓰세요.
- 따라오기·순찰 등의 전역 예제는 변수 앞에 `CHASE_`, `PATROL_` 등을 붙여 함께 등록할 수 있습니다. 파일마다 위 표의 연결 방법을 따르세요. `input_settings.py`는 전역 등록 대상이 아니며 `guide_global.py`는 `python_guide.py`를 먼저 등록해야 합니다.
- 바뀐 위치·색은 게임 실행 중에만 유지됩니다. 오래 남아야 하는 상태는 게임 변수(`ctx.variables`)에 기록하세요.

## 에셋 가져오기·목록

[assets](assets/README.md): 자체 생성한 반투명 PNG·짧은 WAV의 목록·미리보기·재가져오기·복구 예제입니다. 객체 이미지와 게임 소리 재생은 아래의 이미지·소리 프로젝트에서 각각 확인합니다.


- [BGM·효과음 WAV/MP3 예제](audio/README.md): 소리 이벤트·Python 재생/페이드/정지와 생성 도구.


- [객체 이름표·전체 이름/ID 예제](nameplates/README.md): 게임용 개별 표시, F9 디버그 표시, Python 문구 변경.


- [이름표 배경·위아래 예제](nameplate-style/README.md): 투명 위쪽·색상 아래쪽 비교, Python 스타일 전환.
