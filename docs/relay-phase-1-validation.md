# Ghostwire Relay Phase 1 validation

Phase 1 passes when the Relay and Cardputer satisfy this checklist on real
hardware. Compilation alone is not a hardware validation.

## Hardware progress (2026-08-11)

Test hardware:

- M5Stack Unit PoE-P4, Ethernet MAC/USB serial `30:ED:A0:EA:B9:70`
- M5Stack Cardputer ADV, USB serial `28:84:85:75:6A:70`
- Relay address during testing: `192.168.8.106`

Validated on hardware:

- P4 Ethernet, DHCP, `_ghostwire._tcp` mDNS discovery, `/v1/status`, and the
  read-only WebSocket endpoint work.
- Cardputer discovers and validates the companion automatically.
- RGB transitions work: boot indication, blue without Ethernet, green with
  confirmed internet reachability, purple during Cardputer polling, then back
  to green approximately 30 seconds after leaving the screen.
- Extended status telemetry is visible on the Cardputer, including LAN,
  internet, firmware, uptime, link, system, LED, and Ghostwire state.
- The Cardputer can power the P4 through a complete HY2.0-4P Grove cable when
  its adjacent switch is set to `5V OUT`; the P4 must not simultaneously receive
  USB or PoE power.
- Grove UART P4-to-Cardputer signalling is electrically correct and clean. A
  diagnostic run received 39 CRC-valid frames with `CRC 0` and maximum spacing
  of 1051 ms before the temporary USB debug logger blocked the Cardputer loop.

Current firmware state:

- P4 firmware `0.4.0` is flashed with 115200-baud Grove heartbeats, sequence
  matching, CRC32, acknowledgement tracking, and the cross-core timer/critical-
  section deadlock fix.
- Cardputer is flashed with the matching acknowledgement client, local Grove
  counters, the blocking per-frame USB logging removed, and a five-second link
  display timeout.
- Both firmware targets compile; the existing native suite passes all 13 tests.

Next test:

1. Keep Cardputer USB power connected and disconnect P4 USB/PoE power.
2. Set the Cardputer Grove switch to `5V OUT` and connect the Grove cable.
3. Leave Scout -> Network -> PoE Companion open for at least one minute.
4. Expected: `RECEIVING` remains continuous, sequence and frame counts increase
   once per second, age stays near or below 1100 ms, CRC remains zero, and
   maximum gap remains near 1000-1100 ms.
5. With non-PoE Ethernet also connected, confirm the relayed status shows both
   local `RX` and P4 `ACK` state.

The one-minute Grove run above is still pending; do not mark the bidirectional
Grove link hardware-validated until it passes.

## Grove drop-out root cause (2026-08-11)

Live USB-serial capture (rate-limited, one line/second) during a hardware run
found the drop-out: it is not a wiring or protocol problem. `PoeCompanionService`
runs mDNS discovery synchronously on the main loop, and `mdns_query_ptr`'s
hard-coded 3000 ms timeout plus the 1500 ms fallback `queryHost` call can block
`loop()` (and therefore `groveCompanionLink.update()`) for up to ~4.5 s. A
captured run showed `GroveCompanionLink::maximumGapMs()` jump to 5324 ms (just
over the 5 s Cardputer-side link timeout) at the exact sample where
`PoeCompanionService::state()` flipped to `NotFound`, and this repeated every
10 s poll cycle while the companion stayed unfound, living right at the edge of
the default 256-byte `HardwareSerial` RX buffer's ~1.5 s capacity.

Fixes applied:

- `GroveCompanionLink::begin()` now calls `serial_.setRxBufferSize(1024)`
  before `begin()`, so a multi-second stall elsewhere in the loop no longer
  drops frames.
- `PoeCompanionService::poll()` now backs off 30 s between full discovery
  attempts while the companion stays unfound, instead of re-running the
  blocking mDNS scan on every 10 s poll.

Re-verified on hardware after the fix: a 180-second capture held `RECEIVING`
continuously with zero CRC errors and zero disconnects, including one stall of
6455 ms (worse than the failure case above) fully absorbed by the larger
buffer.

Separate, still-open issue observed during the same capture: `PoeCompanionService`
stayed in `Error` state for the whole 180 s run (mDNS discovery succeeding but
the status fetch failing/timing out), and the Grove sequence number trailed the
Cardputer's cumulative valid-frame count by a constant offset, evidence the P4
rebooted at some point in this session. This is consistent with the documented
brownout risk of powering the P4 solely from the Cardputer's Grove 5 V-out
during Wi-Fi activity, and needs its own investigation before Phase 1 is
considered validated.

## Companion Mode command channel (2026-08-12)

Hardware-validated on the same P4/Cardputer pair as above, Grove cable
connected, P4 on Ethernet with internet access (required for its NTP sync,
which the clock-offset handshake depends on):

- Re-pairing after flashing correctly reported "the P4's clock isn't synced
  yet" while the P4 had no Ethernet, then succeeded with a logged clock
  offset once Ethernet was connected and the P4 had NTP-synced.
- `Run slot 0`/`Run slot 1` from the Cardputer's Relay detail screen (Tab
  menu, paired state) triggered the P4 payload engine remotely: slot 1
  (port scan) showed the amber "running" LED exactly as a physical long
  press does, then green on completion, and the detail screen's status line
  showed `Slot: ok (N)` before reverting to idle. Slot 0 (internet check)
  completes too quickly to observe the LED transition, same as a real short
  press.
- The physical button still triggers both slots unchanged, confirming the
  shared `payload_button_task` dispatch path (real press vs. Grove command)
  didn't regress.

## Relay screens revision + Wi-Fi command channel (2026-08-12)

