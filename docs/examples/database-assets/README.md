# DB 이미지·소리 예제

이 폴더를 Naru Studio에서 열고 제작툴 F9 → 표현 자료에서 이미지·소리 속성을 확인합니다. Ctrl+R → 상자 왼쪽까지 이동 → 오른쪽을 바라보고 Z로 조사하면 DB의 그림과 효과음을 적용합니다.

`scripts/db_media.py`의 `run(ctx)`를 상자의 스크립트 호출 명령이 실행합니다. 인자는 `ctx`, 반환은 None이며 전역 등록은 필요 없습니다. F5/F6에서는 DB 읽기·로그만 확인합니다. `media/demo`는 기본 그림·개별 효과음, `media/empty`는 기본 그림·소리 없음입니다.

[사용법·입력값·오류·수동 확인](../../guide/DB_ASSETS.md), [API](../../SCRIPT_API.md). 이미지·음원은 저장소의 직접 생성 예제에서 복사하며 개인 음원을 포함하지 않습니다.

새 사본: `python tools/generate_database_asset_example.py build/manual-db-assets`. 생성기는 기존 폴더를 덮어쓰지 않습니다.
