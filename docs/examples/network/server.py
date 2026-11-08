"""Naru HTTP 예제용 로컬 서버. python server.py로 시작하고 Ctrl+C로 종료합니다."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import time
from urllib.parse import urlsplit


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, value):
        data = json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass  # 취소 또는 시간 초과를 확인할 때 클라이언트가 먼저 종료할 수 있다.

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/slow":
            time.sleep(3)
        if path in ("/hello", "/slow"):
            self.reply(200, {"message": "안녕하세요", "score": 42})
        else:
            self.reply(404, {"error": "경로가 없습니다"})

    def do_POST(self):
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if size < 0 or size > 65536:
                self.reply(413, {"error": "요청이 너무 큽니다"})
                return
            payload = json.loads(self.rfile.read(size).decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            self.reply(400, {"error": "JSON을 읽을 수 없습니다"})
            return
        self.reply(200, {"received": payload})


if __name__ == "__main__":
    print("Naru 테스트 서버: http://127.0.0.1:8765 (종료: Ctrl+C)", flush=True)
    try:
        ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
    except KeyboardInterrupt:
        print("서버 종료")
