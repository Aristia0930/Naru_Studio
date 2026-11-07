"""Render the real SDL player and inspect PNG pixels without third-party Python packages."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def png(path, width, height, pixels):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    raw = b''.join(b'\0' + bytes(pixels[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                    + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def read_png(path):
    data = path.read_bytes()
    offset, compressed = 8, b''
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        tag, payload = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        if tag == b'IHDR':
            width, height, bits, kind, _, _, interlace = struct.unpack('>IIBBBBB', payload)
            assert bits == 8 and kind in (2, 6) and interlace == 0
        elif tag == b'IDAT':
            compressed += payload
        offset += 12 + size
    channels = 4 if kind == 6 else 3
    stride = width * channels
    raw = zlib.decompress(compressed)
    rows, previous, offset = [], bytearray(stride), 0
    for _ in range(height):
        method = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += stride + 1
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            above, upper_left = previous[x], previous[x - channels] if x >= channels else 0
            if method == 0:
                value = 0
            elif method == 1:
                value = left
            elif method == 2:
                value = above
            elif method == 3:
                value = (left + above) // 2
            else:
                assert method == 4
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                value = (left, above, upper_left)[distances.index(min(distances))]
            row[x] = (row[x] + value) % 256
        rows.append(row)
        previous = row
    return lambda x, y: tuple(rows[y][x * channels:x * channels + 3])


with tempfile.TemporaryDirectory(prefix='naru-images-') as temporary:
    root = Path(temporary)
    (root / 'assets').mkdir()
    (root / 'scenes').mkdir()
    image = root / 'assets/picture.png'
    png(image, 4, 2, [255, 0, 0, 255] * 2 + [0, 255, 0, 255] * 2
        + [0, 0, 255, 128] * 2 + [255, 255, 0, 0] * 2)
    project = dict(formatVersion=1, name='images', scenes=['scene_001'], startScene='scene_001',
                   assets=[dict(id='picture', name='picture', kind='image', path='assets/picture.png',
                                sourceName='picture.png', sha256=hashlib.sha256(image.read_bytes()).hexdigest())])
    manifest = root / 'project.json'
    manifest.write_text(json.dumps(project), encoding='utf-8')

    def render(objects=(), background=None):
        scene = dict(formatVersion=1, id='scene_001', name='images', width=320, height=180,
                     background='#101820', objects=list(objects))
        if background:
            scene['backgroundImage'] = background
        (root / 'scenes/scene_001.json').write_text(json.dumps(scene), encoding='utf-8')
        before = manifest.read_bytes()
        shot = root / 'shot.png'
        result = subprocess.run([sys.argv[1], str(root), '--frames', '2', '--screenshot', str(shot)],
                                capture_output=True, encoding='utf-8', timeout=12)
        assert result.returncode == 0, result.stdout + result.stderr
        assert manifest.read_bytes() == before
        return read_png(shot), result.stdout

    obj = dict(id='sprite', name='sprite', x=40, y=40, width=8, height=8, color='#ff00ff',
               image=dict(asset='picture', width=40, height=20))
    pixel, _ = render([obj])
    assert pixel(45, 45) == (255, 0, 0) and pixel(75, 45) == (0, 255, 0)
    assert pixel(75, 55) == (16, 24, 32), 'transparent PNG must reveal scene color'
    assert 6 <= pixel(45, 55)[0] <= 9 and 140 <= pixel(45, 55)[2] <= 145, pixel(45, 55)
    obj['image']['flipX'] = True
    pixel, _ = render([obj])
    assert pixel(45, 45) == (0, 255, 0)
    obj['image'].update(flipX=False, flipY=True)
    pixel, _ = render([obj])
    assert pixel(45, 55) == (255, 0, 0)
    obj['image'].update(flipY=False, opacity=0)
    pixel, _ = render([obj])
    assert pixel(45, 45) == (16, 24, 32)
    obj['image'].update(opacity=1, anchorX=.5, anchorY=.5, rotation=180)
    pixel, _ = render([obj])
    assert pixel(45, 45) == (255, 0, 0), pixel(45, 45)
    obj['image'].update(anchorX=0, anchorY=0, rotation=0, order=2)
    behind = dict(id='behind', name='behind', x=40, y=40, width=40, height=20, color='#ffffff')
    pixel, _ = render([obj, behind])
    assert pixel(45, 45) == (255, 0, 0), 'draw order must override object list order'
    obj['visible'] = False
    pixel, _ = render([obj])
    assert pixel(45, 45) == (16, 24, 32)
    for mode in ('fit', 'fill', 'stretch', 'original', 'repeat'):
        pixel, _ = render(background=dict(asset='picture', mode=mode))
        if mode == 'fit':
            assert pixel(10, 3) == (16, 24, 32) and pixel(10, 20) == (255, 0, 0)
        elif mode == 'original':
            assert pixel(0, 0) == (255, 0, 0) and pixel(10, 10) == (16, 24, 32)
        elif mode == 'repeat':
            assert pixel(4, 2) == (255, 0, 0) and pixel(6, 2) == (0, 255, 0)
        else:
            assert pixel(10, 10) == (255, 0, 0)
    obj['visible'] = True
    obj['image']['asset'] = 'missing'
    pixel, logs = render([obj], dict(asset='missing', mode='fit'))
    assert pixel(41, 41) == (255, 0, 255) and pixel(10, 10) == (16, 24, 32)
    assert logs.count('missing') == 1, logs
    (root / 'scripts').mkdir()
    (root / 'scripts/switch.py').write_text('''def on_scene_enter(ctx):
    ctx.scene.object("sprite").image = "picture"
    ctx.scene.background_image = "picture"
''', encoding='utf-8')
    project['globalScripts'] = ['switch.py']
    manifest.write_text(json.dumps(project), encoding='utf-8')
    obj['image']['asset'] = ''
    pixel, logs = render([obj], dict(mode='fit'))
    assert pixel(45, 45) == (255, 0, 0) and pixel(10, 20) == (255, 0, 0)
    print('[PASS] real player PNG alpha, flips, rotation, opacity, order, hidden, five backgrounds, missing fallback, source preservation')
