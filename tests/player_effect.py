"""Real player effect overlay and Python entry point."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from png_tools import pixels

source=Path(__file__).parents[1]/"docs/examples/animation"
with tempfile.TemporaryDirectory(prefix="naru-effect-") as temp:
    root=Path(temp)/"project";shutil.copytree(source,root)
    project_path=root/"project.json";project=json.loads(project_path.read_text(encoding="utf-8"))
    first={"image":"pose","x":0,"y":0,"width":32,"height":48,"seconds":.25}
    project["effects"]=[{"id":"flash","name":"회복빛","frames":[first,dict(first),dict(first,screenFlash=True,screenShake=True)]}]
    project_path.write_text(json.dumps(project,ensure_ascii=False),encoding="utf-8")
    (root/"scripts/animation_controls.py").write_text('''def on_scene_enter(ctx):
    token = ctx.effects.play("flash", "hero")
    assert ctx.effects.is_playing(token)
''',encoding="utf-8")
    shot=root/"shot.png"
    result=subprocess.run([sys.argv[1],str(root),"--frames","2","--screenshot",str(shot)],capture_output=True,text=True,encoding="utf-8",timeout=20)
    assert result.returncode==0,result.stdout+result.stderr
    assert "Traceback" not in result.stdout+result.stderr,result.stdout+result.stderr
    _,_,pixel=pixels(shot)
    assert pixel(120,140)==(255,210,40),pixel(120,140)
    result=subprocess.run([sys.argv[1],str(root),"--frames","35","--screenshot",str(shot)],capture_output=True,text=True,encoding="utf-8",timeout=20)
    assert result.returncode==0,result.stdout+result.stderr
    _,_,pixel=pixels(shot)
    assert pixel(10,10)[0]>60,pixel(10,10)
    print("[PASS] effect project load, Python play, target overlay pixels, screen flash")
