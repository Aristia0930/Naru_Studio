# 이름표 예제

제작툴에서 이 폴더를 열어 Ctrl+R로 실행합니다. NPC는 **여관 주인**, 여관 사각형은 **여관**을 머리 위에 표시합니다. 주인공·일반 객체는 표시하지 않고 숨긴 객체는 그림도 표시하지 않습니다.

게임 창에서 **F9**를 누르면 모든 객체의 편집용 이름·ID와 숨김 상태가 보입니다. 다시 누르면 게임용 이름표만 남습니다. 제작툴 창의 F9는 기존 데이터베이스 단축키입니다.

NPC 아래에서 위를 보고 Z/Enter/Space로 조사하면 Python `nameplates.py`의 `run(ctx)`가 이름표를 `어서 오세요`/`여관 주인`으로 전환합니다. `hide_name(ctx)`는 이름표만 숨기고 `use_object_name(ctx)`는 빈 문구로 객체 이름을 사용합니다. 인자 ctx·반환 None·전역 등록 불필요, F5/F6 보호 코드가 있습니다.

객체 `npc`의 showName=true/nameText='여관 주인', `inn`의 showName=true/문구 빈 칸을 비교하세요. `assets/npc.png`는 생성 도구가 직접 만든 테스트 그림입니다. `tools/generate_nameplate_example.py`로 새 폴더에 재생성할 수 있습니다. 개인 게임 자료는 포함하지 않습니다.

[설정·Python API·그림·수동 확인](../../guide/NAMEPLATES.md).
