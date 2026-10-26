"""Sync the reviewed handoff additions to the already verified new workspace."""
import hashlib
import json
from pathlib import Path
import shutil

SOURCE = Path(__file__).resolve().parents[1]
TARGET = SOURCE.parent.parent / 'game_tool'

def sha(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()

def main():
    target = TARGET.resolve()
    if target.name != 'game_tool' or target != (SOURCE.resolve().parent.parent / 'game_tool').resolve():
        raise RuntimeError('Unexpected target')
    initial_path = target / 'MIGRATION_MANIFEST.json'
    initial = json.loads(initial_path.read_text(encoding='utf-8'))
    if initial['status'] != 'copied-and-sha256-verified':
        raise RuntimeError('Missing verified initial transfer')
    baseline = {item['path']: item['sha256'] for item in initial['files']}
    previous = target / 'HANDOFF_MANIFEST.json'
    if previous.exists():
        baseline.update({item['path']: item['sha256'] for item in json.loads(previous.read_text(encoding='utf-8'))['files']})
    paths = ['README.md', 'SESSION_STATE.md', 'NARU_NEXT_STEPS.md', 'ENGINE_TOOL_PLAN.md',
             'TOOL_FOUNDATION.md', 'REPOSITORY_AGENTS.md', '.gitignore',
             'tools/acquire_unity_reference.py', 'tools/finalize_naru_handoff.py']
    for folder in ['analysis/unity', 'refs/unity-csreference']:
        paths.extend(path.relative_to(SOURCE).as_posix() for path in (SOURCE / folder).rglob('*') if path.is_file())
    entries = [(SOURCE / path, path) for path in sorted(set(paths))]
    entries.append((SOURCE / 'REPOSITORY_AGENTS.md', 'AGENTS.md'))
    for original, relative in entries:
        destination = target / relative
        if destination.exists() and relative in baseline and sha(destination) != baseline[relative]:
            raise RuntimeError('Destination was edited independently: ' + relative)
    changes = []
    for original, relative in entries:
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(original), str(destination))
        expected = sha(original)
        if sha(destination) != expected:
            raise RuntimeError('Hash mismatch: ' + relative)
        changes.append({'path': relative, 'sha256': expected, 'bytes': destination.stat().st_size})
    status = '''# 2026-10-27 handoff complete

Development root: this game_tool / Naru_Studio repository.
Confirmed architecture: independent C++ engine, Qt editor UI, Python game scripts.

Initial transfer copied 548 files and verified SHA-256 for every copied file.
Unity REA metadata/type observations and public-reference-source research were added afterward.
MIGRATION_MANIFEST.json records the initial snapshot; HANDOFF_MANIFEST.json records the additions.

Start with README.md, AGENTS.md, NARU_NEXT_STEPS.md and SESSION_STATE.md.
The C++/Qt/Python product is not implemented yet. Next: check the development environment,
pin compatible versions, then connect a minimal CMake core test, Qt window and Python embedding.
app/ is the older Godot experiment only (39 passing checks), not the product engine.

Research includes RPG Maker, Godot and Unity, with raw REA evidence and stated coverage limits.
Local Unity reference-source text is excluded from Git; its acquisition manifest and downloader
allow another PC to retrieve the pinned public source. Original Unity binaries are not copied.
Godot research binaries are local-only and excluded from Git. Ghidra installation stays at the
old configured location so the existing MCP registration continues to work.

The original game/engine-tool copy is retained as an archive. Continue development here.
The old game directory remains for game planning, scripts and assets.

Git remote target: https://github.com/Aristia0930/Naru_Studio.git
Remote push is verified separately against the committed local HEAD; this file is not itself proof of push.
'''
    (target / 'MIGRATION_STATUS.md').write_text(status, encoding='utf-8')
    manifest = {'status': 'additions-sha256-verified', 'date': '2026-10-27', 'files': changes,
                'architecture': {'engine': 'C++', 'editor': 'Qt', 'game_scripting': 'Python'}}
    (target / 'HANDOFF_MANIFEST.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps({'synced': len(changes), 'target': str(target)}, ensure_ascii=True))

if __name__ == '__main__':
    main()
