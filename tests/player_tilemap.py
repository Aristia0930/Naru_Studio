"""실제 SDL 화면에서 타일 레이어 순서와 카메라 오프셋을 검사한다."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

spec = importlib.util.spec_from_file_location("tile_example", Path(__file__).parents[1]/"tools/create_tilemap_example.py")
example = importlib.util.module_from_spec(spec)
spec.loader.exec_module(example)


def pixels(path):
    data=path.read_bytes();offset=8;compressed=b""
    while offset<len(data):
        length=struct.unpack_from(">I",data,offset)[0];tag=data[offset+4:offset+8];payload=data[offset+8:offset+8+length];offset+=length+12
        if tag==b"IHDR":
            width,height,bits,kind,_,_,interlace=struct.unpack(">IIBBBBB",payload)
            assert bits==8 and kind in (2,6) and interlace==0
        if tag==b"IDAT":compressed+=payload
    raw=zlib.decompress(compressed);channels=4 if kind==6 else 3;stride=width*channels;previous=bytearray(stride);rows=[];offset=0
    for _ in range(height):
        method=raw[offset];row=bytearray(raw[offset+1:offset+1+stride]);offset+=stride+1
        for x in range(stride):
            a=row[x-channels] if x>=channels else 0;b=previous[x];c=previous[x-channels] if x>=channels else 0
            p=a+b-c;choices=(a,b,c);paeth=choices[min(range(3),key=lambda i:abs(p-choices[i]))]
            row[x]=(row[x]+(0,a,b,(a+b)//2,paeth)[method])%256
        rows.append(row);previous=row
    return width,height,lambda x,y:tuple(rows[y][x*channels:x*channels+3])


with tempfile.TemporaryDirectory(prefix="naru-tiles-") as temporary:
    root=example.create(Path(temporary)/"project")
    scene_path=root/"scenes/map.json";scene=json.loads(scene_path.read_text(encoding="utf-8"))
    scene["objects"][0].update(x=800,y=500)
    # camera origin: (492, 332), hero screen: (308,168)
    cell=15*64+25
    scene["tileLayers"][2]["cells"]=[[cell,3],[cell+64,3]]
    def render():
        scene_path.write_text(json.dumps(scene,ensure_ascii=False),encoding="utf-8")
        shot=root/"shot.png"
        result=subprocess.run([sys.argv[1],str(root),"--frames","2","--screenshot",str(shot)],capture_output=True,text=True,encoding="utf-8",timeout=20)
        assert result.returncode==0,result.stdout+result.stderr
        width,height,pixel=pixels(shot);assert (width,height)==(640,360),(width,height)
        return pixel
    pixel=render();assert pixel(310,170)==(30,90,50),pixel(310,170)
    assert pixel(320,186)==(30,90,50),pixel(320,186)
    assert pixel(50,50)==(70,130,65)
    scene["tileLayers"][2]["above"]=False
    pixel=render();assert pixel(310,170)==(255,204,68),pixel(310,170)
    scene["tileLayers"][2]["above"]=True;scene["tileLayers"][2]["visible"]=False
    pixel=render();assert pixel(310,170)==(255,204,68)
    print("[PASS] player tile layers below/above/hidden and camera world-to-screen coordinates")
