# Ghostwire Companion Protocol

This directory defines the versioned boundary shared by Ghostwire on the
Cardputer ADV and external companions. Protocol version 1 started as a
read-only discovery and health slice; the only state-changing capability it
now carries is the authenticated payload-slot command channel (Grove and
Wi-Fi, below), gated behind Grove pairing.

## Discovery

Companions advertise the DNS-SD service `_ghostwire._tcp.local` and include
these TXT records:

| Key | Version 1 value |
| --- | --- |
| `proto` | `1` |
| `role` | `poe` |
| `model` | `unit-poe-p4` |
| `path` | `/v1/status` |

The first Unit PoE-P4 image uses hostname `ghostwire-poe-p4.local`, instance
name `Ghostwire Unit PoE-P4`, and TCP port `8765`.

Discovery is advisory. A client must retrieve `/v1/status` and verify the
protocol name and version before trusting the advertised capabilities.

## Status endpoint

`GET /v1/status` returns `application/json`:

```json
{
  "protocol": "ghostwire-companion",
  "protocol_version": 1,
  "device": {
    "id": "poe-p4-30eda0eab970",
    "model": "M5Stack Unit PoE-P4",
    "firmware": "0.3.0"
  },
  "capabilities": ["status", "events", "command", "payload_upload", "loot"],
  "ethernet": {
    "started": true,
    "link": true,
    "ip": "192.0.2.10",
    "netmask": "255.255.255.0",
    "gateway": "192.0.2.1",
    "dns": "192.0.2.53",
    "speed_mbps": 100,
    "full_duplex": true
  },
  "internet": {"reachable": true},
  "ghostwire": {"connected": true},
  "indicator": {"state": "ghostwire"},
  "payload": {"state": "idle", "finding_count": 0},
  "system": {
    "uptime_ms": 12345,
    "reset_reason": "power_on",
    "temperature_c": 42.5,
    "free_heap_bytes": 284160,
    "minimum_free_heap_bytes": 280112
  }
}
```

Unknown fields must be ignored. Required version 1 fields are `protocol`,
`protocol_version`, `device.id`, `device.model`, `device.firmware`,
`capabilities`, and `ethernet`.

The `internet`, `ghostwire`, `indicator`, and `payload` objects are optional
protocol-v1 telemetry. `internet.reachable` reflects the latest bounded
outbound probe. `ghostwire.connected` means a client contacted a Ghostwire
HTTP or WebSocket endpoint recently; it is not an authenticated-session
claim. Indicator states are `booting`, `fault`, `ready`, `lan`, `internet`,
and `ghostwire`. `payload.state` (`idle`/`running`/`success`/`error`) and
`payload.finding_count` mirror the payload engine's LED and last completed
scan -- the same fields Grove's `S` frame carries (see below), so a
command's result is visible the same way over either transport. The
optional `system` object provides uptime, reset reason, internal chip
temperature, and current/minimum free heap. Internal temperature is diagnostic
silicon telemetry and must not be interpreted as enclosure or ambient
temperature.

## Grove UART

115200 baud, 8 data bits, no parity, one stop bit. Frames are newline-
terminated ASCII. The final comma-separated field is an eight-digit
hexadecimal IEEE CRC32 calculated over everything preceding that comma. All
constants (`GHOSTWIRE_GROVE_*`) live in `grove_link.h`, shared by both
firmwares so encode/decode can't drift apart.

```text
GW1,H,<sequence>,<uptime_ms>,<crc32>                          companion heartbeat, 1 Hz
GW1,A,<sequence>,<crc32>                                      Cardputer acknowledgement
GW1,S,<seq>,<link>,<speed>,<duplex>,<internet>,<ind>,          companion status, 1 Hz
       <uptime_s>,<reset>,<temp_x10>,<heap_kb>,
       <min_heap_kb>,<ip>,<payload_state>,<finding_count>,<crc32>
GW1,I,<seq>,<device_id>,<firmware>,<crc32>                     companion identity, every 10th cycle
```

`<payload_state>` is a single-char code (`I`/`R`/`S`/`E` = idle/running/
success/error) mirroring the payload engine's LED, and `<finding_count>` is
its last completed scan's result count -- both described further in "Grove
pairing and command authentication" below, which is what can set them
remotely.

