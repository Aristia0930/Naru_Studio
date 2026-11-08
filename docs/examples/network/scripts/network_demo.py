"""JSONPlaceholder의 공개 GET 응답을 비동기로 받아 실행 변수에 저장한다."""

URL = "https://jsonplaceholder.typicode.com/todos/1"
request_id = None


def on_game_start(ctx):
    global request_id
    ctx.variables.set("api_status", "요청 중")
    try:
        request_id = ctx.network.request(URL, timeout=10.0)
    except (ValueError, RuntimeError) as error:
        ctx.variables.set("api_status", "실패")
        ctx.log("API 요청 시작 실패: " + str(error))
        return
    ctx.log("공개 테스트 API 요청 시작")


def on_update(ctx):
    global request_id
    if request_id is None:
        return
    result = ctx.network.result(request_id)
    if result is None:
        return
    request_id = None
    if not result["ok"]:
        ctx.variables.set("api_status", "실패")
        ctx.log("API 요청 실패: " + result["error"])
        return
    data = result["json"]
    if not isinstance(data, dict) or not isinstance(data.get("title"), str) or not isinstance(data.get("completed"), bool):
        ctx.variables.set("api_status", "응답 형식 오류")
        ctx.log("API 응답에 title 또는 completed가 없습니다.")
        return
    ctx.variables.set("api_title", data["title"])
    ctx.variables.set("api_completed", data["completed"])
    ctx.variables.set("api_status", "완료")
    ctx.log("API 응답 도착: " + data["title"])


def run(ctx):
    ctx.log("게임 실행 Ctrl+R에서 공개 API 요청과 실행 변수를 확인하세요.")
