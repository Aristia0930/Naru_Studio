"""로컬 API 가져오기 검증 서버. 실행: python server.py [포트, 기본 8766]."""
import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        rows = [{"id": "potion", "name": "회복약", "price": 120},
                {"id": "ether", "name": "마력약", "price": 250}]
        if self.path == "/bad":
            rows[1]["price"] = "숫자가 아님"
        elif self.path == "/missing-field":
            del rows[1]["price"]
        elif self.path == "/duplicate":
            rows[1]["id"] = "potion"
        elif self.path == "/slow":
            import time
            time.sleep(3)
        elif self.path != "/":
            self.send_error(404)
            return
        data = json.dumps(rows, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1]) if len(sys.argv) > 1 else 8766), Handler)
    print(server.server_port, flush=True)
    server.serve_forever()
