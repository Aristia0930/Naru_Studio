# 순찰: 객체가 두 지점 사이를 왔다 갔다 한다.
# 쓰는 법: scripts/에 복사 → 스크립트 목록 오른쪽 클릭 → "전역 스크립트로 사용".

PATROL_ID = "obj_002"   # 움직일 객체의 ID
PATROL_LEFT = 500       # 왼쪽 끝 x
PATROL_RIGHT = 800      # 오른쪽 끝 x
PATROL_SPEED = 2        # 한 프레임에 움직일 픽셀

patrol_direction = 1    # 1 = 오른쪽, -1 = 왼쪽. 함수 밖에 둬서 게임 중 계속 기억된다


def on_update(ctx):
    global patrol_direction                 # 위의 patrol_direction을 이 함수에서 바꾸겠다는 표시
    npc = ctx.scene.object(PATROL_ID)
    if npc is None or ctx.busy:
        return
    npc.move(PATROL_SPEED * patrol_direction, 0)
    if npc.x >= PATROL_RIGHT:               # 오른쪽 끝에 닿으면 왼쪽으로
        patrol_direction = -1
    elif npc.x <= PATROL_LEFT:              # 왼쪽 끝에 닿으면 오른쪽으로
        patrol_direction = 1
