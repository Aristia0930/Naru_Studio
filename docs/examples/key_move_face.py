"""8을 누르면 위로 이동한 뒤 오른쪽을 바라보는 전역 스크립트.

사용 방법:
1. 이 파일을 게임 프로젝트의 scripts/ 폴더로 복사합니다.
2. 게임 → 키 설정에서 step_up_face_right 동작을 추가합니다.
3. 그 동작에 8을 연결합니다. 숫자 키패드도 쓰려면 Keypad 8도 추가합니다.
4. 스크립트 → 전역 스크립트에서 이 파일을 등록합니다.
5. 저장하고 게임을 실행해 8을 누릅니다.

위로 이동은 충돌을 검사하는 즉시 이동입니다. 벽·장면 끝이면 그 앞에서 멈춥니다.
이동이 막혀도 오른쪽은 바라봅니다. 누른 채 유지해도 한 번만 실행합니다.
키는 이 파일에서 중복 지정하지 않고 키 설정/Python 설정 파일의 연결을 사용합니다.
"""

KEY_MOVE_DISTANCE = 48.0  # 위로 이동할 거리(px). 원하는 숫자로 변경하세요.


def on_game_start(ctx):
    if "step_up_face_right" not in ctx.input.actions():
        ctx.warning("게임 → 키 설정에서 step_up_face_right 동작을 추가하고 8을 연결하세요.")


def on_update(ctx):
    # 플레이어가 없거나 대화·이벤트 진행 중이면 사용자 이동을 실행하지 않습니다.
    if ctx.player is None or ctx.input is None or ctx.busy:
        return
    if ctx.input.pressed("step_up_face_right"):
        ctx.player.move(0, -KEY_MOVE_DISTANCE)  # y가 작아지는 쪽이 위입니다.
        face_right(ctx)
        ctx.log(f"8 동작 실행: 위치=({ctx.player.x:.0f}, {ctx.player.y:.0f}), 오른쪽 보기")


def face_right(ctx):
    # 나중에 공통 이벤트의 '스크립트 호출'에서 이 함수만 부를 수도 있습니다.
    if ctx.player is not None:
        ctx.player.facing = "right"


def run(ctx):
    # F5는 게임 밖 단독 실행입니다. 실제 키 입력은 전역 등록 후 게임에서 시험하세요.
    ctx.log("키 설정에 step_up_face_right → 8을 연결하고 이 파일을 전역 등록한 뒤 게임을 실행하세요.")
