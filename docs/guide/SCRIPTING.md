# Python 스크립트 작성·연결 안내

현재 공개 기능 전체의 연결·에셋 ID·배경/객체 이미지·변수/DB·입력/이동·공통 이벤트·전역 훅·오류 예제는 [Python 전체 가이드](PYTHON_COOKBOOK.md)를 보세요. 완성 파일 [python_guide.py](../examples/python_guide.py)와 [guide_global.py](../examples/guide_global.py)를 함께 제공합니다. 이벤트 함수만 쓰면 전역 등록 불필요, 다른 파일 import를 쓰는 전역 예제는 python_guide → guide_global 순서로 등록합니다.

기준: 2026-11-08. 현재 구현된 기능으로 설명합니다. 전체 이름·기본값·오류 규칙은 [SCRIPT_API](../SCRIPT_API.md), 이벤트 입력값은 [이벤트 명령 안내](EVENTS.md), 기본 설정 코드/화면 동기화는 [키 설정 안내](INPUT_SETTINGS.md)에 있습니다.

게임 실행 중 `ctx.variables`의 값은 코드를 추가하지 않아도 **실행 변수** 패널에서 볼 수 있습니다. `ctx.log`와 예외의 traceback은 게임 파일 로그에도 기록합니다. Python 오류를 더블클릭하면 파일·행으로, 뒤따르는 **명령 실패**를 더블클릭하면 호출한 이벤트로 이동합니다. [패널·파일 로그·체력 출력 예제](DEBUGGING.md)를 보세요. Python 함수·인자·반환값과 호출 시점은 바뀌지 않았습니다.

## 1. 파일·함수·ctx의 기본

프로젝트의 `scripts/` 폴더에 `.py` 파일을 둡니다. 제작툴의 **스크립트 → 새 스크립트**로 만들거나 외부 편집기를 써도 됩니다. 파일을 열어 F7로 문법 검사, F5로 `run(ctx)`를 실행합니다. F5는 게임 월드 밖의 단독 실행입니다.

```python
def run(ctx):
    count = ctx.variables.get("visits", 0) + 1
    ctx.variables.set("visits", count)
    ctx.log(f"방문 횟수: {count}")
```

`def`는 함수를 정의합니다. `ctx`는 호출 시 엔진이 넘겨주는 연결 객체입니다. `ctx.variables.get(...)`는 그 연결 안의 변수 기능에서 값을 읽는 함수입니다. 점(`.`)은 그 안의 기능을 고르고, 괄호는 호출할 때 넘길 값을 적습니다. 문자열은 따옴표, 함수 안은 같은 깊이로 들여씁니다. Python의 참/거짓은 `True`/`False`이며 소문자 `true`/`false`나 ON/OFF라는 단어를 값으로 쓰지 않습니다.

함수를 정의하는 것만으로 임의 함수가 자동 실행되지는 않습니다. F5는 `run`, 이벤트는 입력한 함수, 전역 스크립트는 아래 정해진 훅을 부릅니다. 게임을 정지시키는 긴 반복이나 `time.sleep` 대신 이벤트 대기·이동 명령을 사용하세요. 대기형 `async`는 아직 없습니다.

## 2. 객체 이벤트에 Python 함수 연결

`scripts/reward.py`를 만듭니다. 이 예제는 프로젝트 전체에서 환영 보상을 한 번만 줍니다.

```python
def give(ctx):
    if ctx.variables.get("welcome_reward_taken", False):
        return False
    gold = ctx.variables.get("gold", 0)
    ctx.variables.set("gold", gold + 100)
    ctx.variables.set("welcome_reward_taken", True)
    ctx.log("환영 보상 100골드 지급")
    return True
```

1. 장면의 NPC/상자 객체 더블클릭 → **이벤트** 탭.
2. 실행 조건을 **조사**로 둡니다.
3. **명령 추가 → 스크립트 호출**, 파일 `reward.py`, 함수 `give`, 반환값 변수 `rewarded`를 지정합니다.
4. 뒤에 **조건 분기** `rewarded == 참`을 넣고, 그렇다면 ‘보상을 받았다’, 아니라면 ‘이미 받았다’ 대사를 넣습니다.
5. F6로 대사·로그를 확인하고, Ctrl+R로 게임에서 실제 조사합니다.

