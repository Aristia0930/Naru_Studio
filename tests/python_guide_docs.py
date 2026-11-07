"""Check copyable guide code, function coverage, and local links/anchors without external packages."""
import ast
from pathlib import Path
import re
import sys
from urllib.parse import unquote

root = Path(sys.argv[1])
cookbook = root / 'docs/guide/PYTHON_COOKBOOK.md'
text = cookbook.read_text(encoding='utf-8')
for name in ('python_guide.py', 'guide_global.py'):
    source = root / 'docs/examples' / name
    code = source.read_text(encoding='utf-8')
    assert '\t' not in code, f'Tab indentation: {source}'
    tree = ast.parse(code, filename=str(source))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and not node.name.startswith('_'):
            assert node.name in text, f'Function missing from cookbook: {node.name}'
blocks = re.findall(r'```python\s*\n(.*?)```', text, re.S)
assert blocks
for index, code in enumerate(blocks, 1):
    ast.parse(code, filename=f'PYTHON_COOKBOOK.md block {index}')


def anchors(path):
    body = re.sub(r'```.*?```', '', path.read_text(encoding='utf-8'), flags=re.S)
    result, counts = set(), {}
    for heading in re.findall(r'^#{1,6}\s+(.+?)\s*#*\s*$', body, re.M):
        slug = re.sub(r'[^\w\- ]', '', heading.lower()).replace(' ', '-')
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        result.add(slug if count == 0 else f'{slug}-{count}')
    return result


documents = ['README.md', 'docs/SCRIPT_API.md', 'docs/guide/PYTHON_COOKBOOK.md', 'docs/guide/README.md',
             'docs/guide/SCRIPTING.md', 'docs/guide/EVENTS.md', 'docs/guide/IMAGES.md', 'docs/guide/ASSETS.md',
             'docs/examples/README.md', 'docs/examples/assets/README.md', 'docs/examples/images/README.md']
checked = 0
for name in documents:
    path = root / name
    for link in re.findall(r'\]\(([^)]+)\)', path.read_text(encoding='utf-8')):
        if '://' in link or link.startswith('mailto:'):
            continue
        target, _, fragment = link.partition('#')
        destination = (path.parent / unquote(target.strip('<>'))).resolve() if target else path
        assert destination.exists(), f'{name}: missing {link}'
        if fragment and destination.suffix == '.md':
            assert unquote(fragment) in anchors(destination), f'{name}: missing anchor {link}'
        checked += 1
print(f'[PASS] {len(blocks)} Python code blocks, all example functions, {checked} local links/anchors')
