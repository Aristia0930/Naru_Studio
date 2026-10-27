# 따라오기: 객체가 플레이어 쪽으로 다가온다.
# 쓰는 법: scripts/에 복사 → 스크립트 목록 오른쪽 클릭 → "전역 스크립트로 사용".
# 이 객체의 실행 조건을 "이벤트 접촉"으로 하면 플레이어를 붙잡는 순간 그 이벤트가 실행된다.

CHASER_ID = "obj_002"   # 따라올 객체의 ID (객체를 선택하면 인스펙터 맨 위에 보임)
CHASE_SPEED = 1         # 한 프레임(1/60초)에 움직일 픽셀


def on_update(ctx):                         # 매 프레임 자동으로 불림
    slime = ctx.scene.object(CHASER_ID)     # ID로 객체 가져오기 (없으면 None)
    if slime is None or ctx.player is None:
        return                              # 객체나 플레이어가 없는 장면이면 아무것도 안 함
    if ctx.busy:
        return                              # 대사·이벤트 중에는 멈춤
    dx = CHASE_SPEED if ctx.player.x > slime.x else -CHASE_SPEED   # 플레이어가 오른쪽이면 +, 왼쪽이면 -
    dy = CHASE_SPEED if ctx.player.y > slime.y else -CHASE_SPEED
    slime.move(dx, dy)                      # 충돌을 따지며 이동
