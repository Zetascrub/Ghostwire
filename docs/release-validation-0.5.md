# Ghostwire 0.5 hardware validation

This worksheet is the hardware release gate for Ghostwire 0.5. Record observed
results; do not infer a pass from a successful build. A test that resets,
freezes, loses its saved result, or leaves another radio mode unusable fails.

## Candidate baseline

| Item | Result |
| --- | --- |
| Candidate code | `76933c3` (`Complete Meshtastic field client workflows`) |
| Roadmap revision | `969ac13` |
| Firmware version | `0.5.0-dev` |
| Native tests | PASS — 13/13 |
| Cardputer ADV build | PASS |
| Application flash | 2,094,753 / 3,342,336 bytes (62.7%) |
| Static RAM | 107,132 / 327,680 bytes (32.7%) |
| USB flash and reboot | PASS on `/dev/ttyACM2` |
| Hardware test date | 2026-08-06 |
| Tester | Zetascrub |

Before and after each block, open **Settings > System > System Diagnostics**.
Record uptime, free heap, minimum heap, last reset, stability events, SD state,
and active operation. Press `E` to export a diagnostic report when an SD card is
present. Development builds also POST the redacted report to
`http://192.168.8.10:8765/diagnostics` when Wi-Fi is connected. Start the local
collector with `python tools/diagnostic_receiver.py`; reports are written under
the ignored `diagnostic-collections/` directory. HTTP failure never prevents the
SD export. A falling minimum heap is expected; steadily falling current free
heap after returning idle is not.

### Initial hardware reading

| Diagnostic | Baseline |
| --- | --- |
| Heap free | 126 KB |
| Heap minimum | 100 KB |
| Last reset | Power on |
| Stability events | 26 (historical baseline; must not increase) |
| microSD | READY / 14,910 MB |
| Operations | Idle |

The existing stability count includes crashes accumulated during development.
This candidate is judged on the delta from 26, not the historical total.

The development HTTP collector was hardware-validated from `192.168.8.198` to
`192.168.8.10`: schema 1 was accepted and stored with 127 KB free heap, 119 KB
minimum heap, Operations idle, stability 26, boot count 251, SD ready, and a
GNSS-synchronised timestamp. The same manual export also wrote its SD report.

Development builds also listen on UDP 8766 for bounded LED/message tests from
`192.168.8.10` only. Run
`python tools/send_led_message.py CARDPUTER_IP "Test message" --color magenta`.
The command accepts named colours or `RRGGBB`, a 250-10000 ms duration, and
1-10 pulses. Release builds do not open the listener. The on-screen delivery
and StampS3A RGB output were hardware-validated; the LED uses GPIO21 with the
ADV-specific GPIO38 power enable.

### Idle soak reading — 15 minutes

| Diagnostic | Result |
| --- | --- |
| Heap free | 126 KB — unchanged |
| Heap minimum | 105 KB reported (initial reading was 100 KB) |
| Last reset | Power on — unchanged |
| Stability events | 26 — unchanged |
| microSD | READY / 14,910 MB |
| Operations | Idle |

The free-heap and reset baselines remained stable. Because an ESP32 boot's
minimum-free-heap watermark cannot increase without a reset, the two rounded
minimum readings are retained as reported and are not used to infer a leak.

## A. Boot, idle, and recovery

- [ ] Cold boot with the GPS/LoRa cap and SD card fitted.
- [ ] Confirm display, keyboard, battery, SD, GNSS UART, LoRa, IMU, and USB HID
      diagnostics are sensible.
- [x] Leave the Familiar/background services idle for 15 minutes; free heap,
      reset state, stability count, SD state, and idle operation state remained
      stable. Idle-animation wake quality confirmation remains below.
- [x] Idle animation woke cleanly and navigation remained responsive, with no
      visible freeze, flicker, or reboot.
- [x] Reboot normally three times and confirm no new stability event. An initial
      ending-count transcription was inconsistent; a controlled follow-up boot
      advanced the counter exactly once from 247 to 248. Final diagnostics were
      127 KB free heap, 120 KB minimum heap, Power on reset, 26 stability events,
      SD ready, and Operations idle.
- [x] While GNSS logging is active, use the global emergency stop and confirm
      the deck returns home with `Operations: Idle`. Recovery diagnostics pass:
      Operations idle, 163 KB free heap, 124 KB minimum heap, and stability
      events unchanged at 26. The GNSS CSV was present and valid in Evidence.
- [ ] Remove and restore power once; confirm settings and bounded Mesh state
      reload without corrupting the SD card.

## B. Radio and operation transitions

Run each row three times. Start the first operation, stop it normally, then
start the second. Repeat once using Back/Escape and once using emergency stop.

