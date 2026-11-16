"""새 폴더에 레이어 타일맵 예제를 만듭니다. 기존 폴더는 덮어쓰지 않습니다."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def create(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=False)
    (root / "assets").mkdir()
    (root / "scenes").mkdir()
    (root / "scripts").mkdir()
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    colors = [(70, 130, 65, 255), (160, 115, 65, 255), (90, 100, 125, 255), (30, 90, 50, 255)]
    raw = b"".join(b"\0" + b"".join(bytes(colors[x // 32]) for x in range(128)) for _ in range(32))
    image = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 128,32,8,6,0,0,0)) + chunk(b"IDAT",zlib.compress(raw)) + chunk(b"IEND",b"")
    (root / "assets/tiles.png").write_bytes(image)
    project = dict(formatVersion=1,name="레이어 맵 예제",startScene="map",scenes=["map"],assets=[dict(id="tiles",name="예제 타일",kind="image",path="assets/tiles.png",sourceName="tiles.png",sha256=hashlib.sha256(image).hexdigest())])
    layers = [dict(name="1 바닥",asset="tiles",visible=True,locked=False,above=False,cells=[[y*64+x,0] for y in range(32) for x in range(64)],solidTiles=[]),
              dict(name="2 벽과 길",asset="tiles",visible=True,locked=False,above=False,cells=[[y*64+12,2] for y in range(4,20)],solidTiles=[2]),
              dict(name="3 지붕",asset="tiles",visible=True,locked=False,above=True,cells=[[y*64+x,3] for y in range(7,9) for x in range(4,9)],solidTiles=[])]
    scene = dict(formatVersion=1,id="map",name="넓은 마을",width=2048,height=1024,background="#202830",tileSize=32,cameraWidth=640,cameraHeight=360,tileLayers=layers,player="hero",objects=[dict(id="hero",name="플레이어",x=160,y=300,width=24,height=24,color="#ffcc44")])
    scene["objects"].append(dict(id="guide",name="카메라 안내원",x=900,y=500,width=24,height=24,color="#88bbff",showName=True,
        event=dict(trigger="action",commands=[dict(type="show_choices",speaker="안내원",text="화면 아래에 선택지가 고정되어 보이나요?",defaultChoice=0,cancelChoice=1,
            choices=[dict(text="네",commands=[dict(type="say",speaker="안내원",text="이름표는 맵을 따라가고 대사창은 화면에 고정됩니다.")]),dict(text="나중에 확인",commands=[])])])) )
    for name,value in [("project.json",project),("scenes/map.json",scene)]:
        (root/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return root


if __name__ == "__main__":
    print(create(sys.argv[1] if len(sys.argv)>1 else "build/manual-tilemap"))
