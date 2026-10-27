# 스크립트로 다른 객체 다루기
def on_update(ctx):
    box = ctx.scene.object("box")
    box.move(4, 0)                       # 충돌을 따지며 오른쪽으로


def inspect(ctx):
    box = ctx.scene.find("상자")
    hero = ctx.scene.object("hero")
    ctx.variables.set("box_id", box.id)
    ctx.variables.set("hero_is_player", hero.is_player)
    ctx.variables.set("count", len(ctx.scene.objects()))
    ctx.variables.set("missing", ctx.scene.object("nope") is None and ctx.scene.find("없음") is None)
    box.color = "#ff8800"
    box.passable = True
    box.x = 10
    return box.x


def bad_color(ctx):
    ctx.scene.object("box").color = "red"
