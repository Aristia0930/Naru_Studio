def run(ctx):
    # 게임 밖(에디터 테스트 실행)에서는 게임 상태가 없다
    return ctx.player is None and ctx.input is None and ctx.scene is None
