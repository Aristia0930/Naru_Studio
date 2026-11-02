"""실제 플레이어의 변수 관측·중첩 오류 위치·로그 파일을 임시 사본에서 검사한다."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

exe, fixture = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix='naru-debug-') as temporary:
    root = Path(temporary)
    project_dir = root / 'project'
    shutil.copytree(fixture, project_dir)
    manifest = project_dir / 'project.json'
    project = json.loads(manifest.read_text(encoding='utf-8'))
    project['variables'] += [
        {'name': 'opened', 'value': False},
        {'name': 'message', 'value': '한글\t줄\n경로 C:\\new\\test'},
    ]
    manifest.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    before = manifest.read_bytes()
    log = root / '게임 로그.log'
    args = [exe, str(project_dir), '--debug-state', '--log-file', str(log),
            '--frames', '28', '--hold', 'confirm:2-2', '--hold', 'confirm:5-5',
            '--hold', 'confirm:15-15']
    result = subprocess.run(args, capture_output=True, encoding='utf-8', timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    states = [json.loads(line.split('\t', 1)[1]) for line in result.stdout.splitlines()
              if line.startswith('NARU_STATE\t')]
    assert len(states) >= 3 and states[0]['hp']['value'] == '10', states
    assert states[-1]['hp']['value'] == '100', states
    assert states[-1]['opened']['type'] == 1, states
    assert states[-1]['message']['value'] == '한글\t줄\n경로 C:\\new\\test', states
    logged = log.read_text(encoding='utf-8')
    assert '공통 회복 완료' in logged and '게임 종료' in logged
    assert 'NARU_STATE' not in logged
    assert manifest.read_bytes() == before, '실행은 편집 원본을 바꾸면 안 된다'
    print('[PASS] 실제 값 변경·스위치/문자열·최종 스냅샷·파일 로그·원본 보존')

    project['commonEvents'][1]['commands'][1]['op'] = 'subtract'
    project['commonEvents'][1]['commands'][1]['value'] = '숫자에서 글자를 빼면 오류'
    manifest.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    result = subprocess.run(args, capture_output=True, encoding='utf-8', timeout=15)
    assert result.returncode == 3, result.stdout + result.stderr
    failures = [line.split('\t') for line in result.stdout.splitlines()
                if line.startswith('NARU\terror') and '명령 실패' in line]
    inner = next(fields for fields in failures if len(fields) >= 10 and fields[9] == 'heal')
    assert inner[7:9] == ['scene_001', 'obj_002'], inner
    assert 'recover' in inner[6] and 'heal' in inner[6], inner
    assert log.read_text(encoding='utf-8').count('명령 실패') > 0
    print('[PASS] 중첩 공통 이벤트 오류의 장면·객체·명령·호출 경로와 파일 로그')
