#!/usr/bin/env python3
"""Receive manual Ghostwire development diagnostic exports over HTTP."""

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


MAX_REPORT_BYTES = 64 * 1024


def safe_name(value: object) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "-", str(value)).strip("-.")
    return cleaned[:48] or "unknown"


class DiagnosticHandler(BaseHTTPRequestHandler):
    output_dir = Path("diagnostic-collections")

    def send_json(self, status: int, value: dict[str, Any]) -> None:
        body = json.dumps(value, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path != "/diagnostics":
            self.send_json(404, {"ok": False, "error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if length <= 0 or length > MAX_REPORT_BYTES:
            self.send_json(413, {"ok": False, "error": "invalid report size"})
            return
        try:
            report = json.loads(self.rfile.read(length))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.send_json(400, {"ok": False, "error": "invalid JSON"})
            return
        if not isinstance(report, dict) or report.get("schema") != 1:
            self.send_json(400, {"ok": False, "error": "unsupported schema"})
            return

        received = datetime.now(timezone.utc)
        report["received_utc"] = received.isoformat().replace("+00:00", "Z")
        report["remote_address"] = self.client_address[0]
        diagnostics = report.get("diagnostics", {})
        device = safe_name(diagnostics.get("Device ID", "unknown"))
        stamp = received.strftime("%Y%m%dT%H%M%S.%fZ")
        self.output_dir.mkdir(parents=True, exist_ok=True)
        destination = self.output_dir / f"{stamp}_{device}.json"
        destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        with (self.output_dir / "index.jsonl").open("a", encoding="utf-8") as index:
            index.write(json.dumps(report, separators=(",", ":")) + "\n")

        print(
            f"received {destination.name}: "
            f"heap={diagnostics.get('Heap free', '?')} "
            f"minimum={diagnostics.get('Heap minimum', '?')} "
            f"operations={diagnostics.get('Operations', '?')} "
            f"stability={diagnostics.get('Stability events', '?')}",
            flush=True,
        )
        self.send_json(201, {"ok": True, "file": destination.name})

    def log_message(self, format: str, *args: object) -> None:
        print(f"{self.client_address[0]} - {format % args}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--output", type=Path, default=Path("diagnostic-collections"))
    args = parser.parse_args()
    DiagnosticHandler.output_dir = args.output
    server = ThreadingHTTPServer((args.bind, args.port), DiagnosticHandler)
    print(
        f"Ghostwire diagnostic receiver listening on "
        f"http://{args.bind}:{args.port}/diagnostics",
        flush=True,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