None·bool·정수·소수·글자는 반환값으로 사용할 수 있습니다. 목록/사전을 게임 변수에 반환하는 것은 지원하지 않습니다. 반환값 변수 빈칸이면 버립니다. 위 보상 상태는 실행 세션 동안만 유지되며 게임 세이브 기능은 아직 없습니다. NPC마다 따로 기록하려면 각각 다른 게임 변수 이름을 사용하세요. 셀프 스위치는 미구현입니다.

함수 이름을 `give`로 만들었으므로 이벤트에도 `give`라고 적어야 합니다. `give()`의 괄호는 입력하지 않습니다. `run`으로 만들었다면 기본 함수 이름 `run`을 사용합니다.

## 3. 게임 전체에서 실행하는 전역 스크립트

**스크립트 → 전역 스크립트...**에서 파일을 등록하고 순서를 정합니다. 파일만 만드는 것으로 등록되지는 않습니다. 새 프로젝트의 `game.py`는 이미 등록되어 있습니다. 전역 스크립트에서 자동 호출하는 이름은 다음 네 개뿐입니다.

| 함수 | 호출 시점 | 사용할 수 있는 게임 연결 |
|---|---|---|
| `on_game_start(ctx)` | 게임 시작 시 한 번 | 변수/DB/입력/소리. 아직 장면·플레이어는 None |
| `on_scene_enter(ctx)` | 장면에 들어온 뒤 | 장면 있음, 플레이어는 지정됐을 때만 있음 |
| `on_scene_exit(ctx)` | 이전 장면을 떠나기 전 | 이전 장면·플레이어 |
| `on_update(ctx)` | 고정 틱, 기본 초당 60번 | 현재 장면/입력/플레이어. 플레이어가 없을 수도 있음 |

```python
def on_scene_enter(ctx):
    ctx.log(f"현재 장면: {ctx.scene.name}")

def on_update(ctx):
    if ctx.player and not ctx.busy and ctx.input.pressed("jump"):
        ctx.log("jump 동작을 눌렀습니다.")
```

이 예제는 **입력을 확인해 로그만** 남깁니다. 점프 물리는 아직 없습니다. `jump` 키 연결은 게임 → 키 설정이나 전용 설정 파일에서 추가합니다.

전역 스크립트 모듈은 게임 한 번 동안 유지됩니다. 이벤트에서 등록된 파일의 함수를 부르면 같은 모듈을 사용합니다. `_`로 시작하지 않는 파일 최상위 변수 이름이 등록 파일 사이에 겹치면 검사에서 오류를 알립니다. 새 전역 변수는 파일별 접두사를 쓰거나 내부 변수에 `_`를 붙이세요. Python 표준 모듈과 같은 파일 이름(예: `random.py`)은 피합니다.

오류가 난 훅은 해당 스크립트에서 그 실행 동안 중지하고 다른 스크립트는 계속할 수 있습니다. 게임 재실행은 파일을 다시 읽습니다. 실행 중 소스를 편집했다고 즉시 실행 코드가 교체되는 것은 아닙니다.

## 4. 게임 변수와 Python 변수

게임 → 변수·스위치에서 이름·형식·처음 값·설명을 정의합니다. F5/F6/게임은 각각 처음 값에서 시작합니다.

```python
def run(ctx):
    ctx.variables.set("door_open", True)
    current = ctx.variables.get("gold", 0)
    if current >= 10:
        ctx.variables.set("gold", current - 10)
```

`ctx.variables`는 이벤트와 공유하는 게임 변수입니다. 함수 안의 `current`는 Python 지역 변수이며 자동으로 이벤트 변수나 세이브 데이터가 되지 않습니다. 전역 Python 변수도 자동 세이브 대상이 아닙니다. 이벤트에서는 목록에 없는 게임 변수 이름도 쓸 수 있습니다. 지원 값은 None·bool·정수·소수·글자입니다.

