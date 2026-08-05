# Ghostwire roadmap

## Product direction

Ghostwire 0.5 builds field reliability on the coherent companion introduced in
0.4. Its core loop remains **Observe -> Scout -> Record**. The Familiar
connects those stages by reacting to discoveries and changes; it is not merely
another tool in the menu. Ghostwire should complement workstation tooling by
collecting bounded, explainable evidence in the field.

The mission-led home screen and unified Evidence browser are implemented in the
current development build. Screen/controller extraction from `src/main.cpp` is
complete (see `docs/screen-extraction.md`). The operation coordinator, named
network profiles, and first-run guide are now active; repeated hardware
reliability testing is the remaining 0.5 release gate.

## 0.5 — Field Reliability

- [x] Central inventory of long-running Wi-Fi, BLE, network, audio, remote, and
  firmware-update operations.
- [x] Tested conflict policy and clear on-device refusal when a requested radio
  mode cannot safely coexist with active work.
- [x] Active operation visibility in System Diagnostics and the header status.
- [x] Up to five named Wi-Fi/network profiles with deliberate connect, rename,
  default selection, migration, and deletion.
- [x] Skippable, replayable first-run guidance for the Observe -> Scout ->
  Record workflow, Familiar, SD evidence, navigation, and profile storage.
- [x] First receive-side Mesh Field Client milestone: bounded node/message
  state, identity and position decoding, duplicate suppression, and navigable
  dashboard, directory, detail, and inbox views.
- [x] GNSS-relative mesh radar, node range/bearing, device-metrics decoding,
  and debounced microSD persistence for the bounded client state.
- [x] Receive-only EU_868 channel profiles with public LongFast plus validated
  SD-backed private AES-128/AES-256 keys and on-device hash/status visibility.
- [ ] Hardware soak pass covering repeated start/stop, cross-radio transitions,
  low-memory behaviour, and recovery after interruption.

The automated release-candidate pass now builds both targets as GNU C++17 and
tests every operation label plus full conflict-policy symmetry. Real-device
radio transition and endurance checks remain deliberately open until exercised
interactively on hardware.

This roadmap favours features that are useful during authorised assessment,
network administration, or field diagnostics on the Cardputer ADV. Inspiration
may come from projects such as Bruce, but features are designed for Ghostwire's
hardware and workflows rather than copied wholesale.

## Selection rules

A proposed feature moves into the active roadmap only when it:

1. solves a repeatable field problem;
2. fits the onboard hardware, or has a clearly identified optional accessory;
3. has a bounded, understandable safety model;
4. can save, export, or otherwise produce a useful result;
5. can be tested on real hardware without destabilising existing radio modes.

Priority rises when a feature completes an existing workflow, reuses proven
services, works offline, and benefits both security and administration. It falls
when a feature is mainly theatrical, duplicates another tool, depends on
unowned hardware, or creates disproportionate legal, RF, or reliability risk.

## Now — complete existing workflows

### Completed quick wins

- [x] NTP system-clock synchronisation alongside the existing GNSS source.
- [x] Offline QR generation for typed text, URLs, and short notes.

### 1. Full passive Wi-Fi capture — implemented, hardware validation pending

- Extend the current probe/EAPOL capture foundation into selectable management,
  data-metadata, and full raw PCAP modes.
- Add channel lock/hop controls, capture filters, dropped-frame counters, and
  clear storage estimates.
- Reuse Log Sessions for preview, metadata, export, and deletion.
- Keep capture passive; transmitting remains a separate confirmed operation.

**Done when:** Wireshark opens the resulting PCAP reliably, long captures do not
starve the UI, and radio state is restored after leaving the tool.

### 2. BLE capture and protocol inspection — implemented, hardware validation pending

- Add continuous BLE logging with address type, advertisement type, service
  data, manufacturer data, and raw payload.
- Add focused decoders only for common, useful protocols discovered in real
  captures; keep raw bytes available when no decoder matches.
- Add filters for name, address, company, service UUID, and RSSI.

**Done when:** repeated scans do not inflate unique-device totals, CSV output is
stable, and Wi-Fi/BLE mode transitions survive repeated use.

### 3. Cyber Familiar encounter integrity — implemented

- Track compact hashes of recently seen Wi-Fi/BLE identities so rescans count
  encounters accurately without storing raw device identifiers indefinitely.
- Offer a deliberate, one-time import of compatible Ghostwire logs with a
  preview and duplicate protection.
- Add reset/export controls for familiar progress and journal data.

