# 공개 API 응답 확인 예제

Naru Studio에서 이 폴더를 열고 **Ctrl+R**을 누릅니다. 인터넷에 연결되어 있으면 전역 스크립트 `scripts/network_demo.py`가 JSONPlaceholder의 `https://jsonplaceholder.typicode.com/todos/1`을 조회합니다. 제작툴의 **실행 변수**에서 `api_status`, `api_title`, `api_completed`를 확인하세요.

정상 응답의 예: `api_status`는 `완료`, `api_title`은 `delectus aut autem`, `api_completed`는 `False`입니다. 공개 사이트 내용과 연결 상태는 바뀔 수 있습니다. [제공 사이트의 응답](https://jsonplaceholder.typicode.com/todos/1), [상세 사용법·오류·수동 확인](../../guide/NETWORK.md).

이 예제는 GET만 사용하며 데이터를 저장하지 않습니다. 사이트의 POST/PUT/PATCH/DELETE는 실제 저장이 아닌 모의 응답입니다. 인터넷 없는 자동 검증이나 지연·오류 확인에는 별도의 `server.py` 로컬 서버를 사용할 수 있습니다.
