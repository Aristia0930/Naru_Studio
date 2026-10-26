"""Copy the authorized tool workspace to the separate development folder.

Originals remain intact. Existing destination files are never overwritten.
Analysis-engine installations stay in place so the current MCP registration works.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

SOURCE = Path(__file__).resolve().parents[1]
DESTINATION = SOURCE.parent.parent / 'game_tool'
DIRECTORIES = ['analysis', 'app', 'probes', 'refs', 'tools']
IGNORED_PARTS = {'.git', '.godot', '__pycache__', '.venv'}

def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()

def inventory():
    entries = []
    for directory in DIRECTORIES:
        for path in (SOURCE / directory).rglob('*'):
            if not path.is_file() or path.is_symlink():
                continue
            relative = path.relative_to(SOURCE)
            if any(part in IGNORED_PARTS for part in relative.parts):
                continue
            entries.append((path, relative))
    for path in SOURCE.iterdir():
        if path.is_file() and (path.suffix.lower() in ['.md', '.cmd'] or path.name == '.gitignore') and path.name != 'AGENTS.md':
            entries.append((path, Path(path.name)))
    entries.append((SOURCE / 'REPOSITORY_AGENTS.md', Path('AGENTS.md')))
    return sorted(entries, key=lambda item: str(item[1]))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    source_root = SOURCE.resolve()
    target_root = DESTINATION.resolve()
    expected = (source_root.parent.parent / 'game_tool').resolve()
    if target_root != expected or target_root.name != 'game_tool' or target_root == source_root:
        raise RuntimeError('Unexpected migration destination')
    entries = inventory()
    conflicts = [str(relative) for _, relative in entries if (target_root / relative).exists()]
    if conflicts:
        raise RuntimeError('Destination is not empty for selected files: ' + ', '.join(conflicts[:8]))
    plan = {'source': str(source_root), 'destination': str(target_root), 'files': len(entries),
            'bytes': sum(path.stat().st_size for path, _ in entries),
            'mode': 'copy originals preserved',
            'excluded': ['Ghidra installed directory and ZIP', '.git', '.godot', 'Python cache'],
            'binary_policy': 'Godot research binary copied locally, excluded from Git by .gitignore',
            'apply': args.apply}
    print(json.dumps(plan, ensure_ascii=True, indent=2), flush=True)
    if not args.apply:
        return
    target_root.mkdir(parents=True, exist_ok=True)
    records = []
    for original, relative in entries:
        target = target_root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(original), str(target))
        original_hash = digest(original)
        if digest(target) != original_hash:
            raise RuntimeError('Hash mismatch: ' + str(relative))
        records.append({'path': relative.as_posix(), 'sha256': original_hash, 'bytes': target.stat().st_size})
    manifest = {'status': 'copied-and-sha256-verified', 'plan': plan, 'files': records,
                'note': 'Hashes describe the initial copied snapshot; later development can change files.'}
    (target_root / 'MIGRATION_MANIFEST.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print('Copied and verified %s files.' % len(records), flush=True)

if __name__ == '__main__':
    main()
