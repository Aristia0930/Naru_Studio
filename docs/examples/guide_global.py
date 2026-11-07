"""python_guide.py와 함께 scripts/에 복사합니다.
전역 스크립트에 python_guide.py를 먼저, guide_global.py를 다음 순서로 등록합니다.
현재 import는 먼저 등록·로드된 전역 모듈을 사용합니다.

게임 → 키 설정에 guide_toggle_background (G), guide_move_up (8),
선택 사항 guide_dash (Shift)를 연결합니다. 지속 설정은 키 설정 화면에서 관리합니다.
"""
GUIDE_GLOBAL_ENABLE_DASH = False
_scene_visits = 0
_ticks = 0
_base_speed = 240.0


def on_game_start(ctx):
    global _scene_visits, _ticks
    _scene_visits = 0
    _ticks = 0
    ctx.variables.set("guide_ticks", 0)
    ctx.log("게임 시작: 아직 ctx.scene과 ctx.player는 None입니다.")
    if "guide_toggle_background" not in ctx.input.actions():
        ctx.warning("게임 → 키 설정에서 guide_toggle_background에 G를 연결하세요.")


def on_scene_enter(ctx):
    global _scene_visits, _base_speed
    _scene_visits += 1
    ctx.variables.set("guide_scene_visits", _scene_visits)
    if ctx.player is not None:
        _base_speed = ctx.player.speed
    ctx.log("장면 들어옴: " + ctx.scene.name)
    # 입장마다 밤 배경을 원하면 다음 줄의 #을 지우세요. night 에셋이 필요합니다.
    # import python_guide as _guide
    # _guide.set_night(ctx)


def on_update(ctx):
    global _ticks
    _ticks += 1  # 현재 엔진은 1/60초 고정 틱. time.sleep으로 멈추지 않습니다.
    ctx.variables.set("guide_ticks", _ticks)
    if ctx.scene is None or ctx.input is None or ctx.busy:
        return
    import python_guide as _guide  # 먼저 등록된 전역 모듈. F5 안내 실행에서는 import하지 않음
    if ctx.input.pressed("guide_toggle_background"):
        _guide.toggle_background(ctx)
    if ctx.player is None:
        return
    if ctx.input.pressed("guide_move_up"):
        ctx.player.move(0, -48)
        _guide.face_right(ctx)
    if GUIDE_GLOBAL_ENABLE_DASH:
        if ctx.input.held("guide_dash"):
            ctx.player.speed = _base_speed * 1.75
        else:
            ctx.player.speed = _base_speed


def on_scene_exit(ctx):
    if GUIDE_GLOBAL_ENABLE_DASH and ctx.player is not None:
        ctx.player.speed = _base_speed
    ctx.log("장면 나감: " + ctx.scene.name)
    # 나가는 훅에서 공통 이벤트를 예약하면 취소됩니다. 입장 훅에서 예약하세요.


def run(ctx):
    ctx.log("전역 스크립트에 python_guide.py → guide_global.py 순서로 등록한 뒤 Ctrl+R로 실행하세요.")
