# 이벤트의 스크립트 호출에 연결. 전역 등록은 필요하지 않습니다.
def toggle_background(ctx):
    if ctx.scene is None:
        ctx.log("배경 변경은 Ctrl+R 실제 게임에서 확인하세요.")
        return False
    if ctx.scene.background_image == "day":
        ctx.scene.background_image = "night"
    else:
        ctx.scene.background_image = "day"
    return True


def hide_tree_picture(ctx):
    if ctx.scene is not None:
        ctx.scene.object("tree").image = ""


def restore_tree_picture(ctx):
    if ctx.scene is not None:
        ctx.scene.object("tree").image = "tree"
