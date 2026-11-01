"""실제 SDL 플레이어에서 공통 이벤트 명령·Python·취소를 검사한다."""
import subprocess
import sys

def check(name, holds, expected, forbidden):
    args = [sys.argv[1], sys.argv[2], '--frames', '28']
    for hold in holds:
        args += ['--hold', hold]
    result = subprocess.run(args, capture_output=True, encoding='utf-8', errors='replace', timeout=10)
    assert result.returncode == 0, f'{name}: {result.returncode}\n{result.stdout}\n{result.stderr}'
    cursor = 0
    for text in expected:
        index = result.stdout.find(text, cursor)
        assert index >= 0, f'{name}: missing or out of order: {text}\n{result.stdout}'
        cursor = index + len(text)
    for text in forbidden:
        assert text not in result.stdout, f'{name}: unexpected: {text}\n{result.stdout}'
    print(f'[PASS] {name}')

check('명령 호출', ['confirm:2-2', 'confirm:5-5', 'confirm:15-15'],
      ['선택: 명령으로 호출', '공통 회복 완료', '체력을 회복했습니다.', '호출 다음 명령으로 복귀'],
      ['Python 예약 완료', '명령 실패'])
check('Python 호출', ['confirm:2-2', 'down:4-4', 'confirm:6-6', 'confirm:16-16'],
      ['선택: Python으로 호출', 'Python 예약 완료', '공통 회복 완료', '체력을 회복했습니다.', '호출 다음 명령으로 복귀'],
      ['명령 실패'])
check('취소', ['confirm:2-2', 'cancel:5-5'],
      ['선택: 나가기', '호출 다음 명령으로 복귀'], ['공통 회복 완료', 'Python 예약 완료'])