The P4 sends one heartbeat per second and only accepts an acknowledgement for
its latest sequence; the link expires after three seconds without a valid
acknowledgement. `S` and `I` are one-way pushes with their own freshness
windows (`GHOSTWIRE_GROVE_STATUS_TIMEOUT_MS`/`_IDENTITY_TIMEOUT_MS`), tracked
independently of the heartbeat/ack link state. `S`'s `<ind>` and `<reset>`
are single-char codes (`ghostwire_grove_indicator_code`/`_reset_code` in
`grove_link.h`) for the same states the HTTP status endpoint's `indicator`/
`system.reset_reason` report; `<ip>` is the P4's DHCP-assigned address,
letting the Cardputer show it even with no Wi-Fi/mDNS path at all. The
Cardputer's PoE Companion screen prefers this Grove telemetry over the HTTP
fetch below whenever it's fresh, falling back to HTTP otherwise.
`GHOSTWIRE_COMMON_PORTS` (`scan_ports.h`) is the shared 13-port quick-scan
list both the Cardputer's Port Scan screen and the P4's button-triggered scan
payload use, so "common ports" means the same thing on both sides.

None of this authenticates either endpoint or carries state-changing
commands -- pairing (below) is what that requires.

## Grove pairing and command authentication

Constants: `auth.h`. Threat model: an unauthenticated remote command channel
is explicitly not part of protocol v1 (see "HTTP behavior" below); pairing
over the physically-wired Grove link, followed by time-windowed authenticated
commands, is how a future command channel (not yet implemented) gets to exist
safely without that exposure.

```text
GW1,P,<seq>,<pubkey_hex_64>,<crc32>                  Cardputer's pairing request
GW1,Q,<seq>,<pubkey_hex_64>,<unix_time>,<crc32>      P4's pairing response
GW1,C,<seq>,<slot>,<tag_hex16>,<crc32>                Cardputer's command request
GW1,K,<seq>,<accepted>,<crc32>                        P4's immediate accept/reject
```

