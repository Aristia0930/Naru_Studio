# 캐릭터 애니메이션 예제

자체 생성한 색상 프레임입니다. sheet.png는 32×32칸 3열×4행이고 pose.png는 32×48 낱장 이미지입니다. 객체 hero에 idle/walk 4방향·attack·cast·victory·atlas 클립이 있습니다.

방향키 이동, X 공격, C 시전, B 반복 동작(이동 잠금), V 중지. B 후 V 또는 F8로 중지하거나 C로 교체할 수 있습니다. V 결과는 제작툴 로그에 표시됩니다. 표지판을 조사하면 객체 애니메이션 이벤트가 끝난 뒤 대사가 나옵니다. 전역 스크립트 animation_controls.py가 프레임 신호를 로그와 last_animation_signal 변수로 전달합니다.

[편집·입력 규칙·Python/이벤트 연결·수동 테스트](../../guide/ANIMATION.md). `python tools/create_animation_example.py build/manual-animation`으로 새 사본을 생성합니다. 기존 폴더는 덮어쓰지 않습니다.
