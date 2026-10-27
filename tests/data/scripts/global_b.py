import global_a


def on_scene_enter(ctx):
    ctx.variables.set("entered", ctx.scene.name)


def read_a(ctx):
    # 다른 전역 스크립트의 전역 변수를 import로 읽는다
    return global_a.counter
