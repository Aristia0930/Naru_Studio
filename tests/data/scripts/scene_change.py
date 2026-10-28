# 장면 이동: 도착 위치와 방향
def go(ctx):
    ctx.scene.change("scene_002", 64, 96, "left")


def half(ctx):
    ctx.scene.change("scene_002", 64)          # y가 없다 → ValueError


def bad_facing(ctx):
    ctx.scene.change("scene_002", facing="north")  # ValueError
