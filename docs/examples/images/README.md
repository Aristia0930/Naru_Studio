# 객체 이미지·장면 배경 예제

폴더 전체를 `build/manual-images` 같은 새 위치에 복사한 뒤 Naru Studio에서 사본을 엽니다. 기존 사용자 사본을 덮어쓰지 않습니다. [설정·클릭 경로·8개 수동 확인](../../guide/IMAGES.md)을 따르세요.

- 배경 그룹: `day` 낮 풍경 / `night` 밤 풍경, 320×180 PNG.
- 캐릭터 그룹: `hero`, 투명 32×48 PNG → 게임에서 64×96 그림과 24×16 발밑 충돌 영역.
- 장식 그룹: `tree`, 투명·반투명 64×96 PNG → 게임에서 128×192 그림과 36×18 줄기 밑 충돌 영역.
- **Ctrl+R** → 방향키/WASD로 나무 바로 아래로 이동 → 위를 보고 결정 키(기본 Z/Enter/Space) → 대사를 넘기면 배경 낮/밤 전환. 나무 수관은 통과하고 줄기 밑은 막습니다.
- 장면/객체 인스펙터의 이미지 설정에서 그림·배경을 변경하고 Undo·저장·재열기를 확인합니다. **에셋 → 에셋 목록**에서 사용처와 사용 중 제거 차단을 확인할 수 있습니다.

`scripts/appearance.py`는 전역 등록 없이 나무 이벤트의 **스크립트 호출 → toggle_background**로 연결되어 있습니다. `toggle_background(ctx)`는 실제 게임에서 배경을 번갈아 바꾸고 bool을 반환합니다. `hide_tree_picture(ctx)`는 그림을 지워 색 사각형으로, `restore_tree_picture(ctx)`는 그림으로 복원하며 반환값은 None입니다. 편집 그림 설정·충돌은 유지하고 실행 사본만 변경합니다. F5/F6에는 `ctx.scene`이 없으므로 배경 전환은 안내 로그만 출력합니다. 오류는 파일·행·traceback과 호출한 이벤트 위치에 기록합니다.

PNG·코드는 `tools/generate_image_example.py`로 직접 만든 검증 자료이며 외부 게임·개인 원본을 복사하지 않았습니다. 이 저장소의 예제 제작·테스트·배포에 자유롭게 사용할 수 있고 별도 외부 저작자 표시는 필요하지 않습니다. 재생성은 새 폴더에 `python tools/generate_image_example.py build/new-image-example`로 실행하세요.
