def run(ctx):
    if ctx.scene is None:
        ctx.log("이름표 변경은 Ctrl+R 게임 실행에서 확인하세요.")
        return
    npc = ctx.scene.object("npc")
    if npc is None:
        ctx.warning("npc 객체가 없습니다.")
        return
    npc.show_name = True
    npc.name_text = "어서 오세요" if npc.name_text != "어서 오세요" else "여관 주인"
    ctx.log("NPC 이름표: " + npc.name_text)


def hide_name(ctx):
    if ctx.scene is not None:
        npc = ctx.scene.object("npc")
        if npc is not None:
            npc.show_name = False


def use_object_name(ctx):
    if ctx.scene is not None:
        npc = ctx.scene.object("npc")
        if npc is not None:
            npc.show_name = True
            npc.name_text = ""
