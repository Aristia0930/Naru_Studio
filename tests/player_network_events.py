"""실제 SDL player에서 이벤트 API, 분기, F8 취소와 원본 보존을 검사한다."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from http.server import ThreadingHTTPServer
from network_integration import Handler

server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix="naru-player-api-") as folder:
        root = Path(folder) / "project"
        subprocess.run([sys.executable, str(Path(sys.argv[2]) / "tools/generate_network_event_example.py"), str(root)], check=True)
        manifest = root / "project.json"
        project = json.loads(manifest.read_text(encoding="utf-8"))
        project["networkConnections"][0]["baseUrl"] = f"http://127.0.0.1:{server.server_port}"
        manifest.write_text(json.dumps(project, ensure_ascii=False), encoding="utf-8")
        scene_file = root / "scenes/start.json"
        scene = json.loads(scene_file.read_text(encoding="utf-8"))
        request = scene["objects"][1]["event"]["commands"][0]
        request["mappings"] = {"/message": "공지내용", "/value": "점수"}
        def run(path, extra=()):
            request["path"] = path
            scene_file.write_text(json.dumps(scene, ensure_ascii=False), encoding="utf-8")
            before = (manifest.read_bytes(), scene_file.read_bytes())
            # Fixed frames execute fast; a global hook below yields to the local server.
            result = subprocess.run([sys.argv[1], str(root), "--frames", "100", "--debug-state", "--hold", "right:0-60", *extra], capture_output=True, encoding="utf-8", errors="replace", timeout=15)
            assert result.returncode == 0, result.stdout + result.stderr
            assert before == (manifest.read_bytes(), scene_file.read_bytes()), "editing source changed"
            states = [json.loads(line.split("\t", 1)[1]) for line in result.stdout.splitlines() if line.startswith("NARU_STATE\t")]
            return states[-1], result.stdout
        (root/"scripts/observe.py").write_text('''import time
def on_update(ctx):
    time.sleep(0.003)
    ctx.variables.set("hero_x", ctx.player.x)
''', encoding="utf-8")
        project["globalScripts"] = ["observe.py"]
        manifest.write_text(json.dumps(project, ensure_ascii=False), encoding="utf-8")
        state, output = run("/unicode")
        assert state["api_ok"]["value"] == "True" and state["점수"]["value"] == "42", state
        assert state["분기결과"]["value"] == "성공" and float(state["hero_x"]["value"]) > 80, state
        state, output = run("/unauthorized")
        assert state["api_error"]["value"] == "http_error" and state["분기결과"]["value"] == "실패", state
        state, output = run("/slow", ["--hold-key", "F8:2-2"])
        assert state["api_error"]["value"] == "cancelled" and state["분기결과"]["value"] == "시작 전", state
        print("[PASS] player API mapping, success/failure branch, movement, F8 and source preservation")
finally:
    server.shutdown()
    server.server_close()
