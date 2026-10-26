"""Acquire the official portable editor and pinned source excerpts for research."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'refs' / 'godot-4.7.2'
TAG = '4.7.2-stable'
SOURCES = [
    'LICENSE.txt', 'scene/main/node.cpp', 'scene/main/scene_tree.cpp',
    'scene/resources/packed_scene.cpp', 'core/object/object.cpp',
    'core/io/resource.cpp', 'core/io/resource_loader.cpp',
    'scene/2d/physics/character_body_2d.cpp', 'scene/2d/camera_2d.cpp',
    'editor/inspector/editor_inspector.cpp', 'editor/scene/scene_tree_dock.cpp',
    'editor/editor_undo_redo_manager.cpp',
]

def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'game-tool-research'})
    with urllib.request.urlopen(request, timeout=180) as response:
        return response.read()

def main():
    DEST.mkdir(parents=True, exist_ok=True)
    release = json.loads(get(f'https://api.github.com/repos/godotengine/godot-builds/releases/tags/{TAG}'))
    asset = next(a for a in release['assets'] if a['name'] == f'Godot_v{TAG}_win64.exe.zip')
    zip_path = DEST / asset['name']
    if not zip_path.exists():
        content = get(asset['browser_download_url'])
        digest = 'sha256:' + hashlib.sha256(content).hexdigest()
        if asset.get('digest') and digest != asset['digest']:
            raise RuntimeError('Official release asset digest mismatch')
        zip_path.write_bytes(content)
    content = zip_path.read_bytes()
    digest = 'sha256:' + hashlib.sha256(content).hexdigest()
    if asset.get('digest') and digest != asset['digest']:
        raise RuntimeError('Cached release asset digest mismatch')
    with zipfile.ZipFile(zip_path) as archive:
        for item in archive.infolist():
            target = (DEST / 'bin' / item.filename).resolve()
            try:
                target.relative_to((DEST / 'bin').resolve())
            except ValueError:
                raise RuntimeError('Archive path outside destination')
        archive.extractall(DEST / 'bin')
    print('Official editor downloaded, digest verified, extracted.', flush=True)
    ref = json.loads(get(f'https://api.github.com/repos/godotengine/godot/git/ref/tags/{TAG}'))['object']
    if ref['type'] == 'tag':
        ref = json.loads(get(ref['url']))['object']
    if ref['type'] != 'commit':
        raise RuntimeError('Tag did not resolve to a commit')
    commit = ref['sha']
    records = []
    for name in SOURCES:
        url = f'https://raw.githubusercontent.com/godotengine/godot/{commit}/{name}'
        try:
            source = get(url)
        except Exception as error:
            records.append({'path': name, 'url': url, 'error': str(error)})
            print(f'Source unavailable: {name}: {error}', flush=True)
            continue
        path = DEST / 'source' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(source)
        records.append({'path': name, 'url': url, 'sha256': hashlib.sha256(source).hexdigest(), 'bytes': len(source)})
        print(f'Source: {name}', flush=True)
    manifest = {
        'tag': TAG, 'source_commit': commit,
        'release_url': release['html_url'], 'asset_url': asset['browser_download_url'],
        'official_asset_digest': asset.get('digest'), 'actual_asset_digest': digest,
        'binary_files': [{'name': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'bytes': p.stat().st_size}
                         for p in (DEST / 'bin').glob('*') if p.is_file()],
        'source_scope': 'Selected source excerpts only, not a complete repository',
        'sources': records,
    }
    (DEST / 'acquisition.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'destination': str(DEST), 'commit': commit, 'source_files': len(records)}), flush=True)

if __name__ == '__main__':
    main()
