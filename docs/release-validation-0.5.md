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

Development builds also export once automatically per boot, with no operator
action: right after the boot animation, a one-shot flag is set and cleared the
first time Wi-Fi is connected that session -- immediately if auto-connect is
on, or whenever Wi-Fi is next connected manually if it isn't. It reuses the
same `exportSystemDiagnostics()` path as the `E` key, so it writes an SD
`diagnostics_NNNN.txt` and POSTs to the collector exactly like a manual
export; on a successful HTTP send the on-screen dev message banner reads
"Diagnostic sent" for about 1.8s. This is a convenience for a soak session
already producing frequent reboots (e.g. across Section B's normal/Back/
emergency-stop passes) -- it does not replace the heap_soak/operation_events
CSVs above, which still cover everything between boots.

Manual snapshots are optional now: development builds with an SD card present
also write two CSVs for the whole boot session, no operator action required.
Pull both at the end of a soak session (Files browser, or a card reader)
instead of transcribing Diagnostics before/after every block:

- `/ghostwire/logs/heap_soak_NNNN.csv` -- one row a minute:
  `timestamp_utc,uptime_ms,heap_free_kb,heap_min_kb,max_loop_gap_ms,
  battery_pct,battery_v,sd_free_mb,operation,stability_events`. It's a
  continuous trend line rather than two point-in-time samples, so it also
  catches a slow leak that only shows up between the blocks below.
  `max_loop_gap_ms` is the longest gap between two `loop()` iterations in
  that minute -- a spike there is a stall/near-freeze even if nothing
  visibly crashed. `sd_free_mb` only refreshes roughly every 10 minutes
  (an FatFs free-space query can itself stall on a large/fragmented card,
  so it isn't cheap enough to poll every sample).
- `/ghostwire/logs/operation_events_NNNN.csv` -- one row per actual
  start/stop transition, not per interval:
  `timestamp_utc,uptime_ms,operation,transition,heap_free_kb,heap_min_kb`.
  Covers every `OperationCoordinator`-tracked operation plus Wi-Fi
  association and Mesh receive (neither is coordinator-gated). This is
  section B's "heap recovered" column produced automatically for every
  transition during the whole soak, not just the rows exercised by hand.

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
1-10 pulses. One solid alert is the default and lasts for the requested
duration. The ADV shares LED power with the backlight, so solid alerts
temporarily use full backlight brightness.
Omit the message for an LED-only test; remote commands do not
wake the UI or reset its idle timer. Release builds do not open the listener. The on-screen delivery
and StampS3A RGB output were hardware-validated; the LED uses GPIO21 with the
ADV-specific shared GPIO38 LED/backlight power at the configured brightness.

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

### Operator spot-check — 2026-08-08

Zetascrub exercised every feature area on hardware: scan results matched
expectations, each operation stopped when expected, rescans behaved correctly,
and navigation (including Cards/Compact and the physical `; , . /` cluster)
worked as expected throughout. No heap/diagnostic numbers were captured
alongside this pass, so it is recorded here as a qualitative note rather than
folded into the Pass/Fail columns below -- per this worksheet's own rule, a
row only moves to Pass once its logged result is in hand. The `heap_soak` and
`operation_events` CSVs added below exist specifically to make that logging
automatic for future passes instead of a separate manual step.

## A. Boot, idle, and recovery

- [x] Cold boot with the GPS/LoRa cap and SD card fitted. Qualitative pass
      (2026-08-08): "all is well and as expected," no numeric capture taken.
- [x] Confirm display, keyboard, battery, SD, GNSS UART, LoRa, IMU, and USB HID
      diagnostics are sensible. Qualitative pass (2026-08-08), same note.
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
- [x] Remove and restore power once; confirm settings and bounded Mesh state
      reload without corrupting the SD card. Qualitative pass (2026-08-08),
      same note as above. Reinforced by a separate reboot-mid-Familiar-Patrol
      observation the same day: the checkpointed patrol correctly resumed
      after reboot once reconnected to the same subnet, confirming
      `/ghostwire/assessments/active.json` recovery works, not just settings/
      Mesh state. The resume is silent (no confirmation prompt; `canResume()`
      exists on the service but nothing in the UI currently reads it) --
      intended per the README's documented behaviour, not a bug, just noted
      for awareness.

## B. Radio and operation transitions

Run each row three times. Start the first operation, stop it normally, then
start the second. Repeat once using Back/Escape and once using emergency stop.

| From | To | Normal stop | Back/escape | Emergency stop | Heap recovered | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Wi-Fi scan/connect | BLE scan | Pass | N/A | N/A | Pass | Wi-Fi connected/disconnected/reconnected and BLE detected devices. Fresh post-flash baseline was 127/124 KB; two repeated post-scan reports were identical at 118/104 KB with stability 26. The bounded five-second discovery scan is synchronous, so Back is processed after completion; cancellation is tested with continuous capture below. |
| BLE continuous capture | Wi-Fi scan | Pass | Pass | Pass after fix | Pass | Normal `C` stop returned idle at 115/100 KB. Back/Escape returned idle at 119/100 KB. Initial emergency-stop test exposed an omitted `bleScanner.stop()` and panicked when Wi-Fi started, raising stability to 27. Commit `0b10cc2` fixed the teardown; exact hardware retest connected Wi-Fi without warning or reboot and reported Power on, Idle, 117/104 KB, stability unchanged at 27. |
| Wi-Fi PCAP capture | saved Wi-Fi connect | Pass | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Normal-stop pass numerically confirmed earlier (111/97 KB). Back/Escape and emergency-stop passes exercised in this batch with no observed issue, but not individually numerically captured -- see batch note below the table. |
| Guardian | Host Discovery | Pass | Pass (same action as normal stop; Guardian has no separate stop key, only leaving the screen) | Pass | Pass | Emergency stop mid-watch returned to the main menu with `Operations: Idle`, no panic or reboot -- boot count held at 288 and stability events held at 27 across the pass (before: 119/113 KB; after Host Discovery completed: 115/87 KB). Guardian has no on-screen stop control -- `R` restarts it, and `Q`/`Backspace`/`Escape` is what calls `stopWifiGuardian()`, so normal-stop and Back/Escape are the same action here rather than two independent paths; revisit if Guardian ever gains a distinct stop control. Guardian drops STA association on start via `WiFi.disconnect()` and does not auto-reconnect on stop, so Wi-Fi Connect had to be redone manually before Host Discovery -- expected, not a bug. Export before: 119/111 KB, Operations idle, stability 27. Export after Host Discovery completed: 117/94 KB, Operations idle, stability 27 (unchanged). **Found during this pass:** while Host Discovery was running, the screen's idle timeout fired and the scan visibly paused until the display woke back up. Root cause: `loop()`'s `if (screenSleeping) { ...; return; }` early return (added for the idle-animation screensaver) sat *above* `networkHostScanService.update()`, `networkPortScanService.update()`, `warDriveService.update()`, and the `wifiConnectAttempting` state machine, so all four stopped advancing the moment the screen slept -- Wi-Fi/BLE capture, GNSS, LoRa, Guardian, and Familiar Patrol were unaffected because they're already updated earlier in `loop()`, before that gate. Fixed by moving those four blocks' `update()`/result-draining calls above the gate, leaving only their `currentScreen`-gated redraws below it. Hardware-retested: Host Discovery kept counting hosts with the screen off and the idle animation running. Port Scan and War Drive share the exact same fix. War Drive is confirmed on its own row below; Port Scan has not been independently exercised against a mid-scan screen sleep by any row in this worksheet and remains an open confirming check, low-risk given it's the same code path as Host Discovery and War Drive. |
| War Drive | Wi-Fi connect | Pass | Pass | Pass | Pass | War Drive's unique AP/device counters kept advancing through a screen-timeout sleep and resumed display correctly on wake, confirming the `screenSleeping` early-return fix (see Guardian/Host Discovery row) also covers `warDriveService`. Normal stop: export before 119/115 KB, Operations idle, boot count 289 (fresh boot -- no stability increase), stability 27; export after reconnect 112/98 KB, Operations idle, boot count/stability unchanged. Back/Escape and emergency-stop passes both left War Drive fully stopped (`Operations: Idle`) and Wi-Fi reconnected cleanly, with boot count steady at 290 and stability events steady at 27 across all three exports of that session (119/115 to 117/108 to 117/105 KB) -- no reboot or panic from either exit path. |
| Familiar Patrol | BLE scan | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | All three exit paths exercised in this batch with no observed issue -- see batch note below the table. |
| BLE Keyboard | Wi-Fi scan | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Exercised in this batch with no observed issue -- see batch note below the table. |
| Mesh background receive | Wi-Fi/BLE operations | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Coexistence check (Mesh isn't `OperationCoordinator`-tracked) rather than a strict stop/start pair; exercised in this batch with no observed issue -- see batch note below the table. |
| GNSS logging | Mesh chat and node requests | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Exercised in this batch with no observed issue -- see batch note below the table. |
| MP3 playback | firmware update check | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Pass (qualitative) | Exercised in this batch with no observed issue -- see batch note below the table. |

**Batch note (2026-08-08, rows above marked "qualitative"):** the operator ran
all three exit paths (normal stop, Back/Escape, emergency stop) for these five
rows back to back per the batched instructions above, reporting no crashes,
freezes, stuck states, or incorrect conflict handling across the whole batch.
Per this worksheet's own rule, a row only moves to a full, unqualified Pass
once its logged numeric result is in hand -- no per-pass diagnostic exports
were taken for these rows, so the heap/stability numbers behind "no bugs
found" aren't individually broken out here the way earlier rows are. Partial
quantitative corroboration does exist: the boot-time auto-export (see above)
fired three times during this batch (boot counts 291, 292, 293, each roughly
10s post-boot) with stability events flat at 27 throughout and heap free/min
consistently ~118-120 KB on every fresh boot, so nothing in the batch caused a
reset or a climbing stability count. Pulling `/ghostwire/logs/heap_soak_NNNN.csv`
and `/ghostwire/logs/operation_events_NNNN.csv` off the SD card would let these
move to fully-numeric Pass rows; otherwise this qualitative record stands, same
treatment as the 2026-08-08 operator spot-check note above Section A.

Expected conflict refusals must name the active operation and leave it running.
After a legitimate stop, the next operation must start without rebooting.

## C. Storage endurance and evidence

Marked complete on the operator's judgment (2026-08-08): "happy with what I've
seen so far in previous testing to call this a pass." No fresh numeric capture
was taken for this pass specifically -- same qualitative standing as the
Section A items and the Section B batch note above, not folded into any
Pass/Fail column since there isn't one here.

- [x] Start and stop Wi-Fi PCAP, BLE capture, GNSS log, IMU log, LoRa log, and
      War Drive; verify every file is non-empty and visible in Evidence.
      BLE capture and GNSS emergency-stop output are confirmed usable.
- [x] Preview representative TXT, CSV, LOG, PCAP metadata, and Mesh JSONL data.
      Mesh JSONL currently remains an SD archive rather than an on-device
      preview; verify it externally for this release.
- [x] Run a 30-minute passive capture while navigating other non-conflicting
      screens. Record drops, ending file size, free heap, and minimum heap.
- [x] Fill a test SD card until less than 5% remains, repeat a short capture,
      and confirm failure is reported without a reset or damaged prior file.
- [x] Delete one disposable evidence file through the confirmed UI and verify
      unrelated files remain intact.

## D. Meshtastic interoperability

Use at least one known-good Meshtastic node on EU_868 LongFast. Where possible,
repeat direct-message checks in both directions.

| Behaviour | Result | Notes |
| --- | --- | --- |
| Receive public LongFast broadcast | Pass | Confirmed 2026-08-08 against a second known-good node. |
| Send public LongFast broadcast | Pass | Confirmed 2026-08-08, repeated a few times. |
| Receive configured private-channel broadcast | Deferred | Out of scope for this assessment per operator decision 2026-08-08; revisit in a future pass. Distinct from direct messages -- a shared-PSK channel, not PKI. |
| Send configured private-channel broadcast | Deferred | Same as above. |
| Learn peer NodeInfo and public key | Pass | Initially appeared broken ("direct doesn't register") -- root cause was simply that neither side had exchanged identity keys yet, not a bug. Node Detail's key-status line ("Awaiting identity key" vs "DM ready") correctly identified the gap. Mesh Settings -> **Exchange identity keys** resolved it. |
| Send and receive PKI direct messages | Pass | Confirmed working both directions after the key exchange above. |
| Delivered ACK is shown | Pass | Operator confirmed the "OK" delivered indicator appears once the peer acknowledges. |
| Failure/no-ACK state is shown | Deferred | Out of scope for this assessment per operator decision 2026-08-08; revisit in a future pass. |
| Identity request receives a response | Pass | Implied by the successful key exchange above (the response is what populated the peer's public key). |
| Position request receives a response | Deferred | Out of scope for this assessment per operator decision 2026-08-08; revisit in a future pass. |
| Telemetry request receives a response | Deferred | Same as above. |
| Cardputer GNSS position is received by peer | Deferred | Same as above. |
| Background alert fires once per new message | Pass | Operator confirmed exactly one alert per incoming message, no duplicates or misses. |
| Read/unread and recent conversations survive reboot | Pass | Operator confirmed: read state and conversation history survived a reboot, with the badge correctly showing "2 new messages" for what arrived while it was down. |
| `/ghostwire/mesh/messages.jsonl` contains message/delivery events | Deferred | Out of scope for this assessment per operator decision 2026-08-08; revisit in a future pass. |
| Changed peer key is blocked until accepted | Deferred | Lab test only; out of scope for this assessment per operator decision 2026-08-08. |

Record the peer firmware version and role in Notes. A missing response is not
automatically a Ghostwire failure: retain the raw receive log and compare the
peer's channel, region, key, role, and request support.

**Scope note (2026-08-08):** the six rows marked Deferred above were
deliberately excluded from this assessment by operator decision -- private
channels, request/response telemetry, GNSS position sharing, the no-ACK
failure path, JSONL archive content, and changed-key rejection are considered
beyond this pass's scope and are carried forward to a future assessment
rather than left as an open gap in this one.

## E. OTA and release recovery

- [x] Check for an update while idle; “up to date” or the expected candidate is
      reported without changing partitions. Qualitative pass (2026-08-08):
      "tested OTA a few times and that seems to work" -- covers the normal
      check/report path, no numeric capture taken.
- [x] Confirm update is refused while each representative tracked operation is
      active. Confirmed 2026-08-08 against Familiar Patrol: attempting
      Firmware Update while a patrol was active was refused with an on-screen
      warning naming Familiar Patrol and prompting to stop it first; the
      patrol kept running. Only one representative operation exercised so
      far, not the full set, but the refusal path itself is confirmed
      working.
- [x] Complete a signed upgrade, signature verification, reboot, and healthy
      checkpoint. Qualitative pass (2026-08-08), same "tested a few times" note.
- [x] Attempt an unsigned/tampered candidate and confirm it is rejected before
      activation. Confirmed 2026-08-08 against a real signed-then-tampered
      release (`v0.5.1-otatest1`, a throwaway test build on a dedicated branch
      off `origin/main`, published and deleted the same session): the CI
      pipeline's genuine ECDSA-P256 signature was swapped for a corrupted copy
      on the published release asset, the device attempted the install, and
      the signature check correctly rejected it before activation -- device
      stayed on `0.5.0-dev`, no reboot.
- [x] Exercise forced-bad-boot rollback with USB recovery available. Confirmed
      2026-08-08 against the same `v0.5.1-otatest1` throwaway release used for
      the tamper-rejection check above, with its genuine signature restored:
      the device downloaded, verified, installed, and rebooted into the
      deliberately broken build (forces `esp_restart()` before
      `markBootHealthy()`), then `verifyOtaBootOrRollback()`'s boot-attempt
      counter correctly fell back to the previous partition after repeated
      failed boots, and the device came back up on `0.5.0-dev` on its own --
      USB recovery was available but not needed. First-ever real hardware
      confirmation of this path; previously an explicitly documented gap
      (`docs/ota-updates.md`'s Boot safety section). Precise boot
      count/partition/reset-reason numbers were not individually captured
      this pass -- the qualitative result ("updated and then booted into
      0.5.0-dev") is recorded as-is per this worksheet's usual standard for
      an unembellished pass.

The forced-bad-boot test is the last step. Do not publish its deliberately bad
artifact, and do not run it without a known-good USB recovery build available.

## Usability findings from the soak

- [ ] Add an Evidence category layer for Recent, Wi-Fi, BLE, Network, Mesh &
      GPS, Patrol, and System & Other. Preserve the current newest-first unified
      list as Recent; category views filter the same files rather than changing
      the SD layout.

## Release decision

- [x] All blocks pass, or every exception is documented and explicitly
      deferred. Sections A-E are all fully accounted for as of 2026-08-08 --
      Pass, qualitative Pass, or an explicitly documented Deferred (the six
      Mesh D rows the operator scoped out of this assessment). Zero rows
      remain unaddressed.
- [x] No unexplained panic, watchdog, brownout, or rising idle heap loss
      remains. One real panic was found and fixed during this pass (BLE
      continuous capture -> emergency stop, commit `0b10cc2`, hardware
      retested clean); stability events otherwise held flat across the whole
      soak with no unexplained increase.
- [ ] README and release notes distinguish built, hardware-tested, and
      deferred behaviour. Not specifically audited this pass -- worth a
      read-through before tagging the real release, particularly to fold in
      this worksheet's Deferred Mesh D rows and the two features added
      mid-soak (Familiar LED alerts, dev-build boot diagnostic export).
- [x] `pio test -e native` and `pio run -e cardputer_adv` pass from a clean
      tree. Reverified 2026-08-08 on the current `dev` tree (13/13 native,
      cardputer_adv build succeeds, 63.2% flash / 32.9% RAM).
- [ ] Signed release assets and SD-card template are generated from the
      tagged commit. Not yet applicable -- no real `v0.5.0` tag has been cut.
      (The `v0.5.1-otatest1` throwaway release used for this worksheet's OTA
      rollback/tamper tests is unrelated and was deleted after use, not a
      candidate for the real release.)

Final decision: **Pending** -- down to two mechanical items: a README/release-notes
pass and cutting the actual signed `v0.5.0` release.
