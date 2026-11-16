def on_game_start(ctx):
    ctx.input.bind("animation_attack", ["X"])
    ctx.input.bind("animation_cast", ["C"])
    ctx.input.bind("animation_stop", ["V"])
    ctx.input.bind("animation_repeat", ["B"])


def on_update(ctx):
    if ctx.scene is None:
        return
    hero = ctx.scene.object("hero")
    if ctx.input.pressed("animation_attack"):
        hero.play("attack", lock=True)
        ctx.log("공격 재생: 이전 동작을 교체했습니다.")
    if ctx.input.pressed("animation_cast"):
        hero.play("cast", lock=True)
        ctx.log("시전 재생: 이전 동작을 교체했습니다.")
    if ctx.input.pressed("animation_repeat"):
        hero.play("victory", loop=True, lock=True)
        ctx.log("반복 동작 시작: 이동이 잠겼습니다. V 또는 F8로 중지하세요.")
    if ctx.input.pressed("animation_stop"):
        active = hero.is_animating()
        hero.stop_animation()
        ctx.log("V 중지: " + ("재생을 중지하고 이동 잠금을 해제했습니다." if active else "중지할 명시적 동작이 없습니다."))
    for signal in ctx.scene.animation_signals():
        ctx.log("프레임 신호: " + signal["object"] + "/" + signal["clip"] + "/" + signal["signal"])
        ctx.variables.set("last_animation_signal", signal["signal"])


def run(ctx):
    if ctx.scene is None:
        ctx.log("Ctrl+R로 실행하세요. 방향키 이동, X 공격, C 시전, B 반복, V 중지입니다.")
        return
    hero = ctx.scene.object("hero")
    return hero.play("attack", lock=True)
