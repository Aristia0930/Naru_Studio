"""scripts/reward.py로 복사하고 이벤트의 스크립트 호출에서 give를 선택하세요.

반환값 변수 rewarded 뒤에 rewarded == 참 조건 분기를 연결할 수 있습니다.
이 예제의 보상은 프로젝트 전체에서 한 실행 세션에 한 번입니다.
"""


def give(ctx):
    if ctx.variables.get("welcome_reward_taken", False):
        return False
    gold = ctx.variables.get("gold", 0)
    ctx.variables.set("gold", gold + 100)
    ctx.variables.set("welcome_reward_taken", True)
    ctx.log("환영 보상 100골드 지급")
    return True
