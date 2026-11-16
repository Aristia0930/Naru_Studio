"""Real player: Python playback, frame pixels/signals, lock, fallback and event wait."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from png_tools import pixels

spec=importlib.util.spec_from_file_location("animation_example",Path(__file__).parents[1]/"tools/create_animation_example.py")
example=importlib.util.module_from_spec(spec);spec.loader.exec_module(example)
with tempfile.TemporaryDirectory(prefix="naru-animation-") as temporary:
    root=example.create(Path(temporary)/"project")
    original=(root/"scenes/stage.json").read_bytes()
    scene=json.loads(original);scene["objects"][0]["image"]={"scaleX":1,"scaleY":1}
    scene["objects"]=scene["objects"][:1]
    scenePath=root/"scenes/stage.json";script=root/"scripts/animation_controls.py"
    def render(mode,frames):
        scenePath.write_text(json.dumps(scene),encoding="utf-8")
        script.write_text('''def on_scene_enter(ctx):
    hero = ctx.scene.object("hero")
    try:
        hero.play("missing")
        raise AssertionError("missing clip must fail")
    except ValueError:
        pass
    assert hero.width == 16 and hero.height == 16
'''+('    hero.play("'+mode+'", lock=True)\n' if mode else '')+'''
def on_update(ctx):
    hero = ctx.scene.object("hero")
    if hero.is_animating():
        x = hero.x
        assert hero.move(10, 0) is False
        assert hero.x == x
    for signal in ctx.scene.animation_signals():
        ctx.log("signal:" + signal["signal"])
''',encoding="utf-8")
        shot=root/"shot.png"
        result=subprocess.run([sys.argv[1],str(root),"--frames",str(frames),"--screenshot",str(shot)],capture_output=True,text=True,encoding="utf-8",timeout=20)
        assert result.returncode==0,result.stdout+result.stderr
        assert "AssertionError" not in result.stdout+result.stderr,result.stdout+result.stderr
        _,_,pixel=pixels(shot);return pixel,result.stdout+result.stderr
    pixel,log=render("",2);assert pixel(120,150)==(230,80,70)
    pixel,log=render("attack",9);assert pixel(120,150)==(70,200,100),pixel(120,150);assert log.count("signal:hit")==1,log
    pixel,log=render("attack",25);assert pixel(120,150)==(230,80,70);assert log.count("signal:hit")==1
    pixel,log=render("cast",2);assert pixel(120,125)==(255,210,40),pixel(120,125)
    # Nonblocking autorun event waits for animation before the following log.
    script.write_text("",encoding="utf-8")
    scene["objects"][0]["event"]={"trigger":"parallel","commands":[{"type":"object_animation","target":"@self","animation":"attack","animationLock":True,"wait":True},{"type":"log","text":"animation-event-finished"}]}
    scenePath.write_text(json.dumps(scene),encoding="utf-8")
    result=subprocess.run([sys.argv[1],str(root),"--frames","30"],capture_output=True,text=True,encoding="utf-8",timeout=20)
    assert result.returncode==0 and "animation-event-finished" in result.stdout,result.stdout+result.stderr
    print("[PASS] animation frames, signal once, Python errors/lock, idle return, different size anchors, event wait")
