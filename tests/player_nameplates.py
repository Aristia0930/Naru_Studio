"""Compare actual SDL nameplate pixels, F9 transitions, and runtime script changes."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import struct
import zlib


def pixels(data):
    offset=8; compressed=bytearray(); width=height=channels=0
    while offset<len(data):
        size=struct.unpack('!I',data[offset:offset+4])[0]
        kind=data[offset+4:offset+8]; block=data[offset+8:offset+8+size]; offset+=size+12
        if kind==b'IHDR':
            width,height,depth,mode,*_=struct.unpack('!2I5B',block)
            assert depth==8 and mode in (2,6)
            channels=3 if mode==2 else 4
        if kind==b'IDAT': compressed.extend(block)
    decoded=zlib.decompress(compressed); stride=width*channels; rows=[]; previous=bytearray(stride)
    for y in range(height):
        start=y*(stride+1); method=decoded[start]; row=bytearray(decoded[start+1:start+1+stride])
        for x in range(stride):
            left=row[x-channels] if x>=channels else 0; above=previous[x]; upper=previous[x-channels] if x>=channels else 0
            if method==0: value=0
            elif method==1: value=left
            elif method==2: value=above
            elif method==3: value=(left+above)//2
            else:
                assert method==4
                p=left+above-upper; distances=(abs(p-left),abs(p-above),abs(p-upper)); value=(left,above,upper)[distances.index(min(distances))]
            row[x]=(row[x]+value)%256
        rows.append(row); previous=row
    return lambda x,y: tuple(rows[y][x*channels:x*channels+3])

player = Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='naru-names-') as temporary:
    root=Path(temporary)
    (root/'scenes').mkdir()
    (root/'scripts').mkdir()
    manifest=root/'project.json'
    project=dict(formatVersion=1,name='names',startScene='scene_001',scenes=['scene_001'])
    manifest.write_text(json.dumps(project),encoding='utf-8')
    npc=dict(id='npc',name='여관 주인',x=150,y=140,width=30,height=30,color='#5070a0')
    def render(objects, args=(), scripts=()):
        project['globalScripts']=list(scripts)
        manifest.write_text(json.dumps(project),encoding='utf-8')
        scene=dict(formatVersion=1,id='scene_001',name='names',width=400,height=260,objects=objects,background='#101820')
        file=root/'scenes/scene_001.json'
        file.write_text(json.dumps(scene),encoding='utf-8')
        before=manifest.read_bytes(),file.read_bytes()
        shot=root/'shot.png'
        result=subprocess.run([str(player),str(root),'--frames','6','--screenshot',str(shot),*args],
                              env=dict(os.environ,SDL_AUDIO_DRIVER='dummy'),capture_output=True,timeout=15)
        assert result.returncode==0,(result.returncode,result.stderr)
        assert before==(manifest.read_bytes(),file.read_bytes()),'runtime modified editor files'
        return shot.read_bytes()
    off=render([npc])
    debug=render([npc],['--debug-object-names'])
    assert off!=debug
    assert render([npc],['--hold-key','F9:0-5'])==debug,'held F9 toggled repeatedly'
    assert render([npc],['--debug-object-names','--hold-key','F9:0-5'])==off
    assert render([npc],['--hold-key','F9:0-0','--hold-key','F9:2-2'])==off
    game=render([dict(npc,showName=True)])
    assert game!=off and game!=debug
    assert render([dict(npc,showName=True)],['--debug-object-names','--hold-key','F9:0-5'])==game
    custom=render([dict(npc,showName=True,nameText='안내원')])
    assert custom!=game
    transparent=render([dict(npc,showName=True,nameBackgroundTransparent=True)])
    red=render([dict(npc,showName=True,nameBackgroundColor='#ff0000')])
    below=render([dict(npc,showName=True,namePosition='below')])
    assert len({game,transparent,red,below})==4
    get_red=pixels(red); get_clear=pixels(transparent); get_below=pixels(below); get_above=pixels(game)
    red_background=[(x,y) for y in range(90,134) for x in range(80,250) if get_red(x,y)[0]>180 and get_red(x,y)[1]<15 and get_red(x,y)[2]<15]
    assert red_background,'chosen red background missing'
    assert any(get_clear(x,y)==(16,24,32) for x,y in red_background),'transparent background still filled'
    assert any(get_below(x,y)!=(16,24,32) for y in range(176,225) for x in range(80,250))
    assert all(get_above(x,y)==(16,24,32) for y in range(176,225) for x in range(80,250))
    assert render([dict(npc,showName=True,namePosition='below',nameBackgroundTransparent=True)],['--debug-object-names'])==debug
    assert render([dict(npc,showName=False,nameText='안내원')])==off
    hidden=dict(npc,visible=False,showName=True)
    assert render([hidden])==render([])
    assert render([hidden],['--debug-object-names'])!=render([]),'debug should identify hidden objects'
    # First frame creates the old label cache, later frames must refresh the text.
    (root/'scripts/names.py').write_text('''ticks=0
def on_update(ctx):
    global ticks
    ticks += 1
    if ticks == 2:
        obj=ctx.scene.object("npc")
        obj.show_name=True
        obj.name_text="안내원"
''',encoding='utf-8')
    assert render([npc],scripts=['names.py'])==custom
    (root/'scripts/style.py').write_text('''ticks=0
def on_update(ctx):
    global ticks
    ticks += 1
    if ticks == 2:
        obj=ctx.scene.object("npc")
        obj.name_position="below"
        obj.name_background_color="#ff0000"
        obj.name_background_transparent=True
''',encoding='utf-8')
    assert render([dict(npc,showName=True)],scripts=['style.py'])==render([dict(npc,showName=True,namePosition='below',nameBackgroundColor='#ff0000',nameBackgroundTransparent=True)])
    (root/'scripts/move.py').write_text('''ticks=0
def on_update(ctx):
    global ticks
    ticks += 1
    if ticks == 2:
        ctx.scene.object("npc").x=190
''',encoding='utf-8')
    assert render([dict(npc,showName=True)],scripts=['move.py'])==render([dict(npc,showName=True,x=190)]),'nameplate did not follow object movement'
    assert render([dict(npc,showName=True,nameText='가'*150)])
    assert render([dict(npc,showName=True,x=0,y=0)])
    # Debug names include IDs even when game-facing labels use the same custom text.
    assert render([dict(npc,id='npc2')],['--debug-object-names'])!=debug
    print('[PASS] actual nameplate pixels: default off, F9 held/edges, game/custom/hidden labels, Python refresh, IDs, bounds, source preservation')
