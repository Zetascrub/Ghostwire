#!/usr/bin/env python3
"""Poll and validate Ghostwire Relay protocol-v1 health for endurance tests."""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="http://ghostwire-poe-p4.local:8765/v1/status")
    parser.add_argument("--duration", type=int, default=24 * 60 * 60,
                        help="test duration in seconds (default: 24 hours)")
    parser.add_argument("--interval", type=float, default=10.0)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--max-consecutive-failures", type=int, default=3)
    return parser.parse_args()


def validate(payload):
    if payload.get("protocol") != "ghostwire-companion":
        raise ValueError("unexpected protocol")
    if payload.get("protocol_version") != 1:
        raise ValueError("unexpected protocol version")
    for path in (("device", "id"), ("device", "firmware"),
                 ("ethernet", "started"), ("ethernet", "link"),
                 ("system", "uptime_ms"), ("system", "free_heap_bytes")):
        value = payload
        for key in path:
            value = value[key]
    return payload["system"]["uptime_ms"], payload["system"]["free_heap_bytes"]


def main():
    args = parse_args()
    started = time.monotonic()
    deadline = started + args.duration
    samples = failures = consecutive = reboots = 0
    minimum_heap = None
    previous_uptime = None

    while time.monotonic() < deadline:
        sample_started = time.monotonic()
        try:
            request = urllib.request.Request(args.url, headers={"Accept": "application/json"})
            with urllib.request.urlopen(request, timeout=args.timeout) as response:
                payload = json.loads(response.read(1537))
            uptime, free_heap = validate(payload)
            if previous_uptime is not None and uptime < previous_uptime:
                reboots += 1
            previous_uptime = uptime
            minimum_heap = free_heap if minimum_heap is None else min(minimum_heap, free_heap)
            samples += 1
            consecutive = 0
            print(f"PASS sample={samples} uptime_ms={uptime} heap={free_heap} "
                  f"link={payload['ethernet']['link']} internet="
                  f"{payload.get('internet', {}).get('reachable', False)}", flush=True)
        except (OSError, KeyError, ValueError, json.JSONDecodeError,
                urllib.error.URLError) as error:
            failures += 1
            consecutive += 1
            print(f"FAIL consecutive={consecutive} error={error}", flush=True)
            if consecutive > args.max_consecutive_failures:
                print("RESULT FAIL: consecutive failure limit exceeded", file=sys.stderr)
                return 1

        delay = args.interval - (time.monotonic() - sample_started)
        if delay > 0:
            time.sleep(delay)

    elapsed = int(time.monotonic() - started)
    print(f"RESULT PASS: elapsed={elapsed}s samples={samples} failures={failures} "
          f"reboots={reboots} minimum_heap={minimum_heap}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
