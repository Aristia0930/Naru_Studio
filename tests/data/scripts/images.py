def run(ctx):
    assert ctx.scene is not None
    obj = ctx.scene.objects()[0]
    obj.image = "picture"
    ctx.scene.background_image = "picture"
    assert obj.image == "picture"
    assert ctx.scene.background_image == "picture"
    for bad in ("sound", "missing"):
        try:
            obj.image = bad
            raise AssertionError("invalid object image was accepted")
        except ValueError:
            pass
        try:
            ctx.scene.background_image = bad
            raise AssertionError("invalid background was accepted")
        except ValueError:
            pass
    assert obj.image == "picture"
    obj.image = ""
    ctx.scene.background_image = ""
    assert obj.image == "" and ctx.scene.background_image == ""


def bad_type(ctx):
    ctx.scene.objects()[0].image = 3
