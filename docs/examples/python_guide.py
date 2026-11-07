"""현재 공개 Python API 예제. 사용법: docs/guide/PYTHON_COOKBOOK.md.

프로젝트 scripts/에 복사하고 아래 GUIDE_* 값을 실제 ID로 바꿉니다.
이벤트의 스크립트 호출에는 함수 이름만 입력합니다. 인자는 엔진이 ctx로 전달합니다.
이 파일 자체는 전역 등록이 필요 없습니다. F5는 안전한 안내 함수 run만 실행합니다.
"""

GUIDE_DAY_IMAGE = "day"
GUIDE_NIGHT_IMAGE = "night"
GUIDE_OBJECT_ID = "tree"              # 장면 객체 ID
GUIDE_OBJECT_NAME = "나무"            # 표시 이름으로 찾기 예제
GUIDE_OBJECT_IMAGE = "tree"           # 이미지 에셋 ID: 객체 ID와 별개
GUIDE_PLAYER_IMAGE = "hero"
GUIDE_NEXT_SCENE = "scene_002"
GUIDE_COMMON_EVENT = "recover"
GUIDE_DB_TYPE = "item"
GUIDE_DB_RECORD = "potion"


def _scene(ctx):
    if ctx.scene is None:
        ctx.log("이 기능은 Ctrl+R 실제 게임에서 확인하세요. F5/F6에는 장면이 없습니다.")
        return None
    return ctx.scene


def _object(ctx):
    scene = _scene(ctx)
    if scene is None:
        return None
    obj = scene.object(GUIDE_OBJECT_ID)
    if obj is None:
        ctx.warning("대상 객체 ID를 확인하세요: " + GUIDE_OBJECT_ID)
    return obj


def _player(ctx):
    if ctx.player is None:
        ctx.log("실제 게임의 현재 장면에 플레이어 객체가 있어야 합니다.")
        return None
    return ctx.player


def run(ctx):
    ctx.log("python_guide.py: 이벤트의 스크립트 호출에서 원하는 함수 이름을 지정하세요.")
    ctx.log("예: set_night / toggle_background / change_object_image / variables_demo")
    return True


def variables_demo(ctx):
    count = ctx.variables.get("guide_count", 0)
    ctx.variables.set("guide_count", count + 1)
    ctx.variables.set("guide_ratio", 0.5)
    ctx.variables.set("guide_message", "안녕하세요")
    ctx.variables.set("guide_empty", None)
    if ctx.variables.get("guide_switch", False):
        ctx.variables.set("guide_switch", False)
    else:
        ctx.variables.set("guide_switch", True)
    ctx.log("guide_count = " + str(count + 1))
    return count + 1


def inspect_database(ctx):
    for type_key in ctx.db.types():
        for record_id in ctx.db.ids(type_key):
            record = ctx.db.get(type_key, record_id)
            ctx.log(str(record))
    return True


def read_item_price(ctx):
    if GUIDE_DB_TYPE not in ctx.db.types():
        ctx.warning("DB 종류 키가 없습니다: " + GUIDE_DB_TYPE)
        return 0
    item = ctx.db.get(GUIDE_DB_TYPE, GUIDE_DB_RECORD)
    if item is None:
        ctx.warning("DB 항목 ID가 없습니다: " + GUIDE_DB_RECORD)
        return 0
    ctx.variables.set("guide_item_name", item["name"])
    price = item.get("price", 0)
    ctx.variables.set("guide_item_price", price)
    # item은 읽어 온 사본입니다. item["price"] = 0으로 바꿔도 DB를 수정하지 않습니다.
    return price


def set_day(ctx):
    scene = _scene(ctx)
    if scene is None:
        return False
    scene.background_image = GUIDE_DAY_IMAGE
    return True


def set_night(ctx):
    scene = _scene(ctx)
    if scene is None:
        return False
    scene.background_image = GUIDE_NIGHT_IMAGE
    return True


def toggle_background(ctx):
    scene = _scene(ctx)
    if scene is None:
        return False
    if scene.background_image == GUIDE_DAY_IMAGE:
        scene.background_image = GUIDE_NIGHT_IMAGE
    else:
        scene.background_image = GUIDE_DAY_IMAGE
    return True


def clear_background(ctx):
    scene = _scene(ctx)
    if scene is None:
        return False
    scene.background_image = ""  # 장면의 배경색만 표시
    return True


