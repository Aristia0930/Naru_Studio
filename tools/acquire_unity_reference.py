"""Download bounded official reference-source files, without target execution."""
import hashlib
import argparse
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'refs/unity-csreference'
NEEDLES = ['SerializedObject.bindings.cs', 'SerializedProperty.bindings.cs', 'Undo.bindings.cs',
           'PrefabUtility.cs', 'InspectorWindow.cs', 'Editor.cs', 'EditorApplication.bindings.cs']
PINNED_COMMIT = 'bf6028387a4fc2ea6d41aa7d8998e774655aedf6'

def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'Naru-Studio-research'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--commit', default=PINNED_COMMIT)
    args = parser.parse_args()
    base = 'https://api.github.com/repos/Unity-Technologies/UnityCsReference'
    commit = json.loads(get(base + '/commits/' + args.commit))['sha']
    tree = json.loads(get(base + '/git/trees/' + commit + '?recursive=1'))
    paths = [item['path'] for item in tree['tree'] if item['type'] == 'blob']
    selected = []
    for needle in NEEDLES:
        matches = [path for path in paths if path.startswith('Editor/') and path.endswith('/' + needle)]
        if len(matches) != 1:
            raise RuntimeError('Ambiguous/missing reference: %s %s' % (needle, matches))
        selected.extend(matches)
    selected.extend(path for path in paths if '/' not in path and ('license' in path.lower() or path.lower() == 'readme.md'))
    records = []
    for path in selected:
        url = 'https://raw.githubusercontent.com/Unity-Technologies/UnityCsReference/' + commit + '/' + path
        content = get(url)
        destination = DEST / 'source' / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
        records.append({'path': path, 'url': url, 'sha256': hashlib.sha256(content).hexdigest(), 'bytes': len(content)})
        print(path, flush=True)
    manifest = {'repository': 'https://github.com/Unity-Technologies/UnityCsReference', 'commit': commit,
                'revision_scope': 'Pinned public reference snapshot; not established as the installed 6000.6.5f1 build',
                'files': records, 'scope': 'Selected public reference source only; no native engine reconstruction'}
    DEST.mkdir(parents=True, exist_ok=True)
    (DEST / 'acquisition.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps({'commit': commit, 'files': len(records)}), flush=True)

if __name__ == '__main__':
    main()
