"""Read-only REA stdio handshake/catalog probe; evidence stays in engine-tool."""
import json
import argparse
import queue
import subprocess
import threading
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis' / 'rea-connection'
OUT.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--analyze', action='store_true')
options = parser.parse_args()
process = subprocess.Popen(
    ['cmd.exe', '/d', '/c', 'npx', '-y', 'rea-agents@6.8.0', 'mcp'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    encoding='utf-8', errors='replace', bufsize=1,
)
messages = queue.Queue()
errors = []

def read_stdout():
    for line in process.stdout:
        try:
            messages.put(json.loads(line))
        except ValueError:
            errors.append('NON_JSON_STDOUT: ' + line)

def read_stderr():
    for line in process.stderr:
        errors.append(line)

threading.Thread(target=read_stdout, daemon=True).start()
threading.Thread(target=read_stderr, daemon=True).start()

def send(message):
    process.stdin.write(json.dumps(message, ensure_ascii=False) + '\n')
    process.stdin.flush()

def request(number, method, params):
    send({'jsonrpc': '2.0', 'id': number, 'method': method, 'params': params})
    while True:
        reply = messages.get(timeout=55)
        if reply.get('id') == number:
            if 'error' in reply:
                raise RuntimeError(json.dumps(reply['error']))
            return reply['result']

def save(name, data):
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')

try:
    initialized = request(1, 'initialize', {
        'protocolVersion': '2024-11-05', 'capabilities': {},
        'clientInfo': {'name': 'game-rea-connection-verifier', 'version': '1.0'},
    })
    save('initialize.json', initialized)
    send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
    catalog = request(2, 'tools/list', {})
    save('tools-list.json', catalog)
    candidates = [t for t in catalog['tools'] if t['name'] == 'analyze_javascript_application']
    print(json.dumps({'server': initialized['serverInfo'], 'tool_count': len(catalog['tools']),
                      'analysis_tool_available': bool(candidates)}, ensure_ascii=True), flush=True)
    if options.analyze:
        arguments = {
            'input_path': str(ROOT / 'refs' / 'rpgmaker-mv-corescript'),
            'source_paths': ['js/rpg_objects/Game_Interpreter.js'],
            'analysis_timeout_ms': 30000,
            'detail': 'summary',
        }
        save('analysis-request.json', arguments)
        result = request(3, 'tools/call', {
            'name': 'analyze_javascript_application', 'arguments': arguments,
        })
        save('analysis-summary.json', result)
        print(json.dumps({'analysis_saved': str(OUT / 'analysis-summary.json'),
                          'isError': result.get('isError', False),
                          'result_keys': list(result)}, ensure_ascii=False), flush=True)
        if result.get('isError'):
            print(json.dumps(result, ensure_ascii=False))
            raise RuntimeError('REA tools/call returned an error; inspect saved evidence')
        bundle = OUT / ('evidence-' + datetime.now().strftime('%Y%m%d-%H%M%S') + '.json')
        exported = request(4, 'tools/call', {
            'name': 'export_evidence_bundle',
            'arguments': {'path': str(bundle), 'overwrite': False},
        })
        save('export-result.json', exported)
        print(json.dumps({'export_isError': exported.get('isError', False),
                          'bundle': str(bundle)}, ensure_ascii=True), flush=True)
        if exported.get('isError'):
            raise RuntimeError('Evidence export failed; inspect export-result.json')
finally:
    process.stdin.close()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True)
    (OUT / 'stderr.log').write_text(''.join(errors), encoding='utf-8')
