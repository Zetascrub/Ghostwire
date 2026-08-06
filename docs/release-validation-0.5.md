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
| Hardware test date | Pending |
| Tester | Pending |

Before and after each block, open **Settings > System > System Diagnostics**.
Record uptime, free heap, minimum heap, last reset, stability events, SD state,
and active operation. Press `E` to export a diagnostic report when an SD card is
present. A falling minimum heap is expected; steadily falling current free heap
after returning idle is not.

## A. Boot, idle, and recovery

- [ ] Cold boot with the GPS/LoRa cap and SD card fitted.
- [ ] Confirm display, keyboard, battery, SD, GNSS UART, LoRa, IMU, and USB HID
      diagnostics are sensible.
- [ ] Leave the Familiar/background services idle for 15 minutes; verify input
      remains responsive and the idle animation wakes cleanly.
- [ ] Reboot normally three times and confirm no new stability event.
- [ ] While a harmless logging operation is active, use the global emergency
      stop and confirm the deck returns home with `Operations: Idle`.
- [ ] Remove and restore power once; confirm settings and bounded Mesh state
      reload without corrupting the SD card.

## B. Radio and operation transitions

Run each row three times. Start the first operation, stop it normally, then
start the second. Repeat once using Back/Escape and once using emergency stop.

| From | To | Normal stop | Back/escape | Emergency stop | Heap recovered | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Wi-Fi scan/connect | BLE scan | Pending | Pending | Pending | Pending | |
| BLE continuous capture | Wi-Fi scan | Pending | Pending | Pending | Pending | |
| Wi-Fi PCAP capture | saved Wi-Fi connect | Pending | Pending | Pending | Pending | |
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

## Release decision

- [ ] All blocks pass, or every exception is documented and explicitly deferred.
- [ ] No unexplained panic, watchdog, brownout, or rising idle heap loss remains.
- [ ] README and release notes distinguish built, hardware-tested, and deferred
      behaviour.
- [ ] `pio test -e native` and `pio run -e cardputer_adv` pass from a clean tree.
- [ ] Signed release assets and SD-card template are generated from the tagged
      commit.

Final decision: **Pending**

