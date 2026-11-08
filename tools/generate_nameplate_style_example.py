"""Create a separate style demo, preserving any edited nameplate examples."""
import json
import sys
from generate_nameplate_example import generate


def generate_style(directory):
    root=generate(directory)
    manifest=root/'project.json'
    project=json.loads(manifest.read_text(encoding='utf-8'))
    project['name']='이름표 배경·위치 확인'
    manifest.write_text(json.dumps(project,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    file=root/'scenes/scene_001.json'
    scene=json.loads(file.read_text(encoding='utf-8'))
    scene['name']='투명 위쪽 / 색상 아래쪽 이름표'
    for obj in scene['objects']:
        if obj['id']=='npc':
            obj.update(nameBackgroundTransparent=True,namePosition='above',nameText='투명 배경 · 위')
        if obj['id']=='inn':
            obj.update(nameBackgroundColor='#8c3454',namePosition='below',nameText='선택한 배경색 · 아래')
        if obj['id']=='plain':
            obj.update(showName=True,nameBackgroundColor='#246847',namePosition='below',nameText='초록 배경 · 아래')
    file.write_text(json.dumps(scene,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    script=root/'scripts/nameplates.py'
    script.write_text(script.read_text(encoding='utf-8')+'''

def transparent_above(ctx):
    if ctx.scene is not None:
        npc = ctx.scene.object("npc")
        if npc is not None:
            npc.show_name = True
            npc.name_background_transparent = True
            npc.name_position = "above"


def colored_below(ctx):
    if ctx.scene is not None:
        npc = ctx.scene.object("npc")
        if npc is not None:
            npc.show_name = True
            npc.name_background_color = "#8c3454"
            npc.name_background_transparent = False
            npc.name_position = "below"
''',encoding='utf-8')
    return root


if __name__=='__main__':
    print(generate_style(sys.argv[1]))
