# 스크립트에 공개된 이름을 모은다 (docs/SCRIPT_API.md에 모두 적혀 있는지 검사용)
import naru


def run(ctx):
    names = set(vars(naru.Context()))  # ctx.variables, ctx.db, ctx.input ...
    for cls in (naru.Context, naru.Variables, naru.Database, naru.Input, naru.Player, naru.Scene):
        names.update(n for n in vars(cls) if not n.startswith("_"))
    return ",".join(sorted(names))
