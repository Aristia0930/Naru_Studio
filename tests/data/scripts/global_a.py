# 전역 스크립트 테스트: 모듈 전역 변수가 틱마다 유지된다
counter = 0


def on_game_start(ctx):
    ctx.input.bind("dash", ["Shift"])
    ctx.variables.set("started", True)


def on_update(ctx):
    global counter
    counter += 1
    ctx.variables.set("ticks", counter)
    if ctx.player and ctx.input.held("dash"):
        ctx.player.speed = 500


def bump(ctx):
    # 이벤트의 "스크립트 호출"로 부르면 실행 중인 이 모듈의 counter를 바꾼다
    global counter
    counter += 100
    return counter
