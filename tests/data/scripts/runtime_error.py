def helper():
    return 1 / 0


def run(ctx):
    ctx.log("before error")
    helper()
