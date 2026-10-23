"""Local ChessScript Studio server using only Python's standard library."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import subprocess


ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
BINARY = ROOT / "build" / "chessscript"
SAMPLE = ROOT / "examples" / "scholar_mate.chs"
HOST = "127.0.0.1"
PORT = 8765
MAX_SOURCE_BYTES = 64 * 1024


class Handler(BaseHTTPRequestHandler):
    def send_bytes(self, status, content_type, payload):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        static = {
            "/": ("index.html", "text/html; charset=utf-8"),
            "/app.js": ("app.js", "text/javascript; charset=utf-8"),
            "/styles.css": ("styles.css", "text/css; charset=utf-8"),
        }
        if self.path == "/api/sample":
            self.send_bytes(200, "text/plain; charset=utf-8", SAMPLE.read_bytes())
        elif self.path in static:
            filename, content_type = static[self.path]
            self.send_bytes(200, content_type, (WEB / filename).read_bytes())
        else:
            self.send_bytes(404, "text/plain; charset=utf-8", b"Not found")

    def do_POST(self):
        if self.path != "/api/compile":
            self.send_bytes(404, "application/json", b'{"ok":false,"error":{"message":"Not found"}}')
            return
        origin = self.headers.get("Origin")
        if origin and origin not in {f"http://{HOST}:{PORT}", f"http://localhost:{PORT}"}:
            self.send_bytes(403, "application/json", b'{"ok":false,"error":{"message":"Invalid origin"}}')
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if length < 0 or length > MAX_SOURCE_BYTES:
            self.send_bytes(413, "application/json", b'{"ok":false,"error":{"message":"Source must not exceed 65536 bytes"}}')
            return
        source = self.rfile.read(length)
        try:
            source.decode("utf-8")
        except UnicodeDecodeError:
            payload = json.dumps({
                "ok": False,
                "error": {
                    "category": "LEXICAL",
                    "message": "Source must be valid UTF-8 text",
                },
            }).encode("utf-8")
            self.send_bytes(400, "application/json; charset=utf-8", payload)
            return
        try:
            completed = subprocess.run(
                [str(BINARY), "--json", "-"],
                input=source,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=5,
                check=False,
                cwd=ROOT,
            )
            result = json.loads(completed.stdout)
            payload = json.dumps(result, ensure_ascii=False).encode("utf-8")
            self.send_bytes(200, "application/json; charset=utf-8", payload)
        except (OSError, subprocess.TimeoutExpired, json.JSONDecodeError) as error:
            payload = json.dumps({"ok": False, "error": {"category": "SERVER", "message": str(error)}}).encode()
            self.send_bytes(500, "application/json; charset=utf-8", payload)


if __name__ == "__main__":
    if not BINARY.exists():
        raise SystemExit("Build the compiler first with `make`.")
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"ChessScript Studio: http://{HOST}:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
