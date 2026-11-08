"""Create a new nameplate demo without changing existing projects."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def generate(directory):
    root=Path(directory)
    if (root/'project.json').exists():
        raise FileExistsError('기존 프로젝트를 덮어쓰지 않습니다. 새 폴더를 지정하세요.')
    for folder in ('scenes','scripts','assets'):
        (root/folder).mkdir(parents=True,exist_ok=True)
    width,height=32,64
    def pixel(x,y):
        if 8<=x<24 and 2<=y<20: return bytes((240,195,150,255))
        if 5<=x<27 and 20<=y<49: return bytes((60,135,200,255))
        if (7<=x<14 or 18<=x<25) and 49<=y<64: return bytes((55,65,80,255))
        return bytes(4)
    raw=b''.join(b'\0'+b''.join(pixel(x,y) for x in range(width)) for y in range(height))
    def chunk(kind,data):
        return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
    image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
    (root/'assets/npc.png').write_bytes(image)
    def object(key,name,x,y,**extra):
        return dict(id=key,name=name,x=x,y=y,width=32,height=32,color='#4778a8',**extra)
    objects=[object('hero','주인공',80,380),
             object('npc','편집용 NPC 이름',180,265,showName=True,nameText='여관 주인',image=dict(asset='npc_sprite',width=48,height=96,anchorX=.5,anchorY=1,offsetX=16,offsetY=32),
                    event=dict(trigger='action',commands=[dict(type='call_script',script='nameplates.py',function='run')])),
             object('inn','여관',465,200,showName=True),
             object('plain','이름표 없는 객체',600,340),
             object('hidden','숨긴 진행 객체',650,80,visible=False,showName=True)]
    project=dict(formatVersion=1,name='객체 이름표 확인',startScene='scene_001',scenes=['scene_001'],
                 assets=[dict(id='npc_sprite',name='이름표 확인 캐릭터',kind='image',path='assets/npc.png',sourceName='npc.png',sha256=hashlib.sha256(image).hexdigest())])
    scene=dict(formatVersion=1,id='scene_001',name='F9 전체 이름·ID / NPC 조사',width=800,height=480,player='hero',background='#202830',objects=objects)
    for path,data in [('project.json',project),('scenes/scene_001.json',scene)]:
        (root/path).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (root/'scripts/nameplates.py').write_text('''def run(ctx):
    if ctx.scene is None:
        ctx.log("이름표 변경은 Ctrl+R 게임 실행에서 확인하세요.")
        return
    npc = ctx.scene.object("npc")
    if npc is None:
        ctx.warning("npc 객체가 없습니다.")
        return
    npc.show_name = True
    npc.name_text = "어서 오세요" if npc.name_text != "어서 오세요" else "여관 주인"
    ctx.log("NPC 이름표: " + npc.name_text)


def hide_name(ctx):
    if ctx.scene is not None:
        npc = ctx.scene.object("npc")
        if npc is not None:
            npc.show_name = False


def use_object_name(ctx):
    if ctx.scene is not None:
        npc = ctx.scene.object("npc")
        if npc is not None:
            npc.show_name = True
            npc.name_text = ""
''',encoding='utf-8')
    return root


if __name__=='__main__':
    print(generate(sys.argv[1]))