Hardware-validated on the same pair, same session:

- Entering Relay from the Network menu is now instant (previously blocked
  for several seconds on a synchronous network fetch).
- Detail screen scrolls through all 8 rows via up/down with nothing cut off.
- `0`/`1` fire a payload slot directly from the *summary* screen (no detail
  visit needed), both as direct hotkeys and via the Tab menu.
- Found and fixed a real P4 crash during this pass: the HTTP server task's
  stack was overflowing (`Guru Meditation Error: ... Stack protection
  fault`) roughly every 30s once `build_status_json()` grew, visible on
  hardware as the LED cycling green/amber/blue and Wi-Fi discovery silently
  failing -- see CHANGELOG. Bumping the httpd task's stack to 8192 bytes
  and reflashing resolved it; watched for several minutes afterward with no
  further panics.
- With Grove pairing established, then the Grove cable *disconnected*
  (Wi-Fi/Ethernet still up), `0`/`1` still triggered the P4's payload
  engine over `POST /v1/command` -- same amber/green LED behavior, and the
  detail screen's status row updated from the network fetch instead of
  Grove, confirming `PoeCompanionService::payloadRunState()`/
  `payloadFindingCount()` work the same as Grove's equivalents.
- `Tab -> Forget pairing key` correctly cleared pairing: `Run slot 0/1`
  disappeared from both screens' Tab menus and the detail row reverted to
  "none (Tab to pair)".
- The physical button still works unchanged.

Not separately hardware-tested: firing the same slot twice in quick
succession over Wi-Fi (nonce-based non-collision) and a deliberately
replayed/malformed request being rejected -- these follow directly from
`command_replay_cache_check_and_record()`/`ghostwire_auth_verify_tag()`
gating `payload_trigger_slot()` in `command_handler()`, verified by code
inspection rather than fault injection, same as the Grove slice.

Not separately hardware-tested this pass: a deliberately invalid/replayed
command being rejected (`K` accepted=0) without running the payload -- this
follows directly from `ghostwire_auth_verify_tag()` gating
`payload_trigger_slot()` in `process_grove_command_request()`, the same
function proven to accept a valid tag above, and was verified by code
inspection rather than fault injection.

## Grove-priority companion telemetry

Grove now carries the same live telemetry as the network status fetch (see
`shared/protocol/grove_link.h`'s status/identity frames) and the PoE
Companion screen prefers it whenever fresh. This still needs a hardware pass
covering all three scenarios; do not mark it validated until all three pass:

1. **Grove-only.** Disconnect the P4's Ethernet cable (or turn off Cardputer
   Wi-Fi) so network discovery can't succeed, with the Grove cable connected.
   Expect: the PoE Companion screen still shows the full detail panel (LAN,
   internet, firmware, uptime, temperature, heap, LED state) sourced from
   Grove alone, the top status line reads "Grove companion online", and the
   `[Grove]` tag shows on the LED/GW/Grove line. IP/gateway/DNS should show
   "no network path" rather than stale/blank values.
2. **Network-only.** Disconnect the Grove cable with Wi-Fi/Ethernet
   connected on both ends. Expect unchanged pre-existing behavior: the panel
   populates from the network fetch, the `[Net]` tag shows, and Grove's own
   `RECEIVING`/`WAITING` line reflects no heartbeat.
3. **Both connected.** Confirm the `[Grove]` tag shows (Grove takes
   priority) and that `poeCompanionService.lastRefreshMs()` stops advancing
   every 10 s -- confirm via the diagnostic Serial capture technique used
   above, watching for polls roughly a minute apart instead of every 10 s.

Also re-confirm Grove stability isn't regressed by the extra per-second S
frame traffic: watch `crcErrors()`/`maximumGapMs()` stay flat over a few
minutes, the same way the buffer-size fix was verified above.

## Functional telemetry

1. Power the Relay with Ethernet connected and open Network -> Relay on the
   Cardputer.
2. Press `R`; confirm discovery completes and firmware, uptime, link speed,
   duplex, gateway, DNS, internet reachability, temperature, heap, reset reason,
   RGB state, and sample age are populated.
3. Confirm the Relay changes to purple while the screen polls every 10 seconds.
4. Leave the screen and confirm purple expires after approximately 30 seconds.

## Recovery matrix

| Interruption | Expected Relay behavior | Expected Cardputer behavior |
| --- | --- | --- |
| Remove Ethernet | Blue after link loss | First failed poll is stale; third is offline |
| Reconnect Ethernet | Cyan, then green if probe succeeds | Recovers on a later poll without rediscovery |
| Renew/change DHCP lease | DNS-SD/status remain available at the new address | `R` rediscovers the new endpoint |
| Reboot Relay | Boot pulse, then normal network colour | Stale/offline while unavailable, then recovers |
| Disable Cardputer Wi-Fi | Relay leaves purple after 30 seconds | Screen requests Wi-Fi; reconnect and press `R` |
| Leave/reopen Relay screen | Purple expires/returns with contact | Cached endpoint polls or `R` rediscovers |

## Endurance run

From a computer on the same network:

```sh
python3 tools/relay_endurance.py
```

The default run lasts 24 hours and polls every 10 seconds. It fails after more
than three consecutive invalid or unavailable responses. A pass requires:

- no unexpected Relay reboot (`reboots=0`);
- no consecutive-failure-limit breach;
- valid bounded JSON for every successful response;
- minimum free heap remains stable enough to show no continuing downward trend;
- successful manual Ethernet removal/reconnection during a separate recovery
  test (intentional interruptions should not be included in the endurance run).

A shorter smoke run is:

```sh
python3 tools/relay_endurance.py --duration 300
```