| From | To | Normal stop | Back/escape | Emergency stop | Heap recovered | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Wi-Fi scan/connect | BLE scan | Pass | N/A | N/A | Pass | Wi-Fi connected/disconnected/reconnected and BLE detected devices. Fresh post-flash baseline was 127/124 KB; two repeated post-scan reports were identical at 118/104 KB with stability 26. The bounded five-second discovery scan is synchronous, so Back is processed after completion; cancellation is tested with continuous capture below. |
| BLE continuous capture | Wi-Fi scan | Pass | Pass | Pass after fix | Pass | Normal `C` stop returned idle at 115/100 KB. Back/Escape returned idle at 119/100 KB. Initial emergency-stop test exposed an omitted `bleScanner.stop()` and panicked when Wi-Fi started, raising stability to 27. Commit `0b10cc2` fixed the teardown; exact hardware retest connected Wi-Fi without warning or reboot and reported Power on, Idle, 117/104 KB, stability unchanged at 27. |
| Wi-Fi PCAP capture | saved Wi-Fi connect | Pass | Pending | Pending | Pass | Management/full PCAP captured and appeared in Evidence; saved Wi-Fi reconnected and HTTP export reported Power on, Idle, 111/97 KB, stability 27. |
| Guardian | Host Discovery | Pending | Pending | Pending | Pending | |
| War Drive | Wi-Fi connect | Pending | Pending | Pending | Pending | |
| Familiar Patrol | BLE scan | Pending | Pending | Pending | Pending | |
| BLE Keyboard | Wi-Fi scan | Pending | Pending | Pending | Pending | |
| Mesh background receive | Wi-Fi/BLE operations | Pending | Pending | Pending | Pending | |
| GNSS logging | Mesh chat and node requests | Pending | Pending | Pending | Pending | |
| MP3 playback | firmware update check | Pending | Pending | Pending | Pending | |

Expected conflict refusals must name the active operation and leave it running.
After a legitimate stop, the next operation must start without rebooting.

## C. Storage endurance and evidence

- [ ] Start and stop Wi-Fi PCAP, BLE capture, GNSS log, IMU log, LoRa log, and
      War Drive; verify every file is non-empty and visible in Evidence.
      BLE capture and GNSS emergency-stop output are confirmed usable.
- [ ] Preview representative TXT, CSV, LOG, PCAP metadata, and Mesh JSONL data.
      Mesh JSONL currently remains an SD archive rather than an on-device
      preview; verify it externally for this release.
- [ ] Run a 30-minute passive capture while navigating other non-conflicting
      screens. Record drops, ending file size, free heap, and minimum heap.
- [ ] Fill a test SD card until less than 5% remains, repeat a short capture,
      and confirm failure is reported without a reset or damaged prior file.
- [ ] Delete one disposable evidence file through the confirmed UI and verify
      unrelated files remain intact.

## D. Meshtastic interoperability

Use at least one known-good Meshtastic node on EU_868 LongFast. Where possible,
repeat direct-message checks in both directions.

| Behaviour | Result | Notes |
| --- | --- | --- |
| Receive public LongFast broadcast | Pending | |
| Send public LongFast broadcast | Pending | |
| Receive configured private-channel broadcast | Pending | |
| Send configured private-channel broadcast | Pending | |
| Learn peer NodeInfo and public key | Pending | |
| Send and receive PKI direct messages | Pending | |
| Delivered ACK is shown | Pending | |
| Failure/no-ACK state is shown | Pending | |
| Identity request receives a response | Pending | |
| Position request receives a response | Pending | |
| Telemetry request receives a response | Pending | |
| Cardputer GNSS position is received by peer | Pending | |
| Background alert fires once per new message | Pending | |
| Read/unread and recent conversations survive reboot | Pending | |
| `/ghostwire/mesh/messages.jsonl` contains message/delivery events | Pending | |
| Changed peer key is blocked until accepted | Pending | Lab test only |

Record the peer firmware version and role in Notes. A missing response is not
automatically a Ghostwire failure: retain the raw receive log and compare the
peer's channel, region, key, role, and request support.

## E. OTA and release recovery

- [ ] Check for an update while idle; “up to date” or the expected candidate is
      reported without changing partitions.
- [ ] Confirm update is refused while each representative tracked operation is
      active.
- [ ] Complete a signed upgrade, signature verification, reboot, and healthy
      checkpoint.
- [ ] Attempt an unsigned/tampered candidate and confirm it is rejected before
      activation.
- [ ] Exercise forced-bad-boot rollback with USB recovery available. Record the
      boot count, partition before/after, reset reason, and recovered version.

The forced-bad-boot test is the last step. Do not publish its deliberately bad
artifact, and do not run it without a known-good USB recovery build available.

## Usability findings from the soak

- [ ] Add an Evidence category layer for Recent, Wi-Fi, BLE, Network, Mesh &
      GPS, Patrol, and System & Other. Preserve the current newest-first unified
      list as Recent; category views filter the same files rather than changing
      the SD layout.

## Release decision

- [ ] All blocks pass, or every exception is documented and explicitly deferred.
- [ ] No unexplained panic, watchdog, brownout, or rising idle heap loss remains.
- [ ] README and release notes distinguish built, hardware-tested, and deferred
      behaviour.
- [ ] `pio test -e native` and `pio run -e cardputer_adv` pass from a clean tree.
- [ ] Signed release assets and SD-card template are generated from the tagged
      commit.

Final decision: **Pending**
