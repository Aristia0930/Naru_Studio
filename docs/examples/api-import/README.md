# API 가져오기 검증 서버

`python docs/examples/api-import/server.py`로 실행합니다. 기본 포트는 8766이고 인자로 바꿀 수 있습니다. 서버의 `/` 응답은 ID·표시 이름·가격이 있는 아이템 배열입니다. 저장이나 서버 쓰기는 하지 않습니다.

[DB 준비·조회·필드 연결·미리보기·적용·Undo·저장/재열기·오류/취소 확인](../../guide/API_IMPORT.md)을 따릅니다. `/bad`, `/duplicate`, `/missing`, `/slow`는 오류·취소 검증용입니다.
