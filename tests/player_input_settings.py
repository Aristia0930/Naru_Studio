"""실제 플레이어가 같은 Python 기본 설정과 실제 키 매핑을 사용하는지 검사한다."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

def run_case(root, speed=60, enabled=True, physical='L'):
    settings = (
        f'DEFAULT_CONTROL_ENABLED = {enabled!r}\nPLAYER_SPEED = {speed}\n'
        'KEY_BINDINGS = {"up": ["I"], "down": ["K"], "left": ["J"], '
        '"right": ["L"], "confirm": ["E"], "cancel": [], "dash": ["Shift"]}\n'
    )
    # 생성한 파일이 Python 문법으로도 올바른지 확인한다.
    compile(settings, 'input_settings.py', 'exec')
    (root / 'scripts/input_settings.py').write_text(settings, encoding='utf-8')
    result = subprocess.run([sys.argv[1], str(root), '--frames', '8', '--hold-key', f'{physical}:0-3'],
                            capture_output=True, encoding='utf-8', errors='replace', timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout

with tempfile.TemporaryDirectory(prefix='naru-input-') as directory:
    root = Path(directory)
    (root / 'scripts').mkdir()
    (root / 'scenes').mkdir()
    (root / 'project.json').write_text(json.dumps(dict(formatVersion=1, name='input', startScene='start',
        scenes=['start'], globalScripts=['observe.py'], inputSettingsScript='input_settings.py')), encoding='utf-8')
    (root / 'scenes/start.json').write_text(json.dumps(dict(formatVersion=1, id='start', name='input',
        width=960, height=640, background='#202830', player='hero', objects=[dict(id='hero', name='hero',
        x=0, y=0, width=16, height=16, color='#ffffff')])), encoding='utf-8')
    (root / 'scripts/observe.py').write_text('''_ticks = 0
def on_update(ctx):
    global _ticks
    _ticks += 1
    if _ticks == 7:
        ctx.log(f"input-result x={ctx.player.x:.1f} speed={ctx.player.speed:.1f} enabled={ctx.player.default_control}")
''', encoding='utf-8')
    assert 'input-result x=4.0 speed=60.0 enabled=True' in run_case(root)
    assert 'input-result x=0.0 speed=60.0 enabled=True' in run_case(root, physical='Right')
    assert 'input-result x=20.0 speed=300.0 enabled=True' in run_case(root, speed=300)
    assert 'input-result x=0.0 speed=60.0 enabled=False' in run_case(root, enabled=False)
    print('[PASS] Python source / key mapping / speed / ON-OFF in SDL player')

    def chord_case(keys, physical, expected_pressed, expected_held, runtime=False):
        bindings = dict(up=[], down=[], left=[], right=[], confirm=[], cancel=[], special=keys)
        source = f'DEFAULT_CONTROL_ENABLED = False\nPLAYER_SPEED = 60\nKEY_BINDINGS = {bindings!r}\n'
        (root / 'scripts/input_settings.py').write_text(source, encoding='utf-8')
        setup = f'    ctx.input.bind("special", {keys!r})\n' if runtime else '    pass\n'
        if runtime:
            bindings['special'] = []
            (root / 'scripts/input_settings.py').write_text(
                f'DEFAULT_CONTROL_ENABLED = False\nPLAYER_SPEED = 60\nKEY_BINDINGS = {bindings!r}\n', encoding='utf-8')
        observer = 'def on_game_start(ctx):\n' + setup + '''
def on_update(ctx):
    if ctx.input.pressed("special"):
        ctx.log("chord-pressed")
    if ctx.input.held("special"):
        ctx.log("chord-held")
'''
        (root / 'scripts/observe.py').write_text(observer, encoding='utf-8')
        args = [sys.argv[1], str(root), '--frames', '8']
        for key in physical:
            args.extend(['--hold-key', key])
        result = subprocess.run(args, capture_output=True, encoding='utf-8', errors='replace', timeout=10)
        assert result.returncode == 0, result.stdout + result.stderr
        assert result.stdout.count('chord-pressed') == expected_pressed, result.stdout
        assert result.stdout.count('chord-held') == expected_held, result.stdout

    chord_case(['Shift + E', 'F'], ['E:0-3'], 0, 0)
    chord_case(['Shift + E', 'F'], ['Left Shift:0-3'], 0, 0)
    chord_case(['Shift + E', 'F'], ['E:0-3', 'Left Shift:0-3'], 1, 4)
    chord_case(['Shift + E', 'F'], ['E:0-7', 'Right Shift:2-4'], 1, 3)
    chord_case(['Shift + E', 'F'], ['Left Shift:0-7', 'E:2-4'], 1, 3)
    chord_case(['Shift + E', 'F'], ['Left Shift:0-7', 'E:0-1', 'E:4-5'], 2, 4)
    chord_case(['Shift + E', 'F'], ['F:0-3'], 1, 4)
    chord_case(['Ctrl + Keypad +'], ['Right Ctrl:0-3', 'Keypad +:0-3'], 1, 4)
    chord_case(['Ctrl + Shift + E'], ['Right Ctrl:0-3', 'Left Shift:0-3', 'E:0-3', 'W:0-7'], 1, 4)
    chord_case(['Shift + E'], ['Right Shift:0-3', 'E:0-3'], 1, 4, runtime=True)
    print('[PASS] SDL simultaneous keys / either order / release-repress / OR / keypad plus / Python bind')