## 5. 데이터베이스 읽기

```python
def run(ctx):
    item = ctx.db.get("item", "potion")
    if item is None:
        ctx.warning("포션 항목이 없습니다.")
        return
    ctx.variables.set("heal_amount", item["heal"])
    ctx.log(item["name"])
```

첫 값은 종류 **키**, 다음 값은 항목 **ID**입니다. 표시 이름과 다릅니다. 항목 없음은 None, 종류 없음은 KeyError입니다. `ctx.db.types()`로 종류, `ctx.db.ids("item")`으로 항목 ID를 볼 수 있습니다. DB는 읽기 전용이며 반환한 사전을 수정해도 DB가 수정되는 API가 아닙니다.

[에셋 추가·파일 교체·파일 내보내기·그룹 분류](ASSETS.md)는 제작툴에서 사용할 수 있습니다. 그룹은 제작 자료 분류이며 게임 실행에 영향을 주지 않습니다. [객체 이미지·장면 배경](IMAGES.md)은 인스펙터에서 지정하고 실제 게임에서는 `ctx.scene.object(ID).image`와 `ctx.scene.background_image`에 등록한 이미지 ID 또는 빈 문자열을 넣어 변경합니다. 잘못된 ID·다른 종류는 ValueError, 문자열 이외는 TypeError이며 기존 값을 유지합니다. 실행 사본에 즉시 반영·대기 없음, 장면 재진입·재시작은 편집값으로 초기화합니다. `docs/examples/images/scripts/appearance.py`를 프로젝트 scripts에 두고 이벤트 스크립트 호출에 toggle_background를 연결하면 됩니다. 인자 ctx·반환 bool, 전역 등록 불필요. F5/F6의 ctx.scene은 None이므로 예제는 안내 로그만 남기며 오류 위치·traceback은 기존과 같습니다. `ctx.assets` 같은 관리 API·애니메이션 API는 아직 없습니다. 게임 소리는 [ctx.audio 안내](AUDIO.md)대로 연결합니다.

DB에 스킬을 정의해서 Python 피해 계산·이미지/소리/이펙트로 연결하는 기능은 개발 계획입니다. 현재 스킬/전투 API가 있다고 가정하지 마세요.

## 6. 게임 중 객체·입력·장면 조작

