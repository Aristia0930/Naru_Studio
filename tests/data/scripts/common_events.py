def run(ctx):
    ctx.run_event("recover")
    # run_event는 예약한다. 함수 안의 다음 줄은 먼저 실행된다.
    ctx.variables.set("script_finished", True)
    return 7


def missing(ctx):
    ctx.run_event("missing")


def recursive(ctx):
    ctx.run_event("recursive")


def fails(ctx):
    ctx.run_event("recover")
    raise ValueError("예약 후 실패")


def on_game_start(ctx):
    ctx.run_event("recover")


def on_scene_enter(ctx):
    if ctx.scene.id == "scene_002":
        ctx.run_event("recover")
