"""Generate an isolated DB image/audio example without overwriting a project."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def generate(destination):
    root = Path(destination)
    if root.exists() and any(p.name != "README.md" for p in root.iterdir()):
        raise FileExistsError("새 폴더를 지정하세요. 기존 자료를 덮어쓰지 않습니다.")
    repo = Path(__file__).resolve().parents[1]
    for sub in ("assets", "data", "scripts", "scenes"):
        (root / sub).mkdir(parents=True, exist_ok=True)
    assets = []
    for key, filename, kind, source in (
        ("db_picture", "picture.png", "image", repo / "docs/examples/assets/assets/sample.png"),
        ("db_sound", "effect.wav", "audio", repo / "docs/examples/audio/assets/effect.wav"),
    ):
        shutil.copyfile(source, root / "assets" / filename)
        assets.append(dict(id=key, name="DB 예제 " + kind, kind=kind, path="assets/" + filename,
                           sourceName=filename, sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    project = dict(formatVersion=1, name="DB 이미지·소리 연결", startScene="scene_001",
                   scenes=["scene_001"], dataTypes=["media"], assets=assets)
    data = dict(formatVersion=1, key="media", name="표현 자료", fields=[
        dict(key="picture", name="그림", type="image", default="db_picture"),
        dict(key="sound", name="효과음", type="audio", default=None),
    ], records=[dict(id="demo", name="그림과 소리 예제", values=dict(sound="db_sound")),
                dict(id="empty", name="기본 그림·소리 없음", values={})])
    scene = dict(formatVersion=1, id="scene_001", name="상자를 조사하면 DB 그림·소리 적용", width=640,
                 height=360, background="#202830", player="hero", objects=[
        dict(id="hero", name="주인공", x=80, y=180, width=40, height=40, color="#5fa7cf"),
        dict(id="display", name="Z로 조사", x=180, y=180, width=50, height=50, color="#dbb560",
             showName=True, event=dict(trigger="action", commands=[
                 dict(type="call_script", script="db_media.py", function="run")]))])
    for filename, content in (("project.json", project), ("data/media.json", data), ("scenes/scene_001.json", scene)):
        (root / filename).write_text(json.dumps(content, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (root / "scripts/db_media.py").write_text('''def run(ctx):
    media = ctx.db.get("media", "demo")
    ctx.log(str(media))
    if ctx.scene is None or ctx.audio is None:
        ctx.log("F5/F6은 DB 읽기만 확인합니다. 그림·소리는 Ctrl+R에서 확인하세요.")
        return
    obj = ctx.scene.object("display")
    obj.image = media["picture"] or ""
    if media["sound"] is not None:
        ctx.audio.play_se(media["sound"], volume=60)
''', encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination")
    generate(parser.parse_args().destination)
