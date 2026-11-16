"""Build an isolated effect project from the bundled character animation example."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import wave


def create(destination):
    destination=Path(destination)
    source=Path(__file__).parents[1]/"docs/examples/animation"
    shutil.copytree(source,destination)
    sound=destination/"assets/effect.wav"
    with wave.open(str(sound),"wb") as out:
        out.setnchannels(1);out.setsampwidth(2);out.setframerate(22050)
        out.writeframes(b"".join(struct.pack("<h",int(9000*(1-i/4400))) for i in range(4400)))
    project_path=destination/"project.json";project=json.loads(project_path.read_text(encoding="utf-8"))
    project["name"]="이펙트 애니메이션"
    project["scenes"]=["stage","other"]
    project["assets"].append(dict(id="effect_sound",name="effect.wav",kind="audio",path="assets/effect.wav",sourceName="effect.wav",sha256=hashlib.sha256(sound.read_bytes()).hexdigest()))
    project["effects"]=[dict(id="spark",name="불꽃 이펙트",frames=[
        dict(image="sheet",x=i*32,y=0,width=32,height=32,seconds=1.0,offsetX=0,offsetY=-28,scale=2,rotation=0,opacity=1,blend="normal",sound="effect_sound" if i==1 else "",targetFlash=i==1,screenFlash=i==2,screenShake=i==2)
        for i in range(3)])]
    project_path.write_text(json.dumps(project,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    scene_path=destination/"scenes/stage.json";scene=json.loads(scene_path.read_text(encoding="utf-8"))
    scene["objects"][1]["event"]["commands"].insert(1,dict(type="show_effect",effect="spark",target="@player",wait=True))
    scene["objects"].append(dict(id="portal",name="다른 장면",x=330,y=150,width=32,height=32,color="#6688aa",showName=True,event=dict(trigger="action",commands=[dict(type="change_scene",scene="other",setPosition=False)])))
    scene_path.write_text(json.dumps(scene,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    other=dict(scene,id="other",name="이펙트 정리 확인",player="hero_other",objects=[dict(scene["objects"][0],id="hero_other",x=120,y=150),dict(id="return_portal",name="돌아가기",x=240,y=150,width=32,height=32,color="#6688aa",showName=True,event=dict(trigger="action",commands=[dict(type="change_scene",scene="stage",setPosition=False)]))])
    (destination/"scenes/other.json").write_text(json.dumps(other,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    (destination/"scripts/animation_controls.py").write_text('''effect_token = None

def on_game_start(ctx):
    ctx.input.bind("effect_play", ["E"])
    ctx.input.bind("effect_stop", ["R"])

def on_scene_enter(ctx):
    if ctx.scene.id == "stage":
        ctx.effects.play("spark", "hero")

def on_update(ctx):
    global effect_token
    if ctx.scene is None:
        return
    if ctx.input.pressed("effect_play"):
        effect_token = ctx.effects.play("spark", "@player")
        ctx.log("이펙트 시작: " + str(effect_token))
    if ctx.input.pressed("effect_stop") and effect_token is not None:
        ctx.effects.stop(effect_token)
        ctx.log("이펙트 중지")
        effect_token = None
''',encoding="utf-8")
    return destination


if __name__=="__main__":
    print(create(sys.argv[1] if len(sys.argv)>1 else "build/manual-effect"))
