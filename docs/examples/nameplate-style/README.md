# 이름표 배경·위아래 예제

제작툴에서 이 폴더를 열고 Ctrl+R로 실행합니다. 왼편 NPC는 **투명 배경·위**, 여관은 **자주색 배경·아래**, 오른쪽 사각형은 **초록 배경·아래**로 이름표를 표시합니다.

객체를 선택해 인스펙터의 **게임 이름표**, **이름표 위치**, **배경 투명**, **이름표 배경색**을 바꾸고 Ctrl+S → Ctrl+R로 확인합니다. 투명을 켜도 고른 색은 보관하고, 해제하면 같은 색으로 표시합니다. 색 선택 취소는 원래 값을 유지합니다. Undo/Redo·저장/재열기도 지원합니다. F9 전체 테스트 표시는 독립적인 위쪽·어두운 배경입니다.

`scripts/nameplates.py`의 `transparent_above(ctx)`는 투명/위, `colored_below(ctx)`는 색/아래로 바꿉니다. 이벤트 스크립트 호출의 파일 nameplates.py·함수에 해당 이름을 지정합니다. ctx 인자·반환 None·전역 등록 불필요·대기 없음입니다. F5/F6은 ctx.scene=None 보호로 무변경입니다. 기본 NPC의 run 함수는 이름 문구 전환을 그대로 시험할 수 있습니다.

`python tools/generate_nameplate_style_example.py build/new-nameplate-style`로 새 사본을 생성할 수 있습니다. 기존 예제/사용자 프로젝트는 덮어쓰지 않으며 원래 nameplates 생성기의 직접 만든 PNG를 사용합니다.

[색/위치의 기본값·API·오류·그림·추가 확인 9~13](../../guide/NAMEPLATES.md).
