def run(ctx):
    ctx.input.bind("special", ["e+shift", "F"])
    assert ctx.input.keys("special") == ["Shift + E", "F"]


def invalid(ctx):
    ctx.input.bind("special", ["Shift + Left Shift"])
