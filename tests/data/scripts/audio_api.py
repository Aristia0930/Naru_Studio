def run(ctx):
    assert ctx.audio is None


def game(ctx):
    assert ctx.audio.play_bgm("music", volume=40, loop=True) is None
    assert ctx.audio.play_se("music", volume=60) is None
    assert ctx.audio.stop_bgm(fade_seconds=1.5) is None
    assert ctx.audio.stop_se() is None
    for bad in (-1, 101, float("nan"), float("inf")):
        try:
            ctx.audio.play_bgm("music", volume=bad)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid volume accepted")
    for bad in (-1, 61, float("nan")):
        try:
            ctx.audio.stop_bgm(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid fade accepted")
    for bad in ("missing", "picture"):
        try:
            ctx.audio.play_se(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid asset accepted")
    try:
        ctx.audio.play_bgm("music", loop="yes")
    except TypeError:
        pass
    else:
        raise AssertionError("invalid loop accepted")
    return 4


def failure(ctx):
    ctx.audio.play_bgm("music")