def change_object_image(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    obj.image = GUIDE_OBJECT_IMAGE
    return True


def clear_object_image(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    obj.image = ""  # 객체 색 사각형으로 복귀. 충돌과 이벤트는 유지
    return True


def change_player_image(ctx):
    scene = _scene(ctx)
    player = _player(ctx)
    if scene is None or player is None:
        return False
    # Player에는 image 속성이 없습니다. 같은 객체를 SceneObject로 얻어서 바꿉니다.
    obj = scene.object(player.id)
    if obj is None:
        return False
    obj.image = GUIDE_PLAYER_IMAGE
    return True


def hide_object(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    obj.visible = False
    return True


def show_object(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    obj.visible = True
    return True


def toggle_passable(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    if obj.passable:
        obj.passable = False
    else:
        obj.passable = True
    return obj.passable


def change_object_color(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    obj.color = "#ff8800"  # 이미지가 없을 때 보이는 색
    return True


def move_object(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    return obj.move(48, 0)  # 충돌 검사, 즉시 이동. 성공은 실제 이동 여부


def teleport_object(ctx):
    obj = _object(ctx)
    if obj is None:
        return False
    obj.x = 128  # 충돌을 검사하지 않는 직접 대입
    obj.y = 128
    return True


def inspect_scene(ctx):
    scene = _scene(ctx)
    if scene is None:
        return False
    ctx.log(f"장면 {scene.id}: {scene.name}, {scene.width} x {scene.height}")
    ctx.log("장면 ID 목록: " + str(scene.ids()))
    named = scene.find(GUIDE_OBJECT_NAME)
    if named is not None:
        ctx.log("이름으로 찾은 객체 ID: " + named.id)
    for obj in scene.objects():
        ctx.log(f"객체 {obj.id}: {obj.name}, ({obj.x}, {obj.y}), {obj.width} x {obj.height}")
        ctx.log(f"플레이어={obj.is_player}, 통과={obj.passable}, 보임={obj.visible}, 색={obj.color}, 이미지={obj.image}")
    return True


def inspect_player(ctx):
    player = _player(ctx)
    if player is None:
        return False
    ctx.log(f"플레이어 {player.id}: {player.name}, ({player.x}, {player.y}), {player.width} x {player.height}")
    ctx.log(f"속도={player.speed}, 방향={player.facing}, 기본 조작={player.default_control}")
    return True


def move_player(ctx):
    player = _player(ctx)
    if player is None:
        return False
    return player.move(48, 0)


def teleport_player(ctx):
    player = _player(ctx)
    if player is None:
        return False
    player.x = 64
    player.y = 64
    return True


def face_right(ctx):
    player = _player(ctx)
    if player is None:
        return False
    player.facing = "right"  # up / down / left / right
    return True


def set_player_speed(ctx):
    player = _player(ctx)
    if player is None:
        return False
    player.speed = 360.0
    return True


def disable_default_control(ctx):
    player = _player(ctx)
    if player is None:
        return False
    player.default_control = False
    return True


def enable_default_control(ctx):
    player = _player(ctx)
    if player is None:
        return False
    player.default_control = True
    return True


def interact(ctx):
    player = _player(ctx)
    if player is None:
        return False
    return player.interact()  # 이벤트·대사 중에는 False


def inspect_input(ctx):
    if ctx.input is None:
        ctx.log("입력은 Ctrl+R 실제 게임에서 확인하세요.")
        return False
    ctx.log("동작 목록: " + str(ctx.input.actions()))
    ctx.log("확인 키: " + str(ctx.input.keys("confirm")))
    ctx.log("오른쪽 유지: " + str(ctx.input.held("right")))
    ctx.log("오른쪽 새 누름: " + str(ctx.input.pressed("right")))
    ctx.log("대사/막는 이벤트 중: " + str(ctx.busy))
    return True


def bind_runtime_keys(ctx):
    if ctx.input is None:
        return False
    ctx.input.bind("guide_special", ["Shift + E", "F"])
    ctx.log("guide_special: " + str(ctx.input.keys("guide_special")))
    return True


def disable_runtime_action(ctx):
    if ctx.input is None:
        return False
    ctx.input.bind("guide_special", [])
    return True


def queue_common_event(ctx):
    try:
        ctx.run_event(GUIDE_COMMON_EVENT)
    except (KeyError, RuntimeError) as error:
        ctx.warning(str(error))
        return False
    ctx.log("공통 이벤트 예약 완료. 이 함수가 반환된 뒤 실행됩니다.")
    return True


def change_scene(ctx):
    scene = _scene(ctx)
    if scene is None:
        return False
    if GUIDE_NEXT_SCENE not in scene.ids():
        ctx.warning("대상 장면을 먼저 만드세요: " + GUIDE_NEXT_SCENE)
        return False
    scene.change(GUIDE_NEXT_SCENE, 64, 96, "up")
    return True  # 장면 변경은 이번 틱 끝. 이 다음 줄에서 새 장면 객체를 쓰지 않습니다.


def log_examples(ctx):
    import sys
    ctx.log("정보 로그")
    ctx.warning("경고 로그")
    print("print도 정보 로그로 기록됩니다.")
    print("이 줄은 stderr의 오류 수준 로그 예제입니다. 예외를 발생시키지는 않습니다.", file=sys.stderr)
    return True


def error_example(ctx):
    # 오류 위치·traceback을 확인하려고 직접 선택한 경우에만 실행하세요.
    raise ValueError("가이드의 의도적인 오류입니다. 파일·행·호출 위치를 확인하세요.")
