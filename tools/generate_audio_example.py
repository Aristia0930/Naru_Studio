"""Create an isolated audio demo. --bgm copies a user supplied WAV/MP3 without changing it."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import wave


def generate(directory, bgm=None):
    root = Path(directory)
    if (root / 'project.json').exists():
        raise FileExistsError('기존 프로젝트를 덮어쓰지 않습니다. 새 폴더를 지정하세요.')
    for sub in ('assets', 'scenes', 'scripts'):
        (root / sub).mkdir(parents=True, exist_ok=True)
    def tone(name, seconds, hz):
        with wave.open(str(root / 'assets' / name), 'wb') as output:
            output.setparams((1, 2, 22050, 0, 'NONE', 'not compressed'))
            output.writeframes(b''.join(struct.pack('<h', int(4000 * math.sin(2*math.pi*hz*i/22050) * min(1, i/220, (seconds*22050-i)/220))) for i in range(int(seconds*22050))))
    tone('effect.wav', .35, 880)
    if bgm:
        source = Path(bgm)
        if source.suffix.lower() not in ('.mp3', '.wav'):
            raise ValueError('BGM은 WAV/MP3를 지정하세요.')
        music = 'user_bgm' + source.suffix.lower()
        shutil.copyfile(source, root / 'assets' / music)
        source_name = source.name
    else:
        music, source_name = 'demo_bgm.wav', 'demo_bgm.wav'
        tone(music, 2, 220)
    def asset(key, filename, name, original=None):
        return dict(id=key, name=name, kind='audio', path='assets/'+filename, sourceName=original or filename,
                    sha256=hashlib.sha256((root/'assets'/filename).read_bytes()).hexdigest())
    def audio(action, **options):
        return dict(type='audio', action=action, **options)
    def obj(key, name, x, commands, trigger='action', y=120):
        return dict(id=key, name=name, x=x, y=y, width=42, height=42, color='#6b93be', passable=False, visible=True,
                    event=dict(trigger=trigger, commands=commands))
    hero = obj('hero', '주인공', 55, [], y=220)
    objects = [hero, obj('startup', '처음 BGM 재생', 10, [audio('play_bgm',asset='test_bgm',volume=35,loop=True)], 'autorun', y=10)]
    for index, (key, name, command) in enumerate([
        ('music', 'BGM 다시 재생', audio('play_bgm',asset='test_bgm',volume=35,loop=True)),
        ('effect', '효과음 (겹침 가능)', audio('play_se',asset='test_se',volume=70)),
        ('fade', 'BGM 2초 페이드 정지', audio('stop_bgm',fadeSeconds=2)),
        ('silence', '효과음 모두 정지', audio('stop_se')),
        ('script', 'Python 소리 호출', dict(type='call_script',script='audio_demo.py',function='run')),
    ]):
        objects.append(obj(key,name,50+index*110,[command,dict(type='log',text=name+' 실행')]))
    project = dict(formatVersion=1, name='BGM·효과음 확인', startScene='scene_001', scenes=['scene_001'],
                   assets=[asset('test_bgm',music,'테스트 BGM',source_name),asset('test_se','effect.wav','효과음')])
    scene = dict(formatVersion=1,id='scene_001',name='위쪽 객체를 조사해 소리 확인',width=640,height=360,
                 background='#202830',player='hero',objects=objects)
    for path, data in [('project.json',project),('scenes/scene_001.json',scene)]:
        (root/path).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (root/'scripts/audio_demo.py').write_text('''def run(ctx):
    if ctx.audio is None:
        ctx.log("소리는 Ctrl+R 게임 실행에서 확인하세요.")
        return
    ctx.audio.play_bgm("test_bgm", volume=35, loop=True)
    ctx.audio.play_se("test_se", volume=70)
    ctx.log("Python BGM·효과음 호출 완료")


def fade(ctx):
    if ctx.audio is not None:
        ctx.audio.stop_bgm(fade_seconds=2)


def stop_effects(ctx):
    if ctx.audio is not None:
        ctx.audio.stop_se()
''',encoding='utf-8')
    return root


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory')
    parser.add_argument('--bgm', type=Path)
    args = parser.parse_args()
    print(generate(args.directory, args.bgm))
