#!/usr/bin/env python3
"""Serve the generated map catalog on localhost without third-party packages."""

from __future__ import annotations

import argparse
import json
from functools import partial
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlparse


ROOT = Path(__file__).resolve().parent
DATA = ROOT / "data"


class CatalogHandler(SimpleHTTPRequestHandler):
    def end_headers(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def send_json_file(self, path: Path) -> None:
        if not path.is_file():
            self.send_error(HTTPStatus.NOT_FOUND, "Catalog entry not found")
            return
        payload = path.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self) -> None:
        route = unquote(urlparse(self.path).path)
        static_routes = {
            "/api/catalog": DATA / "catalog.json",
            "/api/terrains": DATA / "terrains.json",
            "/api/tilesets": DATA / "tilesets.json",
            "/api/chapters": DATA / "chapters.json",
            "/api/layouts": DATA / "layouts.json",
            "/api/assets": DATA / "assets.json",
        }
        if route == "/api/health":
            payload = json.dumps({"ok": True, "catalog": "/api/catalog"}).encode("utf-8")
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        if route in static_routes:
            self.send_json_file(static_routes[route])
            return
        if route.startswith("/api/maps/"):
            map_id = route.removeprefix("/api/maps/")
            if not map_id or any(character not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for character in map_id):
                self.send_error(HTTPStatus.BAD_REQUEST, "Invalid map id")
                return
            self.send_json_file(DATA / "maps" / f"{map_id}.json")
            return
        super().do_GET()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4173)
    args = parser.parse_args()
    if not (DATA / "catalog.json").is_file():
        raise SystemExit("Catalog missing. Run: python3 map_data/build.py")
    server = ThreadingHTTPServer(("127.0.0.1", args.port), partial(CatalogHandler, directory=str(ROOT)))
    print(f"Map catalog: http://127.0.0.1:{args.port}/api/catalog")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
