def run(ctx):
    media = ctx.db.get("media", "demo")
    ctx.log(str(media))
    if ctx.scene is None or ctx.audio is None:
        ctx.log("F5/F6은 DB 읽기만 확인합니다. 그림·소리는 Ctrl+R에서 확인하세요.")
        return
    obj = ctx.scene.object("display")
    obj.image = media["picture"] or ""
    if media["sound"] is not None:
        ctx.audio.play_se(media["sound"], volume=60)
