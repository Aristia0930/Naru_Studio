# 공통 이벤트 예제

이 폴더를 별도 위치로 복사한 뒤 Naru Studio에서 열고 **Ctrl+R**로 실행하세요. 주인공 바로 아래의 회복 샘을 **결정 키(Z/Enter/Space)**로 조사합니다.

- **명령으로 호출**: 공통 이벤트 `recover` → `heal` 순서로 중첩 호출합니다.
- **Python으로 호출**: `scripts/recover.py`가 `ctx.run_event("recover")`를 예약합니다. Python 로그가 먼저 나오고 회복을 실행합니다.
- **나가기 / X·Esc**: 회복 없이 돌아옵니다.

`heal`은 잠깐 기다렸다가 `hp`를 100으로 바꾸고, `recover`는 대사를 보여 줍니다. 대사를 넘기면 원래 객체 이벤트의 다음 로그 명령으로 돌아갑니다. 공통 이벤트 탭에서 `recover`를 고르고 **F6**로도 실행할 수 있습니다. **F8**은 안쪽 호출까지 중지합니다.

원본은 자동 검사·설명서용이므로 편집할 때는 복사본을 사용하세요. 실제 SDL 플레이어의 명령 호출·Python 호출·취소는 `ctest`의 `player_common_events`가 확인합니다.

촬영 (저장소 폴더에서 PowerShell):

```powershell
$exe = "build\windows-msvc\bin\NaruStudio.exe"
& $exe docs/examples/common-events --common-event recover --run-event --smoke-test 1500 --screenshot docs/guide/images/22-common-events.png
& $exe --command-dialog call_common_event --screenshot docs/guide/images/23-command-common-event.png
```
