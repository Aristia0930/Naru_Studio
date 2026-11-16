effect_token = None

def on_game_start(ctx):
    ctx.input.bind("effect_play", ["E"])
    ctx.input.bind("effect_stop", ["R"])

def on_scene_enter(ctx):
    if ctx.scene.id == "stage":
        ctx.effects.play("spark", "hero")

def on_update(ctx):
    global effect_token
    if ctx.scene is None:
        return
    if ctx.input.pressed("effect_play"):
        effect_token = ctx.effects.play("spark", "@player")
        ctx.log("이펙트 시작: " + str(effect_token))
    if ctx.input.pressed("effect_stop") and effect_token is not None:
        ctx.effects.stop(effect_token)
        ctx.log("이펙트 중지")
        effect_token = None
