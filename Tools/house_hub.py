"""Local-network GLB library for Matrix VR Show. Python 3, no packages."""
from __future__ import annotations

import argparse
import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit

MAX_BYTES = 100 * 1024 * 1024
NAME = re.compile(r"^[^/\\\x00-\x1f]{1,120}\.glb$", re.IGNORECASE)
PAGE = (Path(__file__).with_name("hub_page.html")).read_text(encoding="utf-8")


def valid_glb(data: bytes) -> bool:
    return (len(data) >= 20 and data[:4] == b"glTF" and
            int.from_bytes(data[4:8], "little") == 2 and
            int.from_bytes(data[8:12], "little") == len(data))


def handler_for(folder: Path):
    cover = Path(__file__).resolve().parent.parent / "Branding" / "cover.png"
    icon = Path(__file__).resolve().parent.parent / "Branding" / "app-mark.png"

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            # pythonw has no stderr; keep the server usable as a hidden process.
            pass

        def send(self, code: int, body: bytes, content_type: str):
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = urlsplit(self.path).path
            if path == "/":
                return self.send(200, PAGE.encode(), "text/html; charset=utf-8")
            if path == "/brand/cover.png":
                if not cover.is_file():
                    return self.send(404, b"Not found", "text/plain")
                return self.send(200, cover.read_bytes(), "image/png")
            if path == "/brand/icon.png":
                if not icon.is_file():
                    return self.send(404, b"Not found", "text/plain")
                return self.send(200, icon.read_bytes(), "image/png")
            if path == "/api/models":
                entries = [{"name": p.name, "url": f"http://{self.headers.get('Host', 'localhost')}/models/{quote(p.name)}"}
                           for p in sorted(folder.glob("*.glb")) if p.is_file()]
                return self.send(200, json.dumps(entries).encode(), "application/json")
            if path.startswith("/models/"):
                name = unquote(path.removeprefix("/models/"))
                if not NAME.fullmatch(name):
                    return self.send(400, b"Invalid filename", "text/plain")
                file = folder / name
                if not file.is_file():
                    return self.send(404, b"Not found", "text/plain")
                return self.send(200, file.read_bytes(), "model/gltf-binary")
            return self.send(404, b"Not found", "text/plain")

        def do_PUT(self):
            path = urlsplit(self.path).path
            name = unquote(path.removeprefix("/api/models/"))
            if not path.startswith("/api/models/") or not NAME.fullmatch(name) or name in (".", ".."):
                return self.send(400, b"Invalid filename", "text/plain")
            try:
                length = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                length = 0
            if not 20 <= length <= MAX_BYTES:
                return self.send(413, b"El limite para Quest es 100 MiB. Reduce el GLB antes de subirlo.", "text/plain")
            data = self.rfile.read(length)
            if not valid_glb(data):
                return self.send(400, b"Invalid GLB", "text/plain")
            (folder / name).write_bytes(data)
            return self.send(201, b"Saved", "text/plain")

    return Handler


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--folder", type=Path, default=Path(__file__).resolve().parent / "models")
    args = parser.parse_args()
    args.folder.mkdir(parents=True, exist_ok=True)
    print(f"Matrix VR Show hub: http://localhost:{args.port} — files: {args.folder}")
    ThreadingHTTPServer((args.host, args.port), handler_for(args.folder)).serve_forever()
