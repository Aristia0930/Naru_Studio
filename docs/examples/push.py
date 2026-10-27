# 밀기: 플레이어가 부딪힌 상자를 바라보는 방향으로 한 칸 민다.
# 쓰는 법: scripts/에 복사 (전역 등록은 하지 않음). 상자 객체의 이벤트를
#   실행 조건 "플레이어 접촉" + 명령 "스크립트 호출: push.py · push()" (반환값 → 변수 pushed, 선택)
# 으로 만든다. pushed가 false면 "꿈쩍도 하지 않는다" 같은 대사를 조건 분기로 띄울 수 있다.

PUSH_BOX_ID = "obj_002"  # 밀 상자의 ID
PUSH_STEP = 48           # 한 번에 밀 거리(픽셀)


def push(ctx):           # 정해진 훅 이름이 아니므로 이벤트가 불러야 실행된다
    box = ctx.scene.object(PUSH_BOX_ID)
    if box is None or ctx.player is None:
        return False
    # 플레이어가 바라보는 방향 -> 밀 방향
    moves = {"right": (PUSH_STEP, 0), "left": (-PUSH_STEP, 0), "down": (0, PUSH_STEP), "up": (0, -PUSH_STEP)}
    dx, dy = moves[ctx.player.facing]
    before = (box.x, box.y)
    box.move(dx, dy)                         # 벽에 막히면 덜 가거나 그대로
    return (box.x, box.y) != before          # 실제로 움직였으면 True
