# BGM·효과음 예제

제작툴에서 이 폴더를 열고 Ctrl+R로 실행합니다. 시작하면 짧은 BGM이 반복됩니다. 위쪽 객체를 왼쪽부터 조사하면 BGM 다시 재생 / 효과음 / 2초 페이드 정지 / 효과음 정지 / Python 재생을 확인합니다. F6은 로그만 남깁니다.

`assets/demo_bgm.wav`와 `effect.wav`는 `tools/generate_audio_example.py`가 직접 만든 테스트 톤입니다. 개인 음악 없이 clone한 PC에서 재현할 수 있습니다. 새 폴더에 `python tools/generate_audio_example.py build/new-audio-example`로 생성합니다. 기존 프로젝트는 덮어쓰지 않습니다.

`scripts/audio_demo.py`의 `run(ctx)`는 BGM/효과음 재생, `fade(ctx)`는 2초 페이드 정지, `stop_effects(ctx)`는 효과음 모두 정지입니다. 스크립트 호출 명령에서 해당 함수를 선택하며 반환값은 None입니다. 전역 등록은 필요 없습니다. `ctx.audio is None` 보호로 F5/F6에서는 안내만 표시합니다.

[입력값·API·오류·수동 확인·개인 MP3 테스트 안내](../../guide/AUDIO.md).

처음 확인할 때는 [클릭 순서·객체 위치·기대 결과·결과 양식](../../guide/AUDIO_TEST.md)을 따릅니다. 짧은 효과음은 0.35초라 천천히 여러 번 조사하는 것만으로 겹침/정지 여부를 듣기 어렵습니다. 상세 안내의 ‘효과음 → 대기 0.1초 → 효과음’과 긴 음원의 효과음 재생 절차로 각각 확인합니다.
