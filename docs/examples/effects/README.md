# 이펙트 실행 예제

이 폴더는 `tools/create_effect_example.py`로 만든 자체 생성 예제입니다. 기존 프로젝트를 덮어쓰지 않으려면 `python tools/create_effect_example.py build/manual-effect`로 새 사본을 만들고 제작툴에서 여세요.

에셋 → 이펙트 라이브러리에서 `spark`의 세 프레임과 소리/번쩍임/흔들림을 확인합니다. 각 프레임은 1초이며 전체는 약 3초입니다. Ctrl+R 게임 창에서 **E**를 누르면 플레이어 위에 이펙트가 재생되고, **R**은 마지막 재생을 중지합니다. 표지판을 조사하면 이펙트가 끝난 뒤 대사가 나옵니다. Python은 `scripts/animation_controls.py`의 `on_game_start`/`on_update`에 연결돼 있습니다. [입력 규칙·오류·F6 차이](../../guide/EFFECTS.md), [다음에 진행할 수동 테스트](../../guide/EFFECT_MANUAL_TEST.md).

오른쪽 **다른 장면** 표지판과 다음 장면의 **돌아가기** 표지판은 장면 이동 중 이펙트 정리를 확인할 때 사용합니다.
