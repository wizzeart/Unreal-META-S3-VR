"""Local-network GLB library for the Quest house sandbox. Python 3, no packages."""
from __future__ import annotations

import argparse
import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit

MAX_BYTES = 100 * 1024 * 1024
NAME = re.compile(r"^[^/\\\x00-\x1f]{1,120}\.glb$", re.IGNORECASE)
PAGE = """<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Casas VR · Biblioteca local</title><style>
:root{font-family:system-ui,sans-serif;color:#e8eef8;background:#0c1424}body{max-width:840px;margin:auto;padding:45px 20px}h1{font-size:2.4rem;margin:.3em 0}p{color:#a8b8cd;line-height:1.55}.card{background:#16243a;border:1px solid #35465e;border-radius:18px;padding:24px;margin:24px 0}button,.button{display:inline-block;background:#53dfba;color:#092018;border:0;border-radius:9px;padding:12px 18px;font-weight:700;cursor:pointer;text-decoration:none}input{width:100%;padding:16px;border:1px dashed #6b819d;border-radius:10px;background:#101c2e;color:#fff;box-sizing:border-box}li{display:flex;justify-content:space-between;align-items:center;gap:15px;border-top:1px solid #35465e;padding:14px 0;overflow-wrap:anywhere}small{display:block;color:#9fb0c7}code{color:#9be9d1}#status{min-height:1.3em}</style>
<body><small>CASAS VR / HUB LOCAL</small><h1>Biblioteca de modelos</h1><p>Sube un archivo GLB exportado desde 3ds Max. La laptop y Quest deben estar en la misma red Wi-Fi.</p>
<div class="card"><h2>Subir casa</h2><input id="file" type="file" accept=".glb,model/gltf-binary"><p><button id="upload">Guardar en biblioteca</button></p><p id="status"></p></div>
<div class="card"><h2>Modelos disponibles</h2><ul id="models"></ul></div><p>En Quest, escribe la dirección de esta laptop en el campo HUB y pulsa «VER MODELOS DE LA LAPTOP». Con USB y <code>adb reverse tcp:8765 tcp:8765</code>, usa <code>http://127.0.0.1:8765</code>. Escala: 1 metro GLB = 1 metro VR.</p>
<script>const $=s=>document.querySelector(s);async function refresh(){const data=await (await fetch('/api/models')).json();$('#models').replaceChildren();for(const m of data){let li=document.createElement('li'),span=document.createElement('span'),a=document.createElement('a');span.textContent=m.name;a.className='button';a.href=m.url;a.textContent='Descargar / copiar enlace';a.onclick=async e=>{e.preventDefault();await navigator.clipboard.writeText(a.href);a.textContent='Enlace copiado';};li.append(span,a);$('#models').append(li);}if(!data.length)$('#models').textContent='Aún no hay modelos.'}$('#upload').onclick=async()=>{const f=$('#file').files[0];if(!f){$('#status').textContent='Selecciona un archivo .glb';return}$('#status').textContent='Subiendo…';try{const r=await fetch('/api/models/'+encodeURIComponent(f.name),{method:'PUT',body:f});if(!r.ok)throw new Error(await r.text());$('#status').textContent='Archivo listo para Quest';await refresh()}catch(e){$('#status').textContent='Error: '+e.message}};refresh()</script></body></html>"""


def valid_glb(data: bytes) -> bool:
    return (len(data) >= 20 and data[:4] == b"glTF" and
            int.from_bytes(data[4:8], "little") == 2 and
            int.from_bytes(data[8:12], "little") == len(data))


def handler_for(folder: Path):
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
                return self.send(413, b"Maximum file size: 100 MB", "text/plain")
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
    print(f"House hub: http://localhost:{args.port} — files: {args.folder}")
    ThreadingHTTPServer((args.host, args.port), handler_for(args.folder)).serve_forever()
