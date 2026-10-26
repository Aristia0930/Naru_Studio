"""Retain bounded, hashed source observations for the foundation report."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
scopes = {
    'refs/godot-4.7.2/source/scene/main/node.cpp': ['void Node::add_child('],
    'refs/godot-4.7.2/source/scene/main/scene_tree.cpp': ['bool SceneTree::physics_process(', 'bool SceneTree::process('],
    'refs/godot-4.7.2/source/scene/resources/packed_scene.cpp': ['Node *SceneState::instantiate(', 'Error PackedScene::pack(', 'Node *PackedScene::instantiate('],
    'refs/godot-4.7.2/source/core/object/object.cpp': ['Error Object::connect(', 'Error Object::emit_signalp('],
    'refs/godot-4.7.2/source/scene/2d/physics/character_body_2d.cpp': ['bool CharacterBody2D::move_and_slide('],
    'refs/godot-4.7.2/source/editor/inspector/editor_inspector.cpp': ['void EditorInspector::edit('],
    'refs/godot-4.7.2/source/editor/editor_undo_redo_manager.cpp': ['bool EditorUndoRedoManager::undo(', 'bool EditorUndoRedoManager::redo('],
    'refs/rpgmaker-mv-corescript/js/rpg_managers/DataManager.js': ['DataManager._databaseFiles =', 'DataManager.makeSaveContents ='],
    'refs/rpgmaker-mv-corescript/js/rpg_managers/BattleManager.js': ['BattleManager.update =', 'BattleManager.startTurn ='],
    'refs/rpgmaker-mv-corescript/js/rpg_objects/Game_Event.js': ['Game_Event.prototype.findProperPageIndex =', 'Game_Event.prototype.meetsConditions ='],
    'refs/rpgmaker-mv-corescript/js/rpg_objects/Game_Interpreter.js': ['Game_Interpreter.prototype.update =', 'Game_Interpreter.prototype.updateWaitMode =', 'Game_Interpreter.prototype.executeCommand ='],
}
records = []
for relative, needles in scopes.items():
    data = (root / relative).read_bytes()
    lines = data.decode('utf-8-sig').splitlines()
    for needle in needles:
        index = next(i for i, line in enumerate(lines) if needle in line)
        records.append({'path': relative, 'sha256': hashlib.sha256(data).hexdigest(), 'symbol': needle,
                        'line_start': index + 1, 'line_end': min(index + 55, len(lines)),
                        'excerpt': '\n'.join(lines[index:index + 55])})
output = root / 'analysis/foundation/source-observations.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps({'scope': 'Bounded source observations; not whole-program or runtime coverage', 'observations': records}, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'observations': len(records), 'output': str(output)}, ensure_ascii=True))
