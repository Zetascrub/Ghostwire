#!/usr/bin/env python3
"""Send a development-only message and RGB LED flash to Ghostwire."""

from __future__ import annotations

import argparse
import json
import socket


COLORS = {
    "red": (255, 0, 0),
    "green": (0, 255, 0),
    "blue": (0, 80, 255),
    "cyan": (0, 255, 255),
    "amber": (255, 120, 0),
    "yellow": (255, 220, 0),
    "magenta": (255, 0, 180),
    "purple": (150, 40, 255),
    "white": (255, 255, 255),
}


def parse_color(value: str) -> tuple[int, int, int]:
    named = COLORS.get(value.lower())
    if named:
        return named
    raw = value.removeprefix("#")
    if len(raw) != 6:
        raise argparse.ArgumentTypeError("use a colour name or RRGGBB hex")
    try:
        return tuple(int(raw[index:index + 2], 16) for index in (0, 2, 4))
    except ValueError as error:
        raise argparse.ArgumentTypeError("invalid RRGGBB colour") from error


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="Cardputer IPv4 address")
    parser.add_argument("message", help="message shown on the Cardputer")
    parser.add_argument("--color", type=parse_color, default=COLORS["cyan"])
    parser.add_argument("--duration", type=int, default=1800, metavar="MS")
    parser.add_argument("--pulses", type=int, default=2)
    parser.add_argument("--port", type=int, default=8766)
    args = parser.parse_args()
    red, green, blue = args.color
    payload = json.dumps({
        "message": args.message[:80],
        "red": red,
        "green": green,
        "blue": blue,
        "duration_ms": max(250, min(args.duration, 10000)),
        "pulses": max(1, min(args.pulses, 10)),
    }, separators=(",", ":")).encode("utf-8")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
        client.settimeout(2.0)
        client.sendto(payload, (args.host, args.port))
        try:
            reply, sender = client.recvfrom(512)
        except TimeoutError:
            raise SystemExit("No acknowledgement; check Wi-Fi, IP, and dev firmware")
    print(f"{sender[0]}:{sender[1]} {reply.decode('utf-8', errors='replace')}")


if __name__ == "__main__":
    main()
