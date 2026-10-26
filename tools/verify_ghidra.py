"""Verify the installed Ghidra provider on a static copy, without executing it."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'analysis' / 'rea-connection'
ghidra = root / 'ghidra_12.1.4_PUBLIC_20260921' / 'ghidra_12.1.4_PUBLIC'
cli = Path(os.environ['LOCALAPPDATA']) / 'npm-cache/_npx/da7caeae9db9c589/node_modules/rea-agents/scripts/rea.mjs'
source = Path(os.environ['SystemRoot']) / 'System32/hostname.exe'
target_dir = out / 'native-verification'
target_dir.mkdir(exist_ok=True)
target = target_dir / 'hostname.exe'
shutil.copy2(source, target)
data = target.read_bytes()
digest = hashlib.sha256(data).hexdigest()
assert digest == hashlib.sha256(source.read_bytes()).hexdigest()
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe + 4] == b'PE\0\0'
optional = pe + 24
magic = struct.unpack_from('<H', data, optional)[0]
assert magic in (0x10b, 0x20b)
base = struct.unpack_from('<Q' if magic == 0x20b else '<I', data, optional + (24 if magic == 0x20b else 28))[0]
entry = base + struct.unpack_from('<I', data, optional + 16)[0]
env = dict(os.environ, GHIDRA_INSTALL_DIR=str(ghidra), JAVA_HOME='C:\\Program Files\\Java\\jdk-21')
command = ['node', str(cli), 'decompile', str(target), hex(entry), '--provider', 'ghidra', '--format', 'json']
request = {'source': str(source), 'target': str(target), 'sha256': digest, 'entry': hex(entry), 'command': command, 'target_executed': False}
(out / 'ghidra-native-request.json').write_text(json.dumps(request, indent=2), encoding='utf-8')
print('Starting static decompilation at ' + hex(entry), flush=True)
result = subprocess.run(command, env=env, capture_output=True, encoding='utf-8', errors='replace')
(out / 'ghidra-native-result.json').write_text(result.stdout, encoding='utf-8')
(out / 'ghidra-native-stderr.log').write_text(result.stderr, encoding='utf-8')
print(json.dumps({'exit_code': result.returncode, 'stdout_bytes': len(result.stdout.encode('utf-8')), 'stderr_tail': result.stderr[-1800:]}), flush=True)
raise SystemExit(result.returncode)