Pairing is a deliberate, explicit action (`Tab -> Pair via Grove` on the
Cardputer's PoE Companion detail screen), not automatic. Each side generates
an ephemeral X25519 keypair, sends its public key (32 bytes, hex-encoded),
and on receiving the other's public key computes the ECDH shared secret and
derives a 32-byte session key via `HKDF-SHA256(shared_secret,
salt=GHOSTWIRE_AUTH_HKDF_SALT, info=GHOSTWIRE_AUTH_HKDF_INFO)`. The session
key itself is never transmitted -- only the two publics cross the wire, which
is the entire point of a real key exchange over just sending a shared secret
directly. Each side persists its derived key (P4: NVS; Cardputer:
`Preferences`) and re-pairing overwrites it, which is how a key gets
rotated.

The `Q` response also carries the P4's current Unix UTC time (it's NTP-
synced; the Cardputer has no RTC/NTP of its own outside a manual
`TimeStatus -> Sync from NTP`). The Cardputer computes
`clockOffsetSeconds = p4UnixTime - millis()/1000` once at pairing success
and derives "now" for every future command from that offset alone, so the
command channel works over Grove with no Wi-Fi dependency. A P4 time below
~1700000000 (its own NTP sync hasn't happened yet) leaves the offset unset
and commands unavailable until re-pairing after it syncs.

A command is authenticated by a truncated time-windowed HMAC tag:

```text
tag = truncate(HMAC-SHA256(session_key, command_bytes || window_be64),
               GHOSTWIRE_AUTH_TAG_BYTES)
```

where `window = floor(unix_utc_seconds / GHOSTWIRE_AUTH_WINDOW_SECONDS)`. A
verifier accepts a tag matching the current window or either adjacent one
(`GHOSTWIRE_AUTH_WINDOW_TOLERANCE`, absorbing clock skew between the two
devices' independently NTP-synced clocks), and rejects it otherwise. A
captured command's tag stops validating once its window has passed, so
replaying a sniffed command doesn't grant standing access. This primitive
(`ghostwire_auth_compute_tag`/`_verify_tag` in each firmware) is what
authenticates `C` frames: `<slot>` (0 or 1, matching the two button-
triggered payload slots) is the sole authenticated message. The P4 replies
`K` immediately either way -- `<accepted>` is a bare 0/1, revealing nothing
about *why* a command was rejected (bad tag, not paired, already busy) to
whatever's listening on the wire. Acceptance means the P4 queued the
command through the exact same dispatch path a real button press uses; the
actual run result rides the next 1 Hz `S` frame's `<payload_state>`/
`<finding_count>` fields (see above), not a second response frame.

Pairing itself stays Grove-only (it's the physically-wired trust bootstrap),
but the session key and clock offset it establishes also authenticate
commands sent over Wi-Fi -- see "Wi-Fi command channel" below.

## Payload script upload

The two payload slots' scripts (`REM`/`DELAY`/`LOG`/`INTERNET_CHECK`/
`PING_SWEEP`/`PORT_SCAN`, see "Grove UART" above) are runtime-editable, not
just the two hardcoded defaults they ship with. Scripts live on the
Cardputer's SD card under `/ghostwire/poe-scripts/` (a different directory
*and* vocabulary from the BLE HID DuckyScript picker's `/ghostwire/scripts/`
-- the two interpreters share nothing) and get uploaded to either slot over
Grove or Wi-Fi, authenticated the same way command triggers are. A slot
persists across P4 reboots (NVS, same namespace/pattern as the session key);
upload is rejected while that slot is currently running.

```text
GW1,U,<seq>,<slot>,<total_len>,<crc32>            Cardputer: begin upload
GW1,V,<seq>,<accepted>,<crc32>                     P4: begin-ack (room / not busy)
GW1,D,<seq>,<chunk_hex>,<crc32>                    Cardputer: one data chunk, repeated
GW1,F,<seq>,<nonce>,<tag_hex16>,<crc32>            Cardputer: finish + tag
GW1,K,<seq>,<accepted>,<crc32>                     P4: final accept/reject (reused from the command channel)
```

A script can be up to `PAYLOAD_SCRIPT_MAX_BYTES` (512) bytes, comfortably
larger than `GHOSTWIRE_GROVE_MAX_LINE` (96), so it crosses Grove as a short
burst of `D` chunks (28 raw bytes each, hex-encoded -- hex because a script
may itself contain commas, e.g. `PORT_SCAN`'s port-list argument, which
would otherwise break this comma-delimited framing) between one `U`/`V`
begin exchange and one `F`/`K` finish exchange. There's no chunk index or
per-chunk ack: Grove is a reliable ordered UART stream, not a lossy link
(measured zero CRC errors over extended runs earlier in this project), so
chunks simply accumulate in receive order, and a length mismatch at `F`
rejects the whole upload rather than attempting partial recovery -- the
Cardputer just retries the whole transfer.

The authenticated message is `slot_byte || nonce_be32 ||
sha256(script_bytes)` (37 bytes) -- a hash of the script rather than the
raw bytes themselves, so `ghostwire_auth_compute_tag`/`_verify_tag`'s
existing internal buffer (already sized for every message shape this
protocol uses) never needs to grow to fit an up-to-512-byte script,
regardless of how large a script gets. The hash still cryptographically
binds the accepted script to the tag: a script tampered with in transit
hashes differently, so its tag no longer verifies. Wi-Fi's equivalent:

```text
POST /v1/payload   {"slot": 0, "nonce": 3819204457,
                     "tag": "a1b2c3d4e5f6a7b8", "script": "INTERNET_CHECK\n"}
                    -> {"accepted": true}
```

Same replay cache as the command channel (a captured-and-resent upload
shouldn't silently re-commit), same "malformed request gets HTTP 400,
well-formed-but-rejected gets 200 with `accepted:false`, and the two aren't
distinguishable from the response" rules as `/v1/command`.

## Scan-loot extraction

`PORT_SCAN` findings accumulate into a small in-RAM log (`LOOT_MAX_ENTRIES`
= 64 entries, deduplicated by `(ip, port)`) across every payload run since
boot -- unlike the per-run `finding_count` telemetry above, this doesn't
reset each scan. RAM-only by design: it changes on every scan, and the
P4's 24KB NVS partition/write-wear budget isn't worth spending on data
that's meant to be extracted promptly rather than survive a power cycle --
a P4 reboot clears it.

```text
GW1,X,<seq>,<nonce>,<tag_hex16>,<crc32>           Cardputer: request the loot log
GW1,N,<seq>,<count>,<crc32>                        P4: entry count
GW1,E,<seq>,<ip>,<port>,<crc32>                    P4: one entry, sent <count> times
```

The authenticated message is `nonce_be32` alone -- there's no slot concept
here. Unlike script upload/command triggers, an unauthorized or unpaired
request doesn't get a distinct rejection: it just gets an empty log
(`count` 0 / `{"entries":[]}`), the same response a genuinely empty log
would produce, so the two aren't distinguishable from the wire either.
Wi-Fi's equivalent:

```text
POST /v1/loot   {"nonce": 3819204457, "tag": "a1b2c3d4e5f6a7b8"}
                -> {"entries": [{"ip": "192.0.2.50", "port": 80}, ...]}
```

The Cardputer's PoE Companion screens save an extraction to the SD card as
a CSV (`/ghostwire/logs/poe_loot_<NNNN>.csv`, same convention as the
Host Discovery/Port Scan screens' own CSV exports) rather than feeding the
Familiar's lifetime Loot Board counters -- this is raw recon output, kept
separate from that engagement layer.

## Wi-Fi command channel

```
POST /v1/command   {"slot": 0, "nonce": 3819204457, "tag": "a1b2c3d4e5f6a7b8"}
                    -> {"accepted": true}
```

Same session key and clock offset as Grove pairing above -- no separate
Wi-Fi pairing exists. The difference is the authenticated message: Grove's
`C` frame authenticates just the slot byte, which is fine for a physically-
wired link, but Wi-Fi sniffing is a realistic threat, and a bare `(slot,
window)` tag is otherwise replayable for its whole validity window (a
genuine second press of the same slot in the same window would produce an
*identical* tag, so a cache keyed on the tag alone can't tell a legitimate
repeat from a captured-and-resent one). Wi-Fi's authenticated message is
instead `slot_byte || nonce_be32`, where the Cardputer generates a fresh
random `nonce` per request -- every legitimate send gets a unique tag, so
only an exact replay of a previously-accepted `(slot, tag)` pair is
rejected (a small ring-buffer cache on the P4, populated only after a tag
has already verified valid, so it can't be poisoned by garbage requests).
`ghostwire_auth_compute_tag`/`_verify_tag` are unchanged and generic over
message content; only this endpoint uses the longer message, and only this
endpoint carries a replay cache -- Grove's `C`/`K` frames are untouched.

Malformed requests (bad JSON, missing/non-numeric fields, a tag that isn't
16 hex characters) get HTTP 400. A well-formed request that's rejected for
any other reason (bad tag, not paired, replay, busy) is a normal 200 with
`{"accepted": false}` -- deliberately indistinguishable from each other in
the response, same reasoning as the Grove `K` frame.

## Event endpoint

`GET /v1/events` upgrades to a WebSocket. Server-to-client messages are UTF-8
JSON objects with this envelope:

```json
{
  "protocol_version": 1,
  "type": "ethernet.link",
  "sequence": 2,
  "data": {"up": true}
}
```

The server sends `companion.ready` immediately after connection, followed by
events when Ethernet link or address state changes. Sequence numbers increase
for the current boot and may restart at zero after reboot. Clients must treat
unknown event types as optional and reconnect with backoff after disconnection.

Version 1 event sockets are read-only. Text or binary commands are rejected
with an `unsupported` error event -- the authenticated command channel is
`POST /v1/command` (above), not this socket.

## HTTP behavior

- Responses include `Cache-Control: no-store`.
- Unknown paths return HTTP 404.
- The status endpoint permits `GET` only; `/v1/command`, `/v1/payload`, and
  `/v1/loot` permit `POST` only.
- JSON payloads are bounded by the firmware; clients must also impose limits.
