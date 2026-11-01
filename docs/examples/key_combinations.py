"""쓰는 법: scripts/에 복사하고 전역 스크립트로 등록합니다.
게임 → 키 설정에서 special_action을 추가하고 '동시 키 조합 추가'로 Shift와 E를 선택합니다.
키 목록에 F도 별도로 추가하면 Shift+E 또는 F로 실행합니다. 저장 후 Ctrl+R로 시험하세요.
"""


def on_update(ctx):
    if ctx.input is None or ctx.busy:
        return
    if ctx.input.pressed("special_action"):
        ctx.log("동시 입력 동작 실행! 여기에 스킬이나 공통 이벤트 호출을 작성하세요.")
        # 예: ctx.run_event("common_1")  # 프로젝트에 실제 존재하는 ID로 바꾸세요.


def run(ctx):
    ctx.log("special_action에 Shift + E를 연결하고 이 파일을 전역 등록한 뒤 게임에서 시험하세요.")
