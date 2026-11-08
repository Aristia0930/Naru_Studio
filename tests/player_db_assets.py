"""Read DB asset fields in the actual player and apply the returned IDs."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

player, root = Path(sys.argv[1]), Path(sys.argv[2])
spec = importlib.util.spec_from_file_location("db_generator", root / "tools/generate_database_asset_example.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
with tempfile.TemporaryDirectory(prefix="naru-db-assets-") as temporary:
    project = Path(temporary) / "project"
    generator.generate(project)
    scene_file = project / "scenes/scene_001.json"
    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    scene["objects"][1]["event"]["trigger"] = "autorun"
    scene["objects"][1]["event"]["commands"].append(dict(type="log", text="DB_ASSET_COMPLETE"))
    scene_file.write_text(json.dumps(scene), encoding="utf-8")
    original = {p: p.read_bytes() for p in (project / "project.json", project / "data/media.json", scene_file)}
    env = dict(os.environ, SDL_AUDIO_DRIVER="dummy")

    def run(expected):
        log = Path(temporary) / "game.log"
        result = subprocess.run([str(player), str(project), "--frames", "5", "--log-file", str(log)],
                                env=env, capture_output=True, timeout=20)
        text = log.read_text(encoding="utf-8")
        assert result.returncode == expected, (result.returncode, result.stderr, text)
        return text

    text = run(0)
    assert "DB_ASSET_COMPLETE" in text and "db_picture" in text and "db_sound" in text
    assert all(p.read_bytes() == data for p, data in original.items()), "runtime changed project data"
    script = project / "scripts/db_media.py"
    script.write_text(script.read_text(encoding="utf-8").replace('"demo"', '"empty"'), encoding="utf-8")
    (project / "assets/effect.wav").unlink()
    assert "DB_ASSET_COMPLETE" in run(0)
    script.write_text(script.read_text(encoding="utf-8").replace('"empty"', '"demo"'), encoding="utf-8")
    text = run(3)
    assert "DB_ASSET_COMPLETE" not in text and "db_media.py" in text and "db_sound" in text
    print("[PASS] DB defaults/None, actual player image/audio, missing-file traceback, source preserved")