**Done when:** rescanning the same environment cannot farm discovery totals and
log import is idempotent.

### 4. Chameleon Ultra workflow completion — identity workflow implemented

- Saving and reloading discovered identity records is implemented.
- Confirmed identity emulation is implemented for EM410x and recognised
  MIFARE Classic Mini/1K/4K anti-collision identities. Full authenticated card
  data cloning remains future work and is not represented as complete.
- Require a target summary and explicit confirmation before writes or
  emulation.

**Done when:** read, save, load, and authorised clone/emulate round trips are
verified against test tags.

## Next — high-value operator utilities

### Mesh Field Client progression

- [x] Persist the bounded node database and message journal to microSD.
- [x] Combine local GNSS with received positions for distance/bearing and a
  field radar view; decode device battery and channel telemetry.
- [x] Add deliberate EU_868 receive channel/key configuration before any
  transmission. Other regions remain unavailable on the 868 MHz cap.
- [x] Add standards-compatible broadcast text with packet IDs, selectable
  loaded channel, adjustable 1-7 hop limit, channel-activity checks, and an
  airtime guard.
- [x] Add persistent long/short node identity, a dedicated Mesh Settings page,
  and deliberate NodeInfo advertisement as a `CLIENT_MUTE` endpoint.
- Add direct messages, acknowledgements, and position sharing only after
  broadcast interoperability and duty-cycle testing.

The client behaves as a quiet endpoint: it can receive and originate text but
does not claim router behaviour or rebroadcast other nodes' traffic.

### 5. Network socket workbench

- TCP client and TCP listener with text/hex views, configurable port, bounded
  history, timestamps, and save-to-log.
- UDP send/listen mode if the TCP implementation proves stable.
- Allow a host/port to be handed in from Host Discovery and Port Scan.

This fills a practical diagnostic gap without pretending to be a full terminal
emulator.

### 6. Time and clock management — partially complete

- NTP synchronisation over an active Wi-Fi connection is complete.
- Preserve GNSS sync and show the active time source, age, and uncertainty.
- Add manual time entry for offline logs and explicit timezone/display settings;
  keep stored log timestamps in UTC.

### 7. BLE DuckyScript transport

- Reuse the reviewed USB DuckyScript parser and confirmation screen through the
  existing BLE keyboard service.
- Clearly show the selected transport and connected host before execution.
- Keep unsupported commands rejected rather than silently approximated.

### 8. QR utility — initial generator complete

- Offline typed-text and URL generation is complete. Presets for Wi-Fi
  credentials and selected diagnostic summaries remain.
- Never expose secrets without an explicit reveal/confirm step.
- Add QR decoding only if a supported external camera workflow is identified;
  the onboard hardware cannot capture images.

### 9. Audio spectrum and signal diagnostics

- Add a lightweight microphone spectrum/spectrogram view with peak frequency,
  level history, and optional CSV summary.
- Treat it as a diagnostic visualiser, not calibrated test equipment.

### 10. Wi-Fi auto-connect and template sync

- On boot, if a saved Wi-Fi profile exists, connect automatically (still an
  explicit opt-in setting via `autoConnectWifi`/`saveWifiCredentials`, default
  unchanged otherwise -- this uses the existing, already-implemented saved-
  credential mechanism, not anything new).
- Once connected, check the `sd-card-files/` templates against the public
  GitHub repo. Show what differs; the operator confirms before anything is
  written to the card. No silent auto-apply -- this is a notification-and
  -confirm flow, not an unattended updater.

### 11. Signed firmware OTA updates

**Status: implemented, manual trigger only.** See `docs/ota-updates.md` for
the full design and known gaps. Summary of what shipped vs. what was
originally sketched here:

- Triggered from **Settings > Firmware Update**, not yet from item 10's
  "connected, check in" moment -- item 10 itself isn't built yet, so there's
  nothing to hang an auto-check off of. `setup()`'s `autoConnectWifi` block
  is the natural place to add that hook once item 10 lands.
- `board_build.partitions = default_8MB.csv` already provisions `app0`/`app1`
  (ota_0/ota_1, 0x330000 each) and `otadata` -- every device already flashed
  via USB already has a free, ready OTA slot. No repartitioning and no
  one-time migration reflash needed; this was the main cost the SD/device
  encryption path would have carried and it simply isn't here.