에셋을 추가할 때 **에셋 ID** 칸에 `day`·`night` 같은 이름을 직접 입력할 수 있습니다. 기존 에셋은 목록에서 **ID 변경...**을 사용합니다. 객체/배경 연결은 자동 갱신되지만 스크립트의 ID 문자열은 직접 수정해야 합니다. 예를 들어 ID가 `sky_day`라면 `ctx.scene.background_image == "sky_day"`처럼 비교와 대입 모두 맞춥니다. 자세한 규칙은 [ID 지정·변경](ASSETS.md#id-직접-지정변경)을 보세요.

```python
def open_gate(ctx):
    if ctx.scene is None:
        ctx.log("이 함수는 실제 게임에서 객체를 바꿉니다.")
        return
    gate = ctx.scene.object("obj_002")
    if gate is None:
        ctx.warning("대상 객체가 없습니다.")
        return
    gate.passable = True
    gate.visible = False
```

객체 ID는 인스펙터에서 확인해 바꿔 넣습니다. 없는 ID는 None이므로 위처럼 확인합니다. `ctx.scene.find("이름")`, `ctx.scene.objects()`로 이름 검색/목록을 얻을 수 있습니다. 위치 x/y 직접 대입은 충돌 없이 옮기고, `move(dx, dy)`는 충돌을 검사합니다. 보이기·통과·색도 변경할 수 있습니다. 크기 변경·객체 생성/삭제·이미지 애니메이션은 아직 API가 없습니다.

`ctx.input.held("right")`는 누르는 동안, `pressed("right")`는 새로 누른 첫 틱에 참입니다. 키 이름이 아니라 **동작 이름**을 묻습니다. `ctx.player.default_control = False`는 실행 중 기본 이동/조사를 끄고, `ctx.player.move(...)`·`interact()`로 직접 구현할 수 있습니다. `ctx.busy`가 참이면 대화/막는 이벤트 중이므로 사용자 이동 코드에서도 처리할지 결정하세요.

`ctx.scene.change("장면ID", x, y, "up")`는 이번 틱 끝에 장면을 바꾸며 그 장면의 이벤트는 중지합니다. 생략 가능한 인자와 방향 규칙은 [API](../SCRIPT_API.md)를 따릅니다.

F5/F6에서는 `ctx.scene/input/player`가 None이므로 위처럼 확인해야 합니다. 객체 이벤트의 F6가 실제 객체를 움직이는 게임 미리보기는 아닙니다.

## 7. 공통 이벤트 호출

공통 이벤트 탭에서 이벤트를 만들고 표시된 ID로 예약합니다.

```python
def recover(ctx):
    ctx.run_event("recover")
    ctx.log("여기까지 Python 함수가 먼저 끝납니다.")
```

함수가 반환되면 예약된 공통 이벤트를 순서대로 실행합니다. 같은 함수 다음 줄에서 이미 회복됐다고 가정하지 마세요. 이벤트의 스크립트 호출 다음 명령은 공통 이벤트가 끝나야 진행합니다. F6에서도 호출할 수 있고 F8은 안쪽까지 중지합니다. F5 단독 실행은 사용할 수 없습니다. 순환·호출 깊이·예약 개수 제한은 [API](../SCRIPT_API.md)에 있습니다.

## 8. 오류와 다음 실행

F7은 문법, F5는 단독 `run`, F6는 이벤트 연결, Ctrl+R은 실제 게임 흐름을 확인합니다. 전용 입력 설정 파일은 F5로 일반 함수를 실행하는 대신 설정을 검사·저장합니다.

`ctx.log`, `ctx.warning`, `print`는 로그로 확인합니다. Python 오류는 파일·행·traceback을 기록하며 로그의 파일 위치에서 코드를 열 수 있습니다. 실제 게임의 명령 실패 로그를 더블클릭하면 객체/공통 이벤트의 오류 명령으로 이동할 수 있습니다. 오류가 나면 다음 명령이 실행되지 않았는지와 남아 있는 조작/이동도 확인하세요.

새 기능마다 이 안내·이벤트 문서·API·예제를 함께 갱신합니다. 복사 가능한 예제는 [예제 목록](../examples/README.md), 현재 미구현 범위는 [로드맵](../ROADMAP.md)에 있습니다.


게임 소리는 [audio_demo.py](../examples/audio/scripts/audio_demo.py)를 scripts/에 복사하고 이벤트 스크립트 호출의 run/fade/stop_effects로 연결합니다. 전역 등록은 필요 없고 반환값은 None이며 대기하지 않습니다. F5/F6에서는 ctx.audio가 None이므로 보호 코드가 필요합니다. 인자·범위·오류와 on_game_start 연결은 [소리 안내](AUDIO.md)를 보세요.


이름표는 [nameplates.py](../examples/nameplates/scripts/nameplates.py)의 run/hide_name/use_object_name을 이벤트 스크립트 호출로 연결합니다. 인자 ctx·반환 None·전역 등록 불필요입니다. 객체 show_name(bool)과 name_text(str, 빈 글자면 객체 이름)를 읽고 바꾸며 F5/F6은 ctx.scene=None 보호가 필요합니다. [설정·오류·실행 시점·완성 코드·그림](NAMEPLATES.md).


이름표 배경 투명/색/위아래는 SceneObject.name_background_transparent/name_background_color/name_position으로 바꿉니다. [스타일 완성 코드와 연결](../examples/nameplate-style/README.md), [기본값·오류·실행 시점·그림](NAMEPLATES.md).
