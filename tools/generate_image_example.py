"""Create original geometric pixel-art fixtures; never overwrite an existing project."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zlib


def write_png(path, width, height, pixel):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    raw = b''.join(b'\0' + bytes(c for x in range(width) for c in pixel(x, y)) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def create(folder):
    root = Path(folder)
    if root.exists():
        raise SystemExit('Choose a new folder; existing projects are never overwritten.')
    for directory in ('assets', 'scenes', 'scripts'):
        (root / directory).mkdir(parents=True)

    def landscape(x, y, night=False):
        sky, mountain, grass, road = ((32, 45, 82, 255), (45, 69, 89, 255), (35, 83, 70, 255), (87, 83, 91, 255)) if night else (
            (112, 187, 224, 255), (80, 137, 155, 255), (102, 166, 110, 255), (213, 190, 145, 255))
        if (x - 265) ** 2 + (y - 28) ** 2 < 12 ** 2:
            return (245, 235, 180, 255)
        if y > 85 + abs(x - 110) // 3 and y < 120:
            return mountain
        if y >= 120:
            if abs(x - 100 - (y - 120) // 2) < 23:
                return road
            return grass
        return sky

    def hero(x, y):
        if 8 <= x < 24 and 4 <= y < 20:
            return (60, 68, 90, 255) if y < 8 else (246, 209, 157, 255)
        if 6 <= x < 26 and 20 <= y < 36:
            return (64, 105, 179, 255)
        if 8 <= x < 14 and 36 <= y < 46 or 18 <= x < 24 and 36 <= y < 46:
            return (48, 57, 79, 255)
        return (0, 0, 0, 0)

    def tree(x, y):
        if 27 <= x < 37 and 48 <= y < 94:
            return (120, 82, 54, 255)
        if (x - 32) ** 2 / 30 ** 2 + (y - 35) ** 2 / 32 ** 2 < 1:
            return (47, 120, 77, 190 if x < 8 or x > 56 else 255)
        return (0, 0, 0, 0)

    for filename, width, height, pixel in [('day.png', 320, 180, landscape), ('night.png', 320, 180, lambda x, y: landscape(x, y, True)),
                                            ('hero.png', 32, 48, hero), ('tree.png', 64, 96, tree)]:
        write_png(root / 'assets' / filename, width, height, pixel)
    assets = []
    for key, name, group in [('day', '낮 풍경', '배경'), ('night', '밤 풍경', '배경'), ('hero', '주인공', '캐릭터'), ('tree', '나무', '장식')]:
        assets.append(dict(id=key, name=name, kind='image', group=group, path=f'assets/{key}.png', sourceName=f'{key}.png',
                           sha256=hashlib.sha256((root / f'assets/{key}.png').read_bytes()).hexdigest()))
    project = dict(formatVersion=1, name='객체 이미지와 장면 배경', startScene='scene_001', scenes=['scene_001'],
                   assets=assets, assetGroups=['배경', '캐릭터', '장식'])
    sprite = dict(asset='hero', width=64, height=96, offsetX=12, offsetY=16, anchorX=.5, anchorY=1, order=1)
    scene = dict(formatVersion=1, id='scene_001', name='그림과 충돌 영역', width=640, height=360, background='#202830',
                 backgroundImage=dict(asset='day', mode='fit'), player='hero', objects=[
                     dict(id='hero', name='주인공', x=145, y=260, width=24, height=16, image=sprite),
                     dict(id='tree', name='나무', x=350, y=240, width=36, height=18,
                          image=dict(asset='tree', width=128, height=192, offsetX=18, offsetY=18, anchorX=.5, anchorY=1, order=2),
                          event=dict(trigger='action', commands=[dict(type='say', text='나무 그림과 발밑 충돌 영역은 따로입니다. 배경도 바꿔 볼까요?'),
                                                                 dict(type='call_script', script='appearance.py', function='toggle_background')]))])
    (root / 'scripts/appearance.py').write_text('''# 이벤트의 스크립트 호출에 연결. 전역 등록은 필요하지 않습니다.
def toggle_background(ctx):
    if ctx.scene is None:
        ctx.log("배경 변경은 Ctrl+R 실제 게임에서 확인하세요.")
        return False
    if ctx.scene.background_image == "day":
        ctx.scene.background_image = "night"
    else:
        ctx.scene.background_image = "day"
    return True


def hide_tree_picture(ctx):
    if ctx.scene is not None:
        ctx.scene.object("tree").image = ""


def restore_tree_picture(ctx):
    if ctx.scene is not None:
        ctx.scene.object("tree").image = "tree"
''', encoding='utf-8')
    for path, data in [(root / 'project.json', project), (root / 'scenes/scene_001.json', scene)]:
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    create(sys.argv[1])
