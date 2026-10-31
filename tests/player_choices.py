"""실제 SDL 플레이어에서 예제 프로젝트의 선택·취소·중첩 분기를 확인한다."""

import subprocess
import sys


def run_case(name, holds, expected, forbidden):
    args = [sys.argv[1], sys.argv[2], "--frames", "24"]
    for hold in holds:
        args += ["--hold", hold]
    result = subprocess.run(args, capture_output=True, encoding="utf-8", errors="replace", timeout=10)
    assert result.returncode == 0, f"{name}: exit={result.returncode}\n{result.stdout}\n{result.stderr}"
    cursor = 0
    for text in expected:
        index = result.stdout.find(text, cursor)
        assert index >= 0, f"{name}: missing or out of order: {text}\n{result.stdout}"
        cursor = index + len(text)
    for text in forbidden:
        assert text not in result.stdout, f"{name}: unexpected branch: {text}\n{result.stdout}"
    print(f"[PASS] {name}")


common = "선택한 분기와 공통 후속 명령을 마쳤습니다."
run_case(
    "처음 선택 결정",
    ["confirm:2-2", "confirm:5-5", "confirm:9-9"],
    ["선택: 쉬기", "푹 쉬셨나요?", common],
    ["선택: 이야기 듣기", "선택: 나가기"],
)
run_case(
    "중첩 선택지",
    ["confirm:2-2", "down:5-5", "confirm:8-8", "confirm:12-12", "confirm:16-16"],
    ["선택: 이야기 듣기", "선택: 마을 소문", "동쪽 숲", common],
    ["선택: 쉬기", "선택: 나가기"],
)
run_case(
    "취소 키로 나가기",
    ["confirm:2-2", "cancel:5-5", "confirm:9-9"],
    ["선택: 나가기", "언제든 들러 주세요.", common],
    ["선택: 쉬기", "선택: 이야기 듣기"],
)
run_case(
    "중첩 선택지 취소는 빈 분기를 지나 공통 명령 실행",
    ["confirm:2-2", "down:5-5", "confirm:8-8", "cancel:12-12"],
    ["선택: 이야기 듣기", "선택: 다음에 듣기", common],
    ["동쪽 숲", "선택: 쉬기", "선택: 나가기"],
)
