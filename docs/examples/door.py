# 문 열기: 조사하면 문을 지나갈 수 있게 바꾸고 색을 바꾼다.
# 쓰는 법: scripts/에 복사. 문 객체의 이벤트를 실행 조건 "조사" + 명령 "스크립트 호출: door.py · open_door()"로.
# 바꾼 상태는 게임 실행 중·그 장면에 있는 동안만 유지된다. 장면에 다시 들어와도 열려 있게 하려면
# 이 파일을 전역 스크립트로도 등록한다 (아래 on_scene_enter가 게임 변수를 보고 다시 연다).

DOOR_ID = "obj_002"      # 문 객체의 ID
DOOR_OPEN_COLOR = "#3a3a3a"


def open_door(ctx):
    door = ctx.scene.object(DOOR_ID)
    if door is None:
        return
    door.passable = True                     # 이제 지나갈 수 있음
    door.color = DOOR_OPEN_COLOR             # 열린 느낌으로 어둡게
    ctx.variables.set("door_opened", True)   # 이벤트에서도 쓸 수 있게 게임 변수로 기록


def on_scene_enter(ctx):                     # (전역 스크립트로 등록했을 때) 장면에 들어올 때마다
    if ctx.variables.get("door_opened", False):
        open_door(ctx)
