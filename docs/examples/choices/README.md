# 선택지 예제

이 폴더를 별도 작업 폴더로 복사한 뒤 Naru Studio에서 엽니다. 원본 예제를 보존하려면 복사본을 편집하세요.

- Ctrl+R → 바로 앞의 여관 주인을 Z/Enter/Space로 조사합니다.
- 위/아래로 고른 뒤 결정합니다. **쉬기**는 `rest_count`를 1 올리고, **이야기 듣기**는 중첩 선택지를 엽니다.
- X/Esc는 **나가기**를 고른 것처럼 실행합니다. 안쪽 선택지의 취소는 **다음에 듣기**입니다.
- 이벤트 탭에서 F6으로도 같은 선택·분기를 확인할 수 있습니다. 저장·재열기, 항목 순서 변경·Undo를 함께 확인하세요.

그림 촬영 (저장소 폴더의 PowerShell에서):

```powershell
$demo = 'docs/examples/choices'
$img = (Resolve-Path 'docs/guide/images').Path
& build/windows-msvc/bin/NaruStudio.exe --command-dialog show_choices --screenshot "$img/19-command-choices.png"
& build/windows-msvc/bin/NaruStudio.exe $demo --event obj_002 --run-event --smoke-test 1500 --screenshot "$img/20-choices-preview.png"
& build/windows-msvc/bin/NaruPlayer.exe $demo --frames 12 --hold confirm:2-2 --screenshot "$img/21-choices-game.png"
```
