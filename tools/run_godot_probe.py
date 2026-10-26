"""Run only the isolated Godot research project and retain process evidence."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
executable = root / 'refs/godot-4.7.2/bin/Godot_v4.7.2-stable_win64_console.exe'
project = root / 'probes/godot-foundation'
command = [str(executable), '--headless', '--path', str(project), '--script', 'probe.gd', '--log-file', str(project / 'godot.log')]
result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
evidence = {
    'command': command, 'returncode': result.returncode,
    'stdout': result.stdout.decode('utf-8', errors='replace'),
    'stderr': result.stderr.decode('utf-8', errors='replace'),
    'launcher_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
    'probe_sha256': hashlib.sha256((project / 'probe.gd').read_bytes()).hexdigest(),
}
output = root / 'analysis/godot/runtime-probe-process.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(evidence, ensure_ascii=True, indent=2))
raise SystemExit(result.returncode)
