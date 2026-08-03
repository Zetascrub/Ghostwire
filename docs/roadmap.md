# Ghostwire roadmap

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

## Later — useful, but needs design or soak testing

### 10. ESP-NOW field exchange

- Pair two Ghostwire devices and exchange small logs, configuration bundles, or
  text notes with visible identity and confirmation on both ends.
- Define integrity checking, interruption recovery, and strict receive-size
  limits before implementation.

### 11. Local Web UI

- Read-only dashboards and log download first.
- Mutating actions require authentication, CSRF protection, session expiry, and
  an on-device enable indicator.
- Bind only while explicitly enabled and stop the service on exit or timeout.

### 12. WiGLE-compatible export

- Export wardrive data in a WiGLE-compatible format after validating coordinates,
  timestamps, privacy implications, and schema details.
- Upload is a separate later decision; local export provides most of the value
  without storing credentials or automating disclosure of location data.

### 13. WireGuard client

- Consider only after measuring RAM/flash cost and coexistence with TLS, SSH,
  logging, and the UI.
- Target secure access to an administrator's own network, with explicit tunnel
  state and fail-closed key handling.

### 14. Optional accessory framework

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
