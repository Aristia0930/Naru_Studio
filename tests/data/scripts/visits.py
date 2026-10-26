def run(ctx):
    count = ctx.variables.get("visits", 0) + 1
    ctx.variables.set("visits", count)
    print("print도 로그로 간다")
    ctx.log(f"방문 횟수: {count}")
    ctx.variables.set("greeting", "안녕하세요")
    ctx.variables.set("ratio", 0.5)
    ctx.variables.set("flag", True)
