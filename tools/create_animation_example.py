"""캐릭터 동작 예제를 새 폴더에 생성. 기존 폴더는 덮어쓰지 않습니다."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def create(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=False)
    for name in ("assets","scenes","scripts"):(root/name).mkdir()
    def png(name,width,height,pixel):
        def chunk(tag,data):
            return struct.pack(">I",len(data))+tag+data+struct.pack(">I",zlib.crc32(tag+data))
        raw=b"".join(b"\0"+b"".join(bytes(pixel(x,y)) for x in range(width)) for y in range(height))
        data=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,6,0,0,0))+chunk(b"IDAT",zlib.compress(raw))+chunk(b"IEND",b"")
        (root/"assets"/name).write_bytes(data)
        return dict(id=Path(name).stem,name=name,kind="image",path="assets/"+name,sourceName=name,sha256=hashlib.sha256(data).hexdigest())
    colors=[(230,80,70),(70,200,100),(70,120,240)]
    assets=[png("sheet.png",96,128,lambda x,y:(*colors[x//32],255)),png("pose.png",32,48,lambda x,y:(255,210,40,255))]
    def frame(asset="sheet",x=0,y=0,width=32,height=32,seconds=.12,signal=""):
        return dict(asset=asset,x=x,y=y,width=width,height=height,seconds=seconds,anchorX=.5,anchorY=1,signal=signal)
    clips=[]
    for row,direction in enumerate(("down","left","right","up")):
        clips.append(dict(name="idle_"+direction,loop=True,frames=[frame(y=row*32)]))
        clips.append(dict(name="walk_"+direction,loop=True,frames=[frame(x=i*32,y=row*32) for i in range(3)]))
    clips += [dict(name="attack",loop=False,frames=[frame(x=i*32,signal="hit" if i==1 else "") for i in range(3)]),
              dict(name="cast",loop=False,frames=[frame(asset="pose",height=48,seconds=.4,signal="cast")]),
              dict(name="victory",loop=True,frames=[frame(),frame(asset="pose",height=48)]),
              dict(name="atlas",loop=True,frames=[frame(x=32,width=64,height=32,seconds=.3)])]
    script='''def on_game_start(ctx):
    ctx.input.bind("animation_attack", ["X"])
    ctx.input.bind("animation_cast", ["C"])
    ctx.input.bind("animation_stop", ["V"])
    ctx.input.bind("animation_repeat", ["B"])


def on_update(ctx):
    if ctx.scene is None:
        return
    hero = ctx.scene.object("hero")
    if ctx.input.pressed("animation_attack"):
        hero.play("attack", lock=True)
        ctx.log("공격 재생: 이전 동작을 교체했습니다.")
    if ctx.input.pressed("animation_cast"):
        hero.play("cast", lock=True)
        ctx.log("시전 재생: 이전 동작을 교체했습니다.")
    if ctx.input.pressed("animation_repeat"):
        hero.play("victory", loop=True, lock=True)
        ctx.log("반복 동작 시작: 이동이 잠겼습니다. V 또는 F8로 중지하세요.")
    if ctx.input.pressed("animation_stop"):
        active = hero.is_animating()
        hero.stop_animation()
        ctx.log("V 중지: " + ("재생을 중지하고 이동 잠금을 해제했습니다." if active else "중지할 명시적 동작이 없습니다."))
    for signal in ctx.scene.animation_signals():
        ctx.log("프레임 신호: " + signal["object"] + "/" + signal["clip"] + "/" + signal["signal"])
        ctx.variables.set("last_animation_signal", signal["signal"])


def run(ctx):
    if ctx.scene is None:
        ctx.log("Ctrl+R로 실행하세요. 방향키 이동, X 공격, C 시전, B 반복, V 중지입니다.")
        return
    hero = ctx.scene.object("hero")
    return hero.play("attack", lock=True)
'''
    (root/"scripts/animation_controls.py").write_text(script,encoding="utf-8")
    project=dict(formatVersion=1,name="캐릭터 애니메이션",startScene="stage",scenes=["stage"],assets=assets,globalScripts=["animation_controls.py"])
    scene=dict(formatVersion=1,id="stage",name="동작 확인",width=640,height=360,background="#202830",player="hero",objects=[dict(id="hero",name="동작 캐릭터",x=120,y=150,width=16,height=16,color="#ffcc44",image=dict(scaleX=2,scaleY=2),animations=clips),dict(id="sign",name="동작 이벤트",x=240,y=150,width=32,height=32,color="#887744",showName=True,event=dict(trigger="action",commands=[dict(type="object_animation",target="@player",animation="attack",wait=True,animationLock=True),dict(type="say",speaker="안내",text="공격 동작 완료 후 실행된 대사입니다.")]))])
    for name,value in [("project.json",project),("scenes/stage.json",scene)]: (root/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return root


if __name__=="__main__":
    print(create(sys.argv[1] if len(sys.argv)>1 else "build/manual-animation"))
