"""Deterministic loopback HTTP fixture; no external service or credentials required."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import threading
import time


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/disconnect":
            self.connection.close()
            return
        if self.path == "/slow":
            time.sleep(0.4)
        code = {"/empty": 204, "/unauthorized": 401, "/http-binary": 401, "/redirect": 302}.get(self.path, 200)
        body = {"/empty": b"", "/invalid": b"not JSON", "/large": b'"' + b"a" * 1048576 + b'"',
                "/invalid-utf8": b'{"value":"\xff"}', "/nul": b'{"value":1}\x00suffix',
                "/http-binary": b'\xff\x00tail', "/escaped-nul": b'{"value":"\\u0000tail"}'}.get(
            self.path, json.dumps({"message": "한국어", "value": 42}, ensure_ascii=False).encode())
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        if self.path == "/redirect":
            self.send_header("Location", "/unicode")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    do_PUT = do_PATCH = do_DELETE = do_POST


SCRIPT = r'''
import time

def outside_game(ctx):
    assert ctx.network is None

def run(ctx):
    base = ctx.variables.get("base_url")
    request_id = ctx.network.request(base + "/echo", method="POST", json={"message": "한국어", "values": [1, True, None]})
    deadline = time.monotonic() + 4
    while ctx.network.status(request_id) == "pending":
        assert time.monotonic() < deadline
        time.sleep(0.002)
    response = ctx.network.result(request_id)
    assert response["ok"] and response["status_code"] == 200, response
    assert response["json"] == {"message": "한국어", "values": [1, True, None]}, response
    for path, code, expected_text, expected_error in (
        ("/invalid-utf8", 200, '{"value":"\ufffd"}', "invalid_json"),
        ("/nul", 200, '{"value":1}\x00suffix', "invalid_json"),
        ("/http-binary", 401, '\ufffd\x00tail', "http_error"),
    ):
        malformed = ctx.network.request(base + path)
        deadline = time.monotonic() + 4
        while ctx.network.status(malformed) == "pending":
            assert time.monotonic() < deadline
            time.sleep(0.002)
        failure = ctx.network.result(malformed)
        assert not failure["ok"] and failure["error"] == expected_error, failure
        assert failure["status_code"] == code and failure["text"] == expected_text, failure
        assert failure["json"] is None, failure
    escaped = ctx.network.request(base + "/escaped-nul")
    deadline = time.monotonic() + 4
    while ctx.network.status(escaped) == "pending":
        assert time.monotonic() < deadline
        time.sleep(0.002)
    assert ctx.network.result(escaped)["json"] == {"value": "\x00tail"}
    try:
        ctx.network.status(request_id)
        raise AssertionError("consumed handle remained")
    except ValueError:
        pass
    slow = ctx.network.request(base + "/slow")
    assert ctx.network.result(slow) is None
    ctx.network.cancel(slow)
    assert ctx.network.status(slow) == "cancelled"
    assert ctx.network.result(slow)["error"] == "cancelled"
    for timeout in (0, 61, float("nan")):
        try:
            ctx.network.request(base, timeout=timeout)
            raise AssertionError("invalid timeout allowed")
        except ValueError:
            pass
'''


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="naru-network-") as folder:
            script = Path(folder) / "network_fixture.py"
            script.write_text(SCRIPT, encoding="utf-8")
            subprocess.run([sys.argv[1], f"http://127.0.0.1:{server.server_port}", str(script)], check=True, timeout=25)
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
