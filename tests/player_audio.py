"""Exercise the real SDL player, Python API, event scheduling, and missing-file error logs."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

player, root = Path(sys.argv[1]), Path(sys.argv[2])
spec=importlib.util.spec_from_file_location('audio_generator',root/'tools/generate_audio_example.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='naru-audio-') as temporary:
    project=module.generate(Path(temporary)/'project')
    scene_file=project/'scenes/scene_001.json'
    scene=json.loads(scene_file.read_text(encoding='utf-8'))
    scene['objects'][1]['event']['commands']=[
        dict(type='audio',action='play_bgm',asset='test_bgm',volume=25,loop=True),
        dict(type='call_script',script='audio_demo.py',function='run'),
        dict(type='audio',action='play_se',asset='test_se',volume=50),
        dict(type='wait',seconds=.05),
        dict(type='audio',action='stop_bgm',fadeSeconds=.1),
        dict(type='audio',action='stop_se'),
        dict(type='log',text='AUDIO_SEQUENCE_COMPLETE')]
    scene_file.write_text(json.dumps(scene),encoding='utf-8')
    env=dict(os.environ,SDL_AUDIO_DRIVER='dummy')
    def run(code):
        log=Path(temporary)/'player.log'
        result=subprocess.run([str(player),str(project),'--frames','35','--log-file',str(log)],env=env,capture_output=True,timeout=20)
        text=log.read_text(encoding='utf-8')
        assert result.returncode==code,(result.returncode,result.stderr,text)
        return text
    text=run(0)
    assert 'AUDIO_SEQUENCE_COMPLETE' in text and 'Python BGM·효과음 호출 완료' in text
    assert '소리: play_bgm' in text and '소리: stop_bgm' in text
    (project/'assets/demo_bgm.wav').unlink()
    text=run(3)
    assert 'AUDIO_SEQUENCE_COMPLETE' not in text
    assert 'test_bgm' in text
    print('[PASS] actual player audio events/Python, wait, stop/fade, missing-file failure')
