"""Generate original, dependency-free asset fixtures in a NEW example folder.

Usage: python tools/generate_asset_example.py <new-folder>
The simple geometric PNG and tone WAV are authored test data, not game assets.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import wave
import zlib


def create(folder):
    folder = Path(folder)
    if folder.exists():
        raise SystemExit("Choose a new folder; existing examples are never overwritten.")
    (folder / "assets").mkdir(parents=True)
    (folder / "scenes").mkdir()
    width, height = 192, 128
    pixels = bytearray()
    for y in range(height):
        pixels.append(0)
        for x in range(width):
            inside = 24 <= x < 168 and 20 <= y < 108
            edge = min(x - 24, 167 - x, y - 20, 107 - y)
            color = (74, 144, 217, 180 if edge < 8 else 255) if inside else (0, 0, 0, 0)
            if 68 <= x < 124 and 42 <= y < 86:
                color = (248, 210, 100, 220)
            pixels.extend(color)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b"")
    (folder / "assets" / "sample.png").write_bytes(png)
    with wave.open(str(folder / "assets" / "tone.wav"), "wb") as sound:
        sound.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
        sound.writeframes(b"".join(struct.pack("<h", round(5000 * math.sin(2 * math.pi * 440 * i / 16000)
            * min(1, i / 400, (8000 - i) / 400))) for i in range(8000)))
    assets = []
    for filename, name, kind in [("sample.png", "반투명 그림 예제", "image"), ("tone.wav", "미리듣기 예제", "audio")]:
        assets.append(dict(id="asset_" + kind, name=name, kind=kind, path="assets/" + filename,
            sourceName=filename, sha256=hashlib.sha256((folder / "assets" / filename).read_bytes()).hexdigest(),
            group="그림" if kind == "image" else "소리"))
    project = dict(formatVersion=1, name="에셋 목록 예제", startScene="scene_001", scenes=["scene_001"],
        dataTypes=[], globalScripts=[], variables=[], commonEvents=[], assets=assets,
        assetGroups=["그림", "소리", "빈 그룹 예제"])
    scene = dict(formatVersion=1, id="scene_001", name="시작 장면", width=1280, height=720,
        background="#202830", objects=[])
    for path, data in [(folder / "project.json", project), (folder / "scenes" / "scene_001.json", scene)]:
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    create(sys.argv[1])
