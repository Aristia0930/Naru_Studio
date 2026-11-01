def run(ctx):
    # 함수가 끝난 뒤 공통 이벤트를 실행합니다.
    ctx.run_event("recover")
    ctx.log("Python 예약 완료")