- Signing uses **ECDSA P-256, not Ed25519** -- the bundled mbedTLS 2.28.7
  build has no EdDSA support compiled in (confirmed by inspecting the SDK's
  mbedTLS config directly). P-256 is fully supported and gives the same
  security property; `release.yml` signs with `openssl dgst -sha256 -sign`
  and the public key is embedded in firmware.
- Checks the public repo's latest release tag against the running version,
  notifies, and requires operator confirmation before downloading anything.
- Streams the release's `firmware.bin` asset directly into `Update.write()`
  in chunks, computing SHA-256 as it goes; never buffers the full image in
  RAM (no PSRAM).
- Verifies the ECDSA-P256 signature before calling `Update.end()`/committing
  -- an unsigned or tampered image is rejected before it's written.
- After rebooting into the new image, an NVS boot-attempt counter plus (if
  available) ESP-IDF's native pending-verify mechanism roll back
  automatically to the previous OTA partition if the new image never
  reaches a healthy checkpoint. See `docs/ota-updates.md`'s "Boot safety"
  section for the honest limits of this -- bootloader-level rollback
  support on this exact board hasn't been confirmed on real hardware yet.
- Firmware installation is refused while another tracked operation or capture
  is active, with the conflicting operation named on screen.
- Bootloader/partition-table changes remain USB-reflash-only; this covers the
  application partition, not literally everything.

**Done when:** a signed test release round-trips end-to-end (check, confirm,
download, verify, flash, reboot, self-validate), an unsigned or tampered
image is rejected before it's written, and a forced bad boot triggers
automatic rollback without operator intervention.

- [x] Signing, verification, and rollback logic implemented; builds clean
      (`pio run -e cardputer_adv`) and native unit tests pass
      (`pio test -e native`).
- [x] End-to-end round-trip completed on real hardware against the signed
      0.4.8 release.
- [ ] Forced-bad-boot rollback exercised on real hardware.
- [x] Refuse-to-start-during-active-capture guard.

## Later — useful, but needs design or soak testing

### 12. ESP-NOW field exchange

- Pair two Ghostwire devices and exchange small logs, configuration bundles, or
  text notes with visible identity and confirmation on both ends.
- Define integrity checking, interruption recovery, and strict receive-size
  limits before implementation.

### 13. Local Web UI

- Read-only dashboards and log download first.
- Mutating actions require authentication, CSRF protection, session expiry, and
  an on-device enable indicator.
- Bind only while explicitly enabled and stop the service on exit or timeout.

### 14. WiGLE-compatible export

- Export wardrive data in a WiGLE-compatible format after validating coordinates,
  timestamps, privacy implications, and schema details.
- Upload is a separate later decision; local export provides most of the value
  without storing credentials or automating disclosure of location data.

### 15. WireGuard client

- Consider only after measuring RAM/flash cost and coexistence with TLS, SSH,
  logging, and the UI.
- Target secure access to an administrator's own network, with explicit tunnel
  state and fail-closed key handling.

### 16. Optional accessory framework

- Detect and configure supported CC1101, nRF24, PN532, or IR-receiver modules
  through a common accessory screen.
- No accessory-specific tool enters the active roadmap until the exact module,
  wiring, ownership, and hardware test setup are available.

## Shelved or deliberately excluded

- **RF/Wi-Fi/BLE jammers and broad deauth floods:** poor diagnostic value,
  indiscriminate impact, and disproportionate regulatory risk.
- **Beacon/pairing/Pwngrid identity spam:** novelty-heavy and already shown to
  be difficult to verify meaningfully on real targets.
- **Evil portal plus automatic deauth:** a high-risk combined workflow with
  limited advantage over controlled lab tooling.
- **Responder and ARP poisoning:** keep as research candidates, not scheduled
  work, until packet injection, lwIP behaviour, safeguards, and a dedicated lab
  validation plan are proven.
- **JavaScript interpreter:** substantial memory, sandboxing, and maintenance
  cost while the bounded script runner covers the clearest automation need.
- **FM transmission, MouseJack, iButton, and tag hardware features:** no useful
  onboard path; reconsider only with a specific owned accessory and use case.
- **Image viewer:** possible but low value on a 240x135 display compared with
  improving log and capture workflows.

## Release gates

Every roadmap feature must pass a firmware build, relevant native tests, a
dirty-worktree review, and real-device validation. Radio features additionally
need repeated enter/exit testing, Wi-Fi/BLE coexistence checks, bounded memory
behaviour, stop-on-back handling, and recovery after failure or reset. Release
notes must distinguish compiled, simulated, and hardware-verified behaviour.
