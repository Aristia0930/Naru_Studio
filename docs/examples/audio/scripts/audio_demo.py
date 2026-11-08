def run(ctx):
    if ctx.audio is None:
        ctx.log("소리는 Ctrl+R 게임 실행에서 확인하세요.")
        return
    ctx.audio.play_bgm("test_bgm", volume=35, loop=True)
    ctx.audio.play_se("test_se", volume=70)
    ctx.log("Python BGM·효과음 호출 완료")


def fade(ctx):
    if ctx.audio is not None:
        ctx.audio.stop_bgm(fade_seconds=2)


def stop_effects(ctx):
    if ctx.audio is not None:
        ctx.audio.stop_se()
