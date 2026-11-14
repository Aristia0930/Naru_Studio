# API 이벤트 예제

서버 조회 → 응답 변수 연결 → 성공/실패 조건 분기를 Python 없이 실행합니다. Windows용입니다.

1. 저장소 루트의 터미널에서 `python docs/examples/network/server.py`로 서버를 켭니다.
2. 별도 터미널에서 `python tools/generate_network_event_example.py build/manual-network-events`로 새 테스트 사본을 만듭니다. 기존 폴더가 비어 있지 않으면 덮어쓰지 않습니다.
3. Naru Studio에서 사본을 열고 게임 → 네트워크 접속 설정의 `local`과 `http://127.0.0.1:8765`를 확인합니다.
4. `notice` 객체의 이벤트에서 API 요청과 조건 분기를 확인하고 Ctrl+R로 실행합니다.
5. 실행 변수: `api_ok=True`, `api_code=200`, `공지내용=안녕하세요`, `점수=42`, `분기결과=성공`. 요청은 병렬 1회이며 플레이어는 방향키로 움직일 수 있습니다.
6. 경로 `/missing`이면 `api_code=404`, `api_error=http_error`, 응답 변수=None, 분기결과=실패입니다. `/slow`는 3초 지연합니다. 게임 창의 F8로 진행 중 이벤트를 중지하면 cancelled이며 뒤의 분기는 실행되지 않습니다.

F6는 실제 통신·변수 반영 없이 안내만 출력합니다. POST를 쓰려면 `/hello` 또는 원하는 경로에 POST, 본문 `{"score": 123}`을 지정하고 응답 `/received/score`를 변수에 연결합니다. 예제 서버는 POST 본문을 `received`에 담아 반환합니다. 서버의 저장/인증 기능은 없습니다. 종료는 서버 터미널에서 Ctrl+C입니다.

[전체 입력·오류·Python 공통 설정·수동 확인](../../guide/NETWORK_EVENTS.md). 직접 DB 연결은 다음 단계입니다.
