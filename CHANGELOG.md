# Ghostwire changelog

Ghostwire uses `0.2.<subsystem>[.<fix>]` for foundation releases and their
hotfixes. Versions under `0.3.x` are reserved for explicitly authorized
assessment tools built on those verified foundations.

## Unreleased

- Rework Mesh into a conversation-first Meshtastic client. Its home now follows
  a familiar Chats, Nodes, Map, and Settings layout. Channel and direct messages
  are grouped into conversations with compact live-updating threads and
  contextual composition. Node details can start a DM or request a missing
  identity key. Channel profiles and raw radio diagnostics now live under Mesh
  Settings instead of competing with everyday messaging at the top level.

- Add interoperable direct Meshtastic replies from received-message detail.
  Replies target the sender's node ID, use Curve25519-derived AES-CCM PKI,
  request an acknowledgement, and are journalled with the actual recipient.
  Ghostwire now maintains a persistent mesh key pair, derives its node ID from
  the public key, signs broadcast identity/data with XEdDSA, learns peer keys
  from NodeInfo, requests peer NodeInfo replies during identity advertisement,
  and shows whether a node is ready for direct messaging.
  Message lists and details distinguish incoming senders, outgoing recipients,
  channel, and direct-versus-broadcast traffic.

- Add persistent Background client and Message alerts toggles to Mesh Settings.
  Background mode starts Meshtastic reception at boot and keeps it active away
  from Mesh screens; otherwise the SX1262 stops when the operator leaves Mesh.
  Newly decoded text messages can play a short rate-limited two-note cue that
  yields to active audio playback.
- Schedule the message alert's second note after the first note finishes instead
  of asking the speaker driver to play both simultaneously, which previously
  caused one tone to replace the other.

- Add dedicated Mesh Settings for persistent Meshtastic long/short names,
  transmit channel, and hop limit. The page displays the fixed `CLIENT_MUTE`
  role and EU_868 region and can explicitly advertise a standard NodeInfo
  identity packet so other nodes learn Ghostwire's configured names.

- Add the first Mesh chat transmitter: compose broadcast text from the message
  inbox, select any loaded channel, and originate standards-compatible packets
  as a non-repeating endpoint. The persistent hop limit defaults to 7 and is
  adjustable from 1-7; channel-activity checks, bounded retries, and an airtime
  guard protect the shared EU_868 carrier. Sent messages are retained in the SD
  journal, and positioned nodes now carry a map-pin indicator in the directory.

- Add Meshtastic Channel Profiles. Ghostwire always listens for
  public LongFast and can validate/load three private AES-128/AES-256 profiles
  from `/ghostwire/mesh/channels.json`, match frames using Meshtastic's actual
  name-plus-PSK channel hash, and show loaded names/hashes without displaying
  secret key material.

- Begin the receive-side Mesh Field Client: retain a bounded 24-node directory
  and 32-message inbox, decode Meshtastic public-channel identity and position
  payloads, suppress repeated messages, and add dashboard, list, and readable
  detail views while the SX1262 continues listening in the background.

- Extend the Mesh Field Client with a GNSS-relative position radar, per-node
  range and bearing, Meshtastic device battery/channel telemetry, and a
  debounced `/ghostwire/mesh/state.json` snapshot that restores the bounded
  node directory and message inbox after reboot.

- Fix a connected BLE Keyboard panic on Escape by disabling automatic
  re-advertising, explicitly disconnecting peers, waiting for NimBLE's host task
  to drain the event, and only then deleting the HID server state.

- Harden the 0.5 release candidate by aligning device and native builds on
  GNU C++17, expanding operation-policy coverage, and treating Biscuit Pro and
  Chameleon Ultra connections as BLE accessory operations so incompatible
  capture/transmit modes are refused consistently.

- Add a seven-page first-run field guide after the normal boot experience. It
  introduces Ghostwire's Observe -> Scout -> Record loop, the Familiar, SD
  evidence, navigation choice, and profile storage; every page can be skipped,
  earlier pages can be revisited, and Settings can replay it later.

- Add a five-entry Network Profiles workflow under Wi-Fi. Successful
  connections update the matching SSID without replacing unrelated profiles;
  operators can connect, rename, choose the auto-connect default, or explicitly
  delete each entry. Existing single-network credentials migrate on first boot,
  while full stores report the limit instead of silently evicting a network.

- Begin the 0.5 Field Reliability milestone with a central operation inventory
  and tested radio-conflict policy. System Diagnostics now reports the active
  operation, and firmware installation refuses to begin until other ongoing
  work is stopped.
- Derive the System screen's navigation bounds from its live diagnostics rows
  instead of a fixed count, keeping selection correct when conditional rows
  appear.

- Preserve the originating parent-menu selection when backing out of Wi-Fi,
  Network, Settings, and Utility screens instead of jumping to the first item.
- Make headers, list rows, and footers explicitly single-line regions and
  shorten over-width shortcut legends so M5GFX cannot wrap and hide controls.
- Treat short first words as valid card-title wrap points, so labels such as
  `Boot Experience` retain the same large two-line typography as
  `Firmware Update`.

- Move System and About out of Field Kit's Utility Tools and into Settings,
  with System/Clock and About back-navigation returning to their new parent.
  Restore Defaults is now the final Settings item.

## 0.4.8 - 2026-08-05

- Move OTA download/signature buffers from the loop-task stack to checked
  heap allocations and increase the loop-task stack from 8 KiB to 16 KiB.
  This prevents a panic when the installer begins its TLS connection.

## 0.4.7 - 2026-08-05

- Prevent M5GFX's automatic wrapping from splitting the final letter of card
  labels such as `Boot Experience`; card titles now use only Ghostwire's
  intentional word-aware wrapping.
- Report an empty public GitHub Releases feed as `No public release available
  yet` instead of the opaque `HTTP 404` response.
- Abort OTA installation if writing a downloaded chunk to the inactive flash
  partition is incomplete.
- Expand Cyberdeck Idle into three persistent styles: smoother Data Rain,
  Signal Radar, and connected Node Drift. Compose the animated content in a
  temporary off-screen canvas, push complete frames to reduce flicker, and
  release the canvas immediately on wake. Radio activity produces themed pulse
  highlights, and Enter on Idle animation starts an immediate preview.

- Wrap long Cards-mode titles across two large-text lines instead of clipping
  labels such as Observe signals.
- Extend Cards navigation through Wi-Fi, BLE, GPS, Mesh, Scout Network,
  Devices, Tools, and Settings category menus. Data-heavy results, telemetry,
  terminal, evidence, and preference screens retain purpose-built dense views.

- Redesign Cipher Reveal as a distinct themed breach/decryption sequence with
  cycling hex columns, progressively locked code, ICE status, and completion
  meter instead of a shortened variation of the System Console flow.
- Replace the binary Fast boot toggle with persistent Slow, Normal, and Fast
  speeds that scale animations, the title decrypt, status summary, and holds.
  Existing Fast boot users migrate automatically to the new Fast speed.

- Standardise Settings interaction around Up/Down selection and Left/Right
  adjustment, including display, audio, navigation, boot, and connectivity
  preferences. Enter remains reserved for preview or confirmation actions.
- Add the Night City 2077 theme: near-black and dark-chrome surfaces with
  electric-yellow controls, cyan text, and hot-magenta warnings.
- Make Enter preview the selected boot sound or animation directly from its
  style row. Convert Neon Breach, Hacker Terminal, Silly Bounce, and Synthwave
  Grid from fixed RGB colours to the active theme palette so the complete boot
  sequence matches the selected interface theme.

- Add a persistent Compact/Cards navigation choice under Display & Audio.
  Cards mode gives the home, Observe, and Field kit mission menus large
  procedural theme-aware icons, one-at-a-time labels and descriptions,
  Left/Right browsing, page dots, and live operation badges while preserving
  the efficient list interface as Compact mode.
- Keep the Cards-mode Observe radio glyph fully inside its icon frame.
- Treat the physical `; , . /` cluster as Up/Left/Down/Right on navigation
  screens without requiring Fn. Text entry, passwords, chat, and live terminal
  sessions continue receiving the original punctuation characters.

- Begin the Ghostwire 0.4 product overhaul. Replace the eleven-category home
  menu with six mission-led paths: My Familiar, Observe signals, Scout network,
  Evidence, Field kit, and Settings. The Familiar's chosen name and level now
  appear at the front door, and active Guardian/Patrol/survey state is visible
  before opening a subsystem.
- Promote Evidence to a top-level workflow. Its unified SD browser indexes
  ordinary logs and nested Familiar Patrol assessment output, filters recovery
  checkpoints, identifies the source, and preserves preview and deletion
  controls for individual files.
- Refresh on-device and public-facing language around Ghostwire's intended
  role: a pocket network and radio scout that observes, notices change, and
  returns evidence for deeper analysis elsewhere.

- Add Familiar Patrol, an explicitly confirmed unattended scout of the
  connected subnet. It streams host/port evidence to microSD, progresses from
  ICMP discovery through a prioritized 100-port TCP pass, resumes from atomic
  checkpoints after reboot or Wi-Fi loss, rejects a changed subnet, runs in the
  background, and emits CSV, JSON Lines, and Markdown reports. Exhaustive TCP
  scanning remains a separate interactive per-host tool.
- Replace the Familiar's static text face with lightweight procedural pixel
  animation: breathing, blinking, moving ears/tail, discovery radar, and
  distinct host, service, sensitive-service, completion, and idle reactions.
- Add timed Familiar speech bubbles for host, recognised service, warning,
  completion, and error events, using a reliable compact IP fallback such as
  `.134`. Add persistent No sound, Subtle, Chirps, Arcade, and Mystic cue
  themes; cues are rate-limited and suppressed during other audio playback.
- Add Continuous Watch patrol mode with selectable 1-60 minute intervals,
  SD-backed known-host filtering, new-host-only scout passes and reactions,
  cycle tracking, and a visible next-scan countdown.
- Add a Start idle watch now action and restrict live Familiar redraws to the
  animated region, preventing static dashboard elements from flashing.
- Add an isolated Familiar Phrase Lab under Audio. It composes representative
  event phrases from individual SD-card MP3 word clips and reports total
  sequence time before any Familiar integration is considered.
- Add an optional Familiar Voice pack cue style with asynchronous word-bank
  phrases for patrol events, priority-aware chatter suppression, visible patrol
  mood, and different reactions for quiet versus changed watch cycles.
- Add Familiar Guardian under Wi-Fi: a passive management-frame monitor with
  three sensitivity levels, bounded disruption-burst detection, 30-second
  alert cooldown, CSV event summaries, compact deauth/disassociation PCAP
  evidence, live counters, and Familiar warning reactions.
- Remove the 254-address ceiling for confirmed Familiar Patrol scopes while
  retaining it for the existing interactive Host Discovery screen, and correct
  the documented full-port-scan worst case to approximately 35 minutes per
  heavily filtered host.
- Replace the invalid bundled Claude fallback `claude-sonnet-5` with
  Anthropic's documented API model ID `claude-sonnet-4-20250514` in the
  firmware and example AI configuration.
- Remove all bundled test MP3 files and the theme-specific MP3 boot-sound
  hooks. Five synthesized boot sounds remain available without SD assets.
- Chameleon Ultra now automatically retries BLE discovery and connection while
  its screen remains open, stopping as soon as a connection succeeds.
- Fixed intermittent Chameleon connections caused by clearing BLE scan results
  before their selected device record was consumed; initial UART state readback
  now settles and retries without discarding a healthy BLE link.
- Fixed Chameleon device-command status handling: protocol status `0x68` (104)
  is success, not an error. This affected slot staging and state readback.
- Clarified Wi-Fi sniffer RF versus PCAP recording state and retain an
  explicit saved-frame summary after stopping a capture.
- Chameleon slot staging now reads back active slot 8 and emulator mode,
  reports a verification failure when either state was not accepted, and
  labels the action as stage + emulate.

- Expand passive Wi-Fi sniffing with probe, management PCAP, and full PCAP
  modes; hopping/locked-channel control; independent probe CSV and PCAP
  lifecycles; and live frame, byte, and queue-drop telemetry.
- Add asynchronous continuous BLE capture through a bounded callback queue,
  RSSI filtering, address-stable result updates, raw advertisement payloads,
  continuous CSV logging, drop counters, and explicit radio teardown.
- Make Cyber Familiar Wi-Fi/BLE discoveries identity-based through persistent
  rolling hashes rather than resettable scan counts. Add duplicate-safe capture
  log import, readable record export, and a confirmed progress reset.
- Complete the first safe Chameleon identity workflow: uniquely named SD
  records, reload-last, return-to-reader mode, and confirmed slot-8 identity
  staging for EM410x and recognised MIFARE Classic anti-collision identities.
  HF identity emulation is explicitly labelled as not copying protected card
  data, and unsupported SAK types are refused.
- Add two roadmap quick wins: NTP clock synchronisation from the System Clock
  action menu when Wi-Fi is connected, and an offline QR generator for up to
  100 characters of typed text, URLs, or short notes. QR generation uses the
  existing M5GFX renderer and requires no network service or new dependency.
- Replace the loose feature wish-list with a staged, evidence-based roadmap.
  Prioritise completing passive capture, BLE inspection, Cyber Familiar count
  integrity, Chameleon workflows, and practical operator utilities; document
  hardware, safety, usefulness, and real-device release gates; and explicitly
  shelve indiscriminate, novelty-heavy, or unsupported-hardware ideas.
- Move Cyber Familiar interactions and page jumps into the standard
  Tab-triggered action menu, replacing an overflowing shortcut footer with a
  compact `Up/Down: pages   Tab: menu` hint.
- Add Cyber Familiar, an original passive companion inspired by virtual-pet
  and network-observer concepts. It persistently tracks XP, levels, bond, age,
  Wi-Fi/BLE/LoRa discoveries, learned tools, name, and idle preference; reacts
  to radio, GNSS, uplink, battery, and reset activity; keeps a six-entry event
  journal; develops context-sensitive moods; and follows usage-driven evolution
  paths. Its three-page dashboard supports pet/name/idle interactions, and its
  optional animated watch face replaces the normal screen-timeout display.
- Replace the flat Settings list with grouped Display & Audio, Boot Experience,
  and Connectivity submenus. Add four persistent boot animation styles and six
  independent boot sound styles, including original heroic communicator,
  arcade, starship, and mystic tone patterns, with explicit on-device preview
  actions. Theme-provided SD audio remains available as its own sound choice.
- Make Enter cycle boot sound and animation styles, removing the non-working
  adjustment-shortcut hint now that dedicated preview rows are available.
- Add four original boot animations: neon glitch/breach, hacker terminal,
  bouncing silly duck, and synthwave grid styles. All are rendered from compact
  display primitives and support the existing on-device preview action.
- Polish live UI rendering so telemetry, scan progress, connection state,
  terminal output, chat composition, and text-entry fields update only their
  own regions instead of repeatedly clearing the full display. Suppress
  unchanged device/scan/header paints and prevent long titles, list labels,
  suffixes, and input values from drawing over adjacent UI elements.
- Persist AI chat messages, provider/model metadata, HTTP outcomes, response
  parsing diagnostics, and speech/transcription errors as rotating JSON-lines
  logs on the SD card. API keys and authorization headers are never logged.
- Read provider response text with an explicit ArduinoJson string conversion;
  the previous null fallback inferred `nullptr_t` and discarded valid text
  from both OpenAI and Claude responses.

## 0.3.15.9 — Diagnose and mitigate empty AI responses

### Fixed

- After 0.3.15.8, both providers progressed past transport/JSON-parsing
  and started returning valid, parseable 2xx responses with no text in
  them ("No text in API response") — another previously undiagnosable
  blind spot. `AiService::send()` now logs the actual `finish_reason`
  (OpenAI) / `stop_reason` (Claude) plus up to 500 bytes of the raw
  response to serial, and surfaces the reason on-device
  (`"No text (<reason>)"`), instead of a single generic message.
- Raised both providers' output-token budget from 700 to 2000. The most
  likely real cause, matching both providers failing identically: this
  app's default OpenAI model (`gpt-5-mini`) is very likely a
  reasoning-tier model, which spends part of its completion-token
  budget on hidden reasoning tokens *before* producing any visible
  output — a well-documented behavior of that model class. A 700-token
  budget can be entirely consumed by hidden reasoning, leaving zero
  tokens for the actual answer and producing an empty `content` field
  with `finish_reason: "length"`, which look identical to a genuine
  empty response from the caller's side without the new diagnostic.
- Not yet confirmed on hardware: whether the higher budget alone
  resolves it, or whether the new `finish_reason`/`stop_reason` logging
  reveals something else entirely (e.g. a genuinely different response
  shape for `claude-sonnet-5` specifically). Check serial output if this
  still fails.

## 0.3.15.8 — Wrong default Claude model, and a blind spot on bad JSON

### Fixed

- The built-in Claude model default/fallback (`anthropic_model`,
  `AiService::anthropicModel_`) was `claude-sonnet-4-6`, which doesn't
  match any real Anthropic model naming convention — every request that
  fell back to it would have been rejected by the API. Corrected to
  `claude-sonnet-5` in `include/ai_service.h`, `src/ai_service.cpp` (both
  the config-load fallback and the per-request empty-model guard), and
  `sd-card-files/ghostwire/secrets/ai.example.json`. If your own
  `/ghostwire/secrets/ai.json` on the SD card explicitly set
  `anthropic_model` to the old value, update or blank it out too.
- `AiService::send()` had a real blind spot: when the HTTP response was
  a valid 2xx but the accumulated body still failed `deserializeJson()`,
  the code discarded ArduinoJson's own failure reason and the actual
  bytes received, showing only a generic "Invalid API response" with no
  serial diagnostics at all — exactly the "got a 200 but can't parse
  JSON" symptom reported after testing 0.3.15.7. Now logs
  ArduinoJson's `DeserializationError` reason (e.g. `IncompleteInput`,
  `InvalidInput`, `NoMemory`) plus up to 400 bytes of the actual response
  to serial, and surfaces the reason in the on-device status line too
  (`"Bad JSON: <reason>"`), so a repeat failure is now diagnosable
  instead of opaque.
- Not yet independently verified: the `gpt-5-mini` OpenAI default. No
  evidence found that it's wrong, but it wasn't checked against a live
  key either — worth confirming if OpenAI requests fail with a similar
  "model" error to what Claude was hitting.

## 0.3.15.7 — Reliable AI response start and chat scrolling

### Fixed

- Wait for response bytes and explicitly seek the `HTTP/` status line before
  parsing headers, preventing a valid delayed `200 OK` response from being
  rendered as error text.
- Add Up/Down scrolling across all wrapped lines in the bounded conversation;
  new replies return the view to the latest content.

## 0.3.15.6 — AI response timeout units

### Fixed

- Correct the direct TLS stream timeout from 45 milliseconds to 45 seconds.
  OpenAI's valid `HTTP/1.1 200 OK` status line is now read as the response
  status instead of being mistaken for an error-body prefix.

## 0.3.15.5 — Direct TLS JSON transport

### Fixed

- Validate every chat body locally before transmission, then send one HTTP/1.1
  request directly over certificate-verified TLS with an exact content length.
- Decode fixed-length and chunked provider responses without the Arduino
  HTTPClient request-framing layer that produced malformed JSON at OpenAI.
- Trim removable-media API keys before constructing authentication headers.

## 0.3.15.4 — Simplified OpenAI chat transport

### Fixed

- Move the constrained-device OpenAI text client from Responses to the
  supported Chat Completions endpoint, using its simpler model/messages and
  choices/message response shape.
- Keep the same bounded local history and selected model while removing the
  Responses-specific input/output conversion implicated by repeated server
  reports that the transmitted request lacked its required model.

## 0.3.15.3 — Explicit AI request body transmission

### Fixed

- Send JSON requests through HTTPClient's explicit byte-buffer-and-length
  overload instead of its ambiguous Arduino String convenience overload.
  This guarantees the serialized model and input fields are transmitted and
  prevents the API receiving an effectively empty JSON request.
- Add key-free serial request diagnostics containing only endpoint, selected
  model, and payload byte count.

## 0.3.15.2 — Required AI model fallback

### Fixed

- Treat missing, blank, or whitespace-only provider model settings as absent
  and restore the built-in OpenAI or Claude default before creating a request.
- Accept the common generic `model` field as an OpenAI fallback while keeping
  the documented provider-specific fields authoritative.
- Recheck the model immediately before every request so malformed removable
  media configuration cannot omit this API-required parameter.

## 0.3.15.1 — AI request compatibility and diagnostics

### Fixed

- Send manually retained OpenAI conversations through the Responses API's
  simplest documented string-input form, avoiding optional structured-input
  validation differences on constrained direct HTTP clients.
- Extract both OpenAI and provider-generic error messages before falling back
  to the HTTP status, and print the bounded response body to serial for future
  hardware diagnosis without ever logging the API key.

## 0.3.15 — AI chat and cloud speech

### Added

- Add a top-level **AI Chat** client with OpenAI Responses API and Claude
  Messages API support, provider switching, bounded conversational context,
  editable prompts, and clear on-device status/error reporting.
- Load provider keys and optional model names exclusively from
  `/ghostwire/secrets/ai.json` on microSD; keys are never displayed or logged.
- Add six-second push-to-talk recording and OpenAI transcription, staged
  through microSD to avoid buffering audio in ESP32 RAM.
- Add OpenAI text-to-speech for the latest assistant reply and play the
  downloaded MP3 through Ghostwire's existing asynchronous audio service.
- Validate both API hosts with the bundled Google Trust Services Root R4 CA
  instead of disabling TLS certificate verification.

### Limits

- Conversations are deliberately capped at eight recent turns and roughly
  6,000 characters to protect RAM and control request cost.
- Voice is turn-based rather than realtime. Speech features require an OpenAI
  key even when Claude is selected as the text provider.

## 0.3.14.5 — Consolidated external devices

### Changed

- Move Chameleon Ultra into the top-level **Devices** menu beside Biscuit Pro.
- Remove the now-empty top-level RFID category, while retaining all existing
  Chameleon connection, tag scanning, continuous scan, and logging behavior.

## 0.3.14.4 — Unique Biscuit wardrive counts

### Fixed

- Count unique Wi-Fi BSSIDs and BLE MAC addresses in the Biscuit wardrive
  monitor instead of incrementing for every repeated observation.
- Reassemble the identifying portion of records split across BLE
  notifications, normalize addresses before comparison, reject malformed
  identities, and cap each in-memory identity set at 512 entries.

## 0.3.14.3 — Simplified Biscuit wardrive counters

### Changed

- Replace the cluttered raw Biscuit wardrive notification feed with a compact
  dashboard containing only AP and BLE observation counts.
- Preserve protocol-prefix fragments across BLE notification boundaries so
  split `DATA:AP:` and `DATA:BT:` messages are still counted correctly.

## 0.3.14.2 — Devices menu and Biscuit wardrive monitor

### Added

- Add a top-level **Devices** category for dedicated control panels, moving
  Biscuit Pro out of the general BLE utility list and leaving room for future
  headless-device integrations.
- Add a live Biscuit wardrive monitor. Enter explicitly starts
  `CMD:wardrive:`, incoming BLE notifications are shown as a bounded rolling
  feed, and Enter, Back, emergency stop, or leaving the panel sends the normal
  Biscuit stop-scan command.

### Changed

- Simplify the Biscuit connection screen into a compact readiness summary,
  combining model and firmware information and moving operational choices to
  the tools screen.

## 0.3.14.1 — Biscuit Pro control panel

### Added

- Add **BLE → Biscuit Pro**, a dedicated BLE central client for the user's
  headless Biscuit Pro using its advertised service and command, response,
  status, and Device Information characteristics.
- Discover by both device name and service UUID, request a 512-byte ATT MTU,
  subscribe to fragmented response notifications, and show model, firmware,
  C5 firmware, and current device status after connection.
- Add a scrollable result viewer and a deliberately bounded read-only tool
  set: device information, Wi-Fi AP/station discovery, packet count, current
  channel, and node list.

### Safety / limits

- This first release does not expose Biscuit configuration, portal, OTA,
  reboot, jamming, deauthentication, credential, or other active-attack
  commands. Leaving the panel and Ghostwire's emergency stop both disconnect
  the client and release NimBLE.
- The protocol and GATT layout were validated against Biscuit Manager 1.0.19
  and a locally owned Biscuit Pro before implementation.

## 0.3.13.1 — Guarded microSD DuckyScript Runner

### Added

- Add a DuckyScript file picker under **Tools → USB / HID → Run DuckyScript** for `.txt` and `.duck` files stored in `/ghostwire/scripts` on microSD.
- Add a preflight confirmation screen showing the selected filename, command count, unsupported-command count, and declared delay time before any HID input is emitted.
- Support the bounded core dialect `REM`, `STRING`, `STRINGLN`, `DELAY`, `DEFAULT_DELAY`/`DEFAULTDELAY`, `ENTER`, `TAB`, `BACKSPACE`, and `SPACE`.
- Add a three-second cancelable countdown, Escape cancellation during delays, an execution result screen, and unconditional HID key release after completion or cancellation.

### Safety / limits

- Scripts are local-only and require physical file selection plus explicit confirmation on the Cardputer. Files are capped at 64 KiB, execution at 500 non-empty lines, individual delays at 10 seconds, default delays at 5 seconds, and cumulative delays at 60 seconds.
- Unknown commands are reported during preflight and skipped during execution. Chords, shell-specific commands, automatic downloads, and background execution are not part of this first version.

## 0.3.12.2 — Live BLE HID Keyboard

### Added

- Add a bondable **Ghostwire Keyboard** BLE HID peripheral with a standards-based keyboard report descriptor, battery service, HID appearance, and automatic advertising after disconnect.
- Add an explicitly armed live-keyboard screen showing stopped, advertising, and connected states plus a sent-character counter. No keystrokes are transmitted until the user enters this screen, starts advertising, pairs a host, and the host connects.
- Forward printable Cardputer input plus Enter, Backspace, and Tab to the paired host using press-and-release HID reports. Escape remains a local emergency exit and is never forwarded.
- Shut down and release NimBLE when leaving the keyboard screen or invoking Ghostwire's global emergency stop, preserving the established BLE/Wi-Fi radio handoff discipline.

### Safety

- The screen clearly instructs use only with a device the operator controls; leaving it immediately stops advertising and disconnects the host.

## 0.3.12.1 — Rich BLE Advertisement Sniffer

### Added

- Expand BLE Discovery into a passive **Advertisement Sniffer** that records complete advertisement payload length, legacy advertisement type, public/random address type, inferred connectability, advertised-service count, and up to three service UUIDs per observed device.
- Capture manufacturer-specific data as bounded hexadecimal and decode common Bluetooth SIG company identifiers for Apple, Microsoft, Samsung, and Google while retaining the numeric company ID.
- Redesign BLE device details to expose address/RSSI, advertisement behavior, manufacturer, services, and payload metadata on one screen.
- Extend BLE CSV exports with address type, advertisement type, payload size, service count/list, decoded manufacturer, and raw manufacturer-data hex for offline assessment.

### Changed

- Rename the BLE menu's Discovery entry to **Advertisement Sniffer** and mark the passive BLE Sniffer roadmap foundation complete.

## 0.3.11.19 — Channel Analyzer and Network Dashboard

### Added

- Add a visual 2.4 GHz **Channel Analyzer** under Wi-Fi. It graphs AP density across channels 1-13, highlights crowded channels, and recommends the least-congested non-overlapping channel from 1/6/11 using both AP count and received signal strength from the latest scan.
- Add a **Network Dashboard** showing the active SSID, RSSI, station IP, gateway, subnet mask, DNS server, and local MAC address in one live, refreshable screen.
- Reuse the established Wi-Fi scan lifecycle and connection checks so both tools follow the same radio safety and navigation behavior as Discovery and Host Scan.

## 0.3.11.18 — Cyberdeck Idle Mode

### Added

- Add an opt-in **Cyberdeck idle** setting, disabled by default, which replaces the normal black screen timeout with an animated Matrix-style rain display.
- Show live battery and radio state in the idle footer, including connected Wi-Fi RSSI or passive Wi-Fi/BLE counters when the sniffer or wardrive service is already active.
- Brighten the leading glyph when passive radio counters change, giving captured nearby activity a visible pulse without starting a scan, disrupting an existing connection, or transmitting packets.
- Wake instantly on any key, restore the configured brightness, and redraw the exact screen that was active before idle mode began. The wake key is consumed so it cannot accidentally trigger an action.

## 0.3.11.17 — Optional Wi-Fi auto-connect

### Added

- Add a persistent **Auto-connect Wi-Fi** Settings toggle, disabled by default.
- When enabled and a saved network is available, begin connecting to it in the background after boot without replacing the main menu with the connection-status screen.
- Automatically disable auto-connect and remove its usable credentials when **Save Wi-Fi login** is turned off or settings are restored to defaults.

## 0.3.11.16 — Interactive SSH line editing

### Fixed

- Apply Backspace to the locally echoed SSH input immediately while still forwarding DEL to the remote PTY.
- Give the shared terminal parser real BS/DEL behavior instead of rendering control bytes as dots.
- Advance the local terminal line immediately on Enter and suppress the matching remote CRLF echo.
- Abandon stale echo suppression as soon as remote output differs from the expected echo, preventing unrelated later text from being swallowed.
- Redraw the SSH text area immediately for Backspace and Enter as well as printable input.

## 0.3.11.15 — Immediate SSH echo and stable terminal redraw

### Fixed

- Echo printable SSH keyboard input locally and suppress the matching remote echo, following Bruce's responsive terminal approach instead of making the user wait for a network round trip before typed characters appear.
- Treat carriage return as a cursor movement rather than clearing the pending line. The old parser erased prompts and typed text whenever normal CRLF terminal output arrived, causing the SSH text area to appear blank.
- Remove a worker handoff use-after-lifetime race: after publishing handshake completion, the persistent SSH worker no longer dereferences the stack-backed connection work record.

## 0.3.11.14 — Persistent SSH I/O worker

### Fixed

- Keep libssh session creation and all subsequent channel polling, reads, writes, and cleanup on the same dedicated worker task. This follows Bruce's current Cardputer SSH design and avoids handing libssh objects across FreeRTOS tasks after authentication.
- Add bounded TX/RX queues between the keyboard/UI loop and SSH worker. The worker drains keyboard input, polls stdout and stderr, and forwards received bytes every 10 ms.
- Handle partial channel writes by retaining unsent bytes instead of treating transient backpressure as a disconnected session.

## 0.3.11.13 — Responsive SSH keyboard input

### Fixed

- Remove the blocking 100 ms flush that ran after every SSH channel write. This compounded into severe per-character latency and allowed only roughly two typed characters to reach the session during normal keyboard input.
- Batch Enter, Backspace, Tab, and all printable characters from one Cardputer keyboard event into a single bounded channel write instead of issuing one libssh call per character.

## 0.3.11.12 — Isolated SSH handshake stack

### Fixed / diagnostics

- Run the bounded libssh connect, key exchange, host-key check, authentication, and channel creation in a one-shot task with a 24 KB stack instead of the Arduino loop task's much smaller stack. Interactive channel I/O remains non-blocking on the normal loop after the handshake completes.
- Persist each call-level libssh checkpoint during the handshake. If the larger stack does not resolve the panic, System Diagnostics will report the actual last call rather than falling back to the coarse `libssh connect` marker.
- Verified the reported target is reachable and advertises a normal OpenSSH 10.2 algorithm set before changing the embedded execution model.

## 0.3.11.11 — Stable SSH trust confirmation lifecycle

### Fixed

- Move first-use trust confirmation before opening the SSH transport. LibSSH-ESP32 was consistently panicking after returning to the UI with a negotiated but unauthenticated session retained at the `trust prompt` stage.
- After confirmation, connect, obtain the host key, authenticate, and persist the SHA-256 fingerprint in one bounded call. Later connections continue to compare the observed key with that stored fingerprint and refuse a changed key.
- Preserve the existing password buffer and recent-target history across the pre-connection trust confirmation, avoiding another re-entry cycle.

## 0.3.11.10 — SSH host-key prompt hardening

### Fixed

- Validate persisted SSH SHA-256 fingerprints before use and discard incomplete values that may have survived an earlier reset during storage.
- Replace the dynamically concatenated unknown-key footer with a bounded fixed prompt while retaining the complete fingerprint internally for trust-on-first-use storage.
- Split the post-hash diagnostic stage into host-key comparison and trust-prompt return checkpoints, localizing the remaining panic beyond the previously truncated `Host Key Che...` report.

## 0.3.11.9 — SSH history and call-level panic diagnostics

### Added

- Persist the three most recently submitted SSH targets. Opening SSH pre-fills the newest entry and Tab cycles through older entries, so a crashing or disconnected session does not require retyping `user@host[:port]`.
- Add RTC-retained, call-level SSH crash breadcrumbs covering session allocation, option setup, `ssh_connect`, host-key processing, authentication, and channel setup. These survive a panic without repeated flash writes and appear through the existing System Diagnostics row.

## 0.3.11.8 — Wi-Fi reconnect and radio-state fix

### Fixed

- Saved-network reconnects no longer overwrite the persisted password with the empty password-entry buffer after a successful connection. The password actually used by the current attempt is now tracked separately, saved on success, and wiped afterward.
- All Wi-Fi connection paths now use one radio preparation sequence: stop promiscuous capture, discard scan state, enter station mode, disable power saving during association, disconnect stale station state without erasing credentials, and then begin the new connection. This prevents a previous discovery/sniffer session from leaving the radio unable to associate until timeout.

## 0.3.11.7 — SSH panic localization

### Diagnostics

- Split the persistent SSH connection breadcrumb around `libssh_begin()`. System Diagnostics can now distinguish a panic in the library constructor (`libssh init`) from one in session creation or negotiation (`libssh connect`).
- Confirmed from the installed LibSSH-ESP32 source that repeated initialization is internally reference-counted; the next hardware result will determine whether the failure is initialization itself or the library's subsequent ESP32-S3 cryptographic/network path.

## 0.3.11.6 — SSH target-entry reboot diagnostics

### Changed

- Defer LibSSH-ESP32 initialization until the user actually submits an SSH password. The SSH library is no longer initialized globally during boot, isolating ordinary target entry and the rest of the firmware from its constructor.
- Persist a compact SSH transition breadcrumb before target parsing, password-screen drawing, and connection setup. If the device resets, System Diagnostics now reports the last reached SSH stage so a reset that loses its USB serial output can still be localized.
- Clear the breadcrumb after a successful connection or when the SSH flow is cancelled.

## 0.3.11.5 — SSH host-trust reconnect stability

### Fixed

- Initialize LibSSH-ESP32 explicitly before creating the first SSH session.
- Keep the negotiated SSH transport alive while the user reviews an unknown host key, then authenticate over that same session after confirmation. This avoids the teardown/reconnect sequence that could reboot the device on the second connection attempt.
- Cancel and clean up a pending SSH host-trust session when leaving the password screen.

## 0.3.11.4 — Security hardening, real bug fixes, and project infrastructure

A separate analysis pass reviewed the codebase and made a substantial,
reviewed batch of changes across security, correctness, and process —
verified here (native tests re-run, hardcoded checksum re-derived
against the actual patched archive, a plain `pio run` re-confirmed) before
folding into the changelog and flashing.

**Security hardening:**
- SSH gains real trust-on-first-use host-key pinning: the server's
  SHA-256 host-key fingerprint is stored per `host:port` on first
  connect (after an explicit second confirmation, not silent trust),
  and a later mismatch is refused outright ("HOST KEY CHANGED —
  connection refused") instead of the previous 0.3.11.x behaviour of
  accepting whatever key the server presented every time.
- Wi-Fi credential saving is now opt-in (new Settings row, default
  off) instead of always-on; both the Wi-Fi and SSH password buffers
  are explicitly zeroed in memory after use, not just cleared.
- New global emergency stop (`Ctrl+Alt+Backspace`) — stops every radio/
  socket/logger and returns to the main menu. Checked at the very top
  of `handleInput()`, before the Telnet/SSH/password screens' all-keys-
  forwarded handling, so it can't be swallowed as a literal keystroke
  mid-session.
- `patch_wifi_lib.py` (the deauth-enabling symbol-weakening script) now
  SHA-256-verifies the exact `libnet80211.a` before patching it and
  refuses to run against anything outside this project's own
  `.pio-core` sandbox, rather than patching whatever archive it finds.

**Real bug fixes:**
- `WifiSnifferService` replaced its hand-rolled ring buffers with
  FreeRTOS static queues and added a critical section around the
  handshake-target BSSID — the old plain-`volatile` version had a
  genuine race between the promiscuous-callback (ISR context) reading
  the 6-byte BSSID mid-`memcpy()` from the main thread.
- `NetworkHostScanService::beginNextPing()` recursed on every failed
  `esp_ping_new_session()` call — a real stack-overflow risk under
  sustained resource exhaustion. Now iterative.
- `SdLogger`/`PcapLogger` now check `getWriteError()` on their periodic
  flush and mark themselves inactive with an error count on failure,
  instead of silently continuing to claim "recording" through a full
  or failing SD card.

**Project infrastructure:**
- First test coverage this project has had: native Unity tests for
  `EapolParser` (`test/test_eapol_parser/`), running against a new
  `[env:native]` PlatformIO environment. `Screen` and
  `appendTerminalByte()` extracted into their own headers
  (`app_screen.h`, `terminal_buffer.h`) so the tested code doesn't drag
  in all of `main.cpp`.
- `-Wall -Wextra -Wformat=2` on the firmware build, `-Werror` on
  native; the scattered `%lu`/cast fixes through `main.cpp` are exactly
  what the stricter format checking needed, not unrelated churn.
- War Drive's Wi-Fi CSV export switched to genuine WiGLE CSV 1.6
  format (correct 11-column header/field order) — directly importable
  into wardriving tools instead of needing manual reformatting.
- `LICENSE` (MIT), `CONTRIBUTING.md`, `SECURITY.md`, `docs/` (build/
  release, hardware support, authorized-use), and CI/release GitHub
  Actions workflows — all reference this project's actual conventions
  (the non-blocking-scan lesson, the transmit-confirmation-screen
  rule, dependency-pinning discipline) rather than generic boilerplate.
- `platformio.ini` now keeps packages under `.pio-core` via config
  (`platforms_dir`/`packages_dir`/`cache_dir`) instead of requiring the
  `PLATFORMIO_CORE_DIR` environment variable on every command, and
  drops the hardcoded `/dev/ttyACM0` upload/monitor port in favour of
  PlatformIO's own auto-detection (more portable for other
  contributors' OS/port setups).

## 0.3.11.3 — Fix SSH device-wide sluggishness: drop the background task

User reported the whole device became sluggish while an SSH session was
active (a different, worse symptom than 0.3.11.2's slow-typing fix).
Asked to check how other ESP32 pentest firmware handle SSH before
redesigning: Bruce (`pr3y/Bruce`, AGPL, read for facts only) doesn't
have an SSH client at all in its current source. Evil-Cardputer
(`7h30th3r0n3/Evil-M5Project`, MIT) does, using the same `LibSSH-ESP32`
library, but runs the whole session **fully synchronously with no
FreeRTOS task and no mutex**.

That was the real clue: this app's Telnet client (plain `WiFiClient`
polled directly in `loop()`, no background task) never showed this
symptom. The one thing unique to `SshService` was the background task +
`SemaphoreHandle_t` mutex added in 0.3.11.1 specifically to avoid
blocking the UI during the multi-second connect/auth handshake — that
task/mutex overhead, not SSH/crypto itself, was almost certainly the
actual cause.

**Removed the task and mutex entirely.** `SshService` is now a thin
synchronous wrapper shaped like `WiFiClient` itself: `connect()` is a
single bounded blocking call (10s timeout, same convention as Telnet's
own bounded connect and this app's Wi-Fi/BLE scans) that does the full
handshake up through requesting the shell, then every subsequent
`read()`/`write()` is a direct, lock-free passthrough to
`ssh_channel_read_nonblocking()`/`ssh_channel_write()` — called only
from the main thread, exactly like Telnet's own polling. The
`ssh_blocking_flush()` fix from 0.3.11.2 stays (that was a genuinely
separate, real bug). `connectSsh()` now shows "Connecting..." before
the blocking call and branches on success/failure afterward, matching
`connectTelnet()`'s shape line for line; `drawSshSession()` and the
`loop()` poll block lost their `SshService::State`
(`Connecting`/`Failed`/`Disconnected`) machinery and are now
near-verbatim copies of the Telnet client's equivalents.

## 0.3.11.2 — Fix slow/garbled Telnet and SSH sessions

User feedback after testing 0.3.11.1: SSH felt "slow and cluttered."
Three real, distinct issues, all fixed:

- **Slow typing/output (SSH only)**: confirmed via libssh's own mailing
  list that `ssh_channel_write()` on a non-blocking channel only queues
  data — it doesn't actually reach the socket until something else
  happens to flush it (matches the reported symptom exactly: input
  seemed to lag until another key was pressed). Fixed by calling
  `ssh_blocking_flush(session, 100)` right after every write in
  `SshService::task()`.
- **Garbled/cluttered output (both clients)**: the byte-sanitizing logic
  treated `\r` as just another char to drop, but interactive shells
  constantly use `\r` to return to the start of the current line for
  redraws (readline echo, prompt updates) — dropping it meant every
  redraw concatenated onto the previous one instead of overwriting it,
  producing exactly the "cluttered" garbage the user saw. Also, this
  client renders every non-printable byte as `.`, including the ANSI
  colour/cursor escape sequences most shell prompts send, which shows
  up as literal dot-noise. Fixed both in one shared helper,
  `appendTerminalByte()` (`main.cpp`, used by both the Telnet and SSH
  clients, replacing what had been near-identical logic duplicated in
  each): `\r` now resets the in-progress line instead of being ignored,
  and ANSI CSI (`ESC [ ... letter`) / OSC (`ESC ] ... BEL`) escape
  sequences are recognized and swallowed whole rather than rendered as
  dots. **Still not a terminal emulator** — no cursor addressing, no
  colour, no scrollback reflow — just enough that escape codes and
  line-redraws don't visually wreck this small screen, which is what
  was actually happening.

## 0.3.11.1 — SSH client

Third of the four remaining network tools (Telnet → SSH → Responder →
ARP spoof), reached via a new `Network > SSH Client` item.

**Feasibility spike done before any design work.** `ewpa/LibSSH-ESP32`
(the library named in this project's own roadmap notes) has an open,
unresolved upstream GitHub issue describing a PlatformIO-specific
compile failure (`config.h` include-order conflict with the
Arduino-ESP32 core's own `config.h`). Rather than plan around an
assumption, added the dependency to `platformio.ini` and test-built a
throwaway file against this project's exact toolchain (arduino-esp32
2.0.16, espressif32@6.7.0) before writing any real feature code — it
compiled and linked clean, so the known issue doesn't reproduce with
the current library release. Full feature code adds ~6.6% flash
(44.6%→51.2%) and ~3% RAM (25.8%→28.2%) — comfortable headroom left.

**Architecture note**: every libssh call from `ssh_connect()` through
channel/PTY/shell setup is blocking (confirmed from the library's own
`exec` example and upstream docs), so unlike the Telnet client (plain
non-blocking `WiFiClient` polling in `loop()`), this needed a real
background FreeRTOS task — new `SshService`
(`include/ssh_service.h`/`src/ssh_service.cpp`). This is the first
genuine two-task-concurrency service in this codebase (the only
existing task, `AudioService`'s playback worker, is one-directional and
coordinated with plain `volatile bool` flags); `SshService` uses an
actual `SemaphoreHandle_t` mutex to guard the shared connection state
and the two byte queues (outgoing keystrokes, incoming shell output),
since two real tasks touch them concurrently. The task itself does the
whole blocking connect/auth/channel/PTY/shell sequence, then switches
the channel non-blocking (`ssh_channel_set_blocking(channel, 0)` +
`ssh_channel_read_nonblocking()`, confirmed to exist in libssh's API)
and polls it in its own tight loop.

Screens reuse the Telnet client's conventions rather than inventing new
ones: free-text `user@host[:port]` entry, a masked password screen
(identical mechanic to Wi-Fi Connect's password screen), then a live
shell screen with the same all-keys-forwarded design as Telnet's
session screen (`Esc` disconnects, `Tab` sends a literal tab, Backspace
sends `0x7F`) and the same capped 64-line scrolling buffer/rendering
split. The one new wrinkle over Telnet: connecting can take several
seconds for key exchange, so the session screen shows a real
"Connecting..." state instead of Telnet's near-instant bounded connect.

**Deliberate scope limits, stated plainly rather than silently
accepted**:
- **No host key verification/pinning** — accepts whatever key the
  server presents (`SSH_OPTIONS_STRICTHOSTKEYCHECK` disabled), no
  `known_hosts` storage or change detection, no MITM protection. Aimed
  at the user's own lab equipment, not hardened for hostile networks.
- **Password auth only** — no public-key auth (would need on-device
  private key generation/storage/management).
- **No real terminal emulation**, same as Telnet — no ANSI/VT100
  interpretation. The requested PTY is sized to what this screen can
  actually display (39×6) rather than a dishonest 80×24 default, but
  full-screen curses-style remote programs will still render as
  garbage.

## 0.3.10.1 — Telnet client

First of the four remaining network tools (Telnet → SSH → Responder →
ARP spoof). Two new items reach it: `Network > Telnet Client` (manual
host entry) and a new `Telnet` action in Host Discovery's action menu
(pre-fills the host from the selected scan result).

Unlike Host Discovery/Port Scan, this only ever manages one connection
at a time, so it reuses the existing "quick port scan" precedent — a
single bounded blocking `WiFiClient::connect(host, port, 5000ms)`
(host can be a hostname or an IP; lwIP's DNS resolution is already
wired into `WiFiClient` with no extra code needed) followed by ordinary
non-blocking `available()`/`read()`/`write()` polling in `loop()` —
rather than a new async service class.

The live session screen (`Screen::TelnetSession`) is the most invasive
new input path in the app so far: every key needs to reach the remote
host, so it bypasses the action menu, navigation shortcuts, and even
`Tab` (forwarded as a literal tab byte for remote shell completion,
rather than opening the action menu like every other screen) — `Esc` is
the only way out. Backspace sends `0x7F` (DEL); a specific target
wanting `0x08` (BS) instead would be a one-line follow-up. Incoming
bytes are sanitized the same way `loadTextPreview()` already does
(non-printable → `.`) into a capped, scrolling 64-line buffer, with the
still-open (no trailing newline yet) line also shown so prompts like
"login: " are visible immediately.

**Deliberately not a real terminal emulator**: no ANSI/VT100 escape
sequences, no telnet protocol option negotiation (IAC/DO/WILL). Plain
line-based text in and out; full-screen curses-style remote apps will
render as garbage. Tested against a plain `nc -l <port>` listener
rather than a real telnet server (none available) — a minimal client
doing no protocol negotiation is exactly what that tests.

## 0.3.9.2 — Split Frog and Duck into two separate themes

User's correction after testing 0.3.9.1: "I meant two different themes
for Frog and Duck, not a joint one." Split the single combined theme
into **Frog** (green palette, `0x0081/0x1162/0x8F27/0xD797/0x5C2A/
0xFB45`, ribbit-only boot sound) and **Duck** (blue-teal palette,
`0x0062/0x1125/0xFE85/0xE718/0x5BF0/0xFB45`, quack-only boot sound) —
7 themes total unchanged in count, just the last entry replaced with
two. Each uses `bootSoundPrimary` only (`bootSoundSecondary` stays
`nullptr`) rather than the two-clip sequence the joint theme needed, so
`playBootSound()`'s existing fallback-to-tone-chime logic needed no
changes at all — the two-optional-fields-per-theme design from 0.3.9.1
already covered this shape.

## 0.3.9.1 — Three more themes + Frog and Duck boot sound

Three more built-in themes on top of the original four: **Space**
(near-black navy, electric blue accent), **Frog and Duck** (pond green,
duck-yellow accent), and **Pastel** — deliberately built as a *dark*
background with soft pastel accents (lavender/pink/peach) rather than a
literal light/white theme, after an actual light-background attempt
came out with genuinely bad contrast math (selected list rows would
have been near-illegible — this UI's selected-row highlight needs
`background` dark enough to read as reversed text on `accent`, which
fights against a light theme needing a pale background). Same idea as
popular pastel *dark* themes (Catppuccin, Dracula): soft, muted hues
without sacrificing legibility. All three checked with the same
luminance-contrast math as the original four before flashing.

**Frog and Duck also gets a custom boot sound**: a ribbit followed by a
quack, replacing the usual tone chime, from two short MP3s
(`/ghostwire/audio/Boot/ribbit.mp3` and `.../quack.mp3` — copy the
`Boot/` folder from this repo's `sd-card-files/ghostwire/audio/` onto
the real SD card for the sound to play; falls back to the normal tone
chime if the SD card or those files aren't there). This is a property
of the *theme* now, not a hardcoded "if Frog and Duck" check —
`Branding::Theme` gained two optional MP3-path fields
(`bootSoundPrimary`/`bootSoundSecondary`, both `nullptr` for every other
theme), so any future theme can opt into its own boot sound the same
way. New `playBootSound()` tries the active theme's clips first (via
the existing `AudioService::startMp3()`/`isPlaying()`, same as the MP3
browser/Now Playing screens already use) and falls through to the
original `playBootChimeTones()` on any failure — replaces both existing
call sites of the tone chime (the immediate-success path in
`showBootSummary()` and the deferred-retry path in `loop()` for when the
speaker codec wasn't I2C-ready yet on a cold boot).

**Known trade-off**: each clip gets a bounded 3000ms wait (measured
lengths are ~1.1s/~1.4s), so worst case is ~6s of blocking if something
goes wrong, versus the tone chime's ~1.15s. In the common case this
happens during `setup()`, before `loop()` starts, so it just extends the
boot sequence's own budget; it only risks blocking an already-running
app in the rare deferred-retry path, same category of risk the existing
chime retry already accepted, just a larger bound. Not worth building a
full async sequencer for an opt-in Easter egg.

## 0.3.8.2 — List position indicators

Found while testing 0.3.8.1: adding the Theme row gave Settings 7 items,
but `drawSettings()` drew all 7 unconditionally instead of scrolling, so
"Restore defaults" got clipped behind the footer bar with zero
indication there even was a 7th item — scrolling with no feedback that
you're scrolling. Fixed for Settings, then generalized: added a small
"current/total" readout (new `drawHeaderPosition()`, header top-right,
same idea `drawTextPreview()` already used for its own line counter) to
every screen with a genuinely variable-length or already-over-6-item
list — Main Menu, Wi-Fi Discovery/Connect, BLE Discovery, Network Host
Discovery (idle states only, live scan progress already covers it
mid-scan), MP3 Files, file browser, Log Sessions, System Diagnostics
(22 items — same invisible-scrolling bug as Settings had, just never
reported), Tools, and Settings itself.

Deliberately skipped: Network Port Scan (its header title is dynamic —
`"Port Scan " + ip` — and would collide with the counter on longer IPs),
fixed submenus that can never exceed 6 items (Wi-Fi/BLE/RFID/GPS/Mesh
menus, BLE Spam mode picker, USB/HID presets), and Text Preview (already
has its own equivalent counter, left alone).

## 0.3.8.1 — Selectable UI themes (Matrix, Cyberpunk, Windows, Amber Terminal)

User asked for this alongside the action-menu UI overhaul, deferred until
that was done: "maybe themes? Especially if the theme also works on the
boot screen." Confirmed by direct inspection that every screen in this
codebase — including the boot title/console sequence — already draws
exclusively through six shared colours in `include/branding.h`
(`background`/`panel`/`accent`/`text`/`muted`/`warning`), with zero raw
hex colors anywhere else. That made this tractable without touching any
individual screen's drawing code: the six colours moved from
`constexpr` to runtime-mutable globals (new `src/branding.cpp`), backed
by a `Branding::Theme` table and `Branding::applyTheme(index)`.

Four built-in themes: **Matrix** (index 0, today's exact original look,
unchanged default), **Cyberpunk** (near-black/purple, neon magenta
accent, cyan text), **Windows** (teal desktop, navy header/footer, white
accent — a recognizable 95/98-era palette, not a literal widget-chrome
recreation since this is a 6-color palette, not a skin system), and
**Amber Terminal** (black/amber CRT monochrome, bonus "fun one").

Wired into Settings as a new "Theme" row, reusing the exact same
cycle-through-fixed-options `-`/`=` pattern already used for screen
timeout — no new screen needed. Changing it live-repaints the whole UI
instantly (every draw call reads the shared colours fresh, so there's no
extra work to make the preview live). Persisted via `Preferences`
alongside the other settings, and loaded/applied in `setup()` **before**
the boot sequence runs, so a saved theme carries through to the boot
title/console screen on a real cold boot, not just in-app navigation.
"Restore defaults" resets it to Matrix along with everything else.

## 0.3.7.2 — Action menu: live-screen fix + full letter-shortcut sweep

Two follow-ups after hardware testing of 0.3.7.1:

- **Fix**: on screens that redraw themselves on a timer (Chameleon
  continuous scan, Network Host/Port Scan progress, War Drive counters,
  Wi-Fi Connect status, GNSS/LoRa/Wi-Fi Sniffer/IMU live readouts, the
  system clock), the periodic `draw*()` call — tied only to
  `currentScreen` and a timer, with no idea the action menu overlay
  existed — wiped the whole screen including the menu drawn on top of
  it, every ~100-1000ms. Every one of those periodic redraws in `loop()`
  now also checks `!actionMenuOpen` before firing; the underlying
  service/data keeps updating in the background regardless (only the
  redraw pauses), and closing the menu (Backspace/Esc/Tab) already
  forces one full `drawCurrentScreen()`, so the screen catches straight
  back up the instant the menu closes.
- **Full sweep**: migrated the remaining screens that still had one-off
  letters — GNSS (`L` log toggle), LoRa (`L` log toggle, `P` profile
  switch), Wi-Fi Sniffer (`L` log toggle), IMU (`L` log toggle, `C`
  calibrate), and Log Session Details (`D` delete) — into the same
  `Tab` action menu, on the same "reuse the existing case body via a
  synthetic keypress" mechanism as every screen in 0.3.7.1. Their
  footers now read `Tab: actions` instead of listing the letters.

**Deliberately left as direct keys, not migrated**: Text Preview's `A`/`D`
column-pan controls. These are continuous, repeated-tap navigation in the
same family as `W`/`S` line-scroll (already a direct key, not menu-gated)
rather than a one-off action — putting them behind a menu would mean
reopening it on every keypress to nudge the view sideways, which is worse
than the letters it would replace. Every screen with a genuine
one-off/toggle action now uses the action menu; only navigation-style
repeated keys remain direct, matching the original design's own reasoning
for keeping `R`/refresh and Backspace/Q/Esc as direct keys.

## 0.3.7.1 — Contextual action menu (replaces one-off letter shortcuts)

User's own words: "pressing specific letters which are slowly increasing
in number is making the current UI crowded and sometimes confusing." Nine
distinct one-off letters (`e`, `d`, `l`, `c`, `f`, `p`, `g`, `h`, `a`) had
accumulated across roughly 15 screens with per-screen meanings (`d` alone
meant "deauth," "disconnect," or "pan preview right" depending on the
screen) — and four more network tools on the roadmap were about to add
still more.

Replaced with a single consistent trigger: **Tab** opens a small
on-screen menu listing whatever actions are actually available on the
current screen (`ActionMenuItem{key, label}`, built per-screen by
`actionsForScreen()`), navigated with the same arrows/Enter used
everywhere else. Choosing an item synthesizes the equivalent keypress and
re-dispatches through the existing `handleInput()` logic, so every
underlying action (`exportWifiResults()`, `transmitWifiDeauth()`, etc.)
runs completely unchanged — this increment only changes how each action
is reached, not what it does. `R` (refresh/rescan/reconnect) and
Backspace/Q/Esc (back) stay exactly as direct keys, unchanged.

This first pass migrates the screens that had accumulated the most
letters: Wi-Fi Discovery, Wi-Fi Detail, Wi-Fi Handshake Capture, Wi-Fi
Connect (select + status), BLE Discovery, Chameleon Ultra, Network Host
Discovery, Network Port Scan, and System Diagnostics/Clock. Each
migrated screen's footer now reads `Tab: actions` instead of listing
individual letters. A handful of older, less-crowded screens (GNSS/LoRa/
IMU/Wi-Fi Sniffer's log toggle, LoRa's profile toggle, text preview pan,
deauth confirm, BLE Spam) keep their direct letters for now — same
mechanism, deferred to a follow-on pass once this is confirmed to feel
right in daily use.

## 0.3.6.3 — Full port scan (1-65535)

Second scan mode on Network → Host Discovery's port scan screen: `F` on
a found host now runs a genuine full 1-65535 scan alongside the existing
13-port quick scan (`Enter`) — kept both rather than replacing, since the
quick scan stays useful for a fast check while the full scan is
thorough but slow.

A full sequential scan wasn't viable (up to ~5.5 hours worst case at a
300ms timeout/port), so this needed real concurrency, not just another
blocking loop. New `NetworkPortScanService`
(`include/network_port_scan_service.h` /
`src/network_port_scan_service.cpp`) runs up to **8 concurrent
non-blocking connect attempts** at a time using raw `lwip/sockets.h`
calls (`socket()`/`connect()` with `O_NONBLOCK`, `select()` across all
in-flight fds, `getsockopt(SO_ERROR)` to read the result) — this
project's own bundled `sdkconfig.h` caps the whole system at
`CONFIG_LWIP_MAX_SOCKETS = 16`, so 8 concurrent leaves headroom rather
than maxing it out. Worst case (a target that silently drops packets for
every closed port) is around 8-10 minutes; a typical responsive LAN host
that sends fast RSTs for closed ports finishes far sooner. Same
non-blocking-poll/instant-cancel discipline as every other scan service
this session — `stop()` closes every in-flight socket immediately, so
Backspace/Q works mid-scan same as always.

Results stream in live during the scan (drawn every ~500ms), not just at
the end, and both scan modes share the same results list/CSV export.

## 0.3.6.2 — TCP port scan

Second Network item: press Enter on a host found by Host Discovery to
scan a curated list of 13 common ports against it (21/22/23/25/53/80/
110/139/143/443/445/3389/8080) via `WiFiClient::connect(ip, port,
300)`. Shows only the open ports found, matching the same "list what was
found, not the full attempt log" convention as every other scan screen.

**Deliberately not a new async service class, unlike Host Discovery or
War Drive** — a single host times 13 ports at a 300ms timeout each totals
at most a few seconds, the same order of magnitude as
`scanWifiNetworks()`'s own multi-second blocking channel scan, which this
app already treats as acceptable with a "Please wait" screen. Right-sizing
the engineering to the actual duration rather than reflexively repeating
the last two increments' non-blocking pattern where it isn't needed.

CSV export (`E`) follows the same convention as every other scan screen —
`network_ports`, `timestamp_utc,ip_address,port`.

This is the first of five network tools being built in sequence (port
scan → Telnet client → SSH client → Responder → ARP spoof, the user's own
chosen order — ARP spoof deliberately last since it's the one most likely
to hit a real ESP32 Wi-Fi/lwIP driver limitation, closest in shape to the
deauth block earlier in this project).

## 0.3.6.1 — Network host discovery (new "Network" menu category)

New top-level **Network** category (`Wi-Fi / BLE / RFID / GPS / Mesh /
War Drive / Network / Tools / Settings`, 9 items now), first of the
network-scanning tools Wi-Fi Connect was built to unlock. One item so
far, **Host Discovery** — expected to gain a **Port Scan** item next,
same growth pattern BLE/RFID went through.

Host Discovery sweeps the connected subnet with real ICMP pings (one
`count=1` echo request per candidate address, 300ms timeout — LAN round
trips are fast) and lists which addresses actually replied. Requires an
active Wi-Fi Connect session; range comes from `WiFi.localIP()` +
`WiFi.subnetMask()` (standard network/broadcast bitwise math), capped at
254 hosts since almost all real networks are `/24` and a much larger
range would take far too long scanned sequentially.

`NetworkHostScanService` (`include/network_host_scan_service.h` /
`src/network_host_scan_service.cpp`) is the first use in this codebase of
ESP-IDF's raw lwIP ping API (`esp_ping_new_session`/`esp_ping_start`/
`esp_ping_get_profile`, `ping/ping_sock.h`) — confirmed present in this
project's own precompiled `liblwip.a` before writing any code against it,
not assumed. It's callback-driven and asynchronous by design
(`esp_ping_start()` returns immediately; `on_ping_success`/`on_ping_end`
fire later from an internal ping task), so the service's `update()` polls
a volatile completion flag from `loop()` rather than blocking — applying
the non-blocking-scan lesson from War Driving proactively this time
instead of hitting the same bug again. A watchdog force-advances past any
single host whose callback never fires, so one unresponsive address can't
hang the whole sweep.

CSV export (`E` key) follows the exact convention already established by
`exportWifiResults()`/`exportBleResults()` — `SdLogger`,
`timestamp_utc,ip_address` columns, `/ghostwire/logs`.

**Confirmed on hardware**: sweep progresses, found hosts cross-checked
against the router's own connected-clients list and matched, Backspace/Q
stops instantly mid-scan, and CSV export works.

## 0.3.5.2 — Wi-Fi connected indicator in the header

Small addition after confirming 0.3.5.1's persistent connection works as
intended: a green dot in the top-right status strip (next to the clock
and battery) whenever `WiFi.status() == WL_CONNECTED`, so the connection
is visible from any screen, not just the Connect status screen itself.
Reuses the existing 1-second `drawHeaderStatus()` refresh already driving
the clock, so no new redraw plumbing was needed.

## 0.3.5.1 — Wi-Fi client connect

New **Connect** item in the Wi-Fi menu (`Discovery / Sniffer / Connect`).
USB-C-to-Ethernet was investigated and ruled out first: the board's
USB-C port has 5.1kΩ pull-downs on both CC lines (confirmed from
`stamp-s3a-schematic.pdf`), which is the standard wiring for a USB-C
*peripheral* port, not a host port — there's no VBUS-out capability to
power a connected adapter either. Arduino-ESP32 also has no USB Host
Ethernet (CDC-ECM/RNDIS) driver. Wi-Fi station mode is the actual path to
giving the Cardputer an IP address, which is the real prerequisite for
the network-scanning tools planned next (live host discovery, port
mapping) — those need a real IP stack, which today only ever existed
transiently inside a Wi-Fi scan's brief window before it disconnects
again.

Pick a network from a scanned list (reuses `scanWifiNetworks()`/
`accessPoints` exactly like Wi-Fi Discovery), type its password (a new
masked free-text entry screen — the first one in this codebase), and
connect. Non-blocking throughout: `WiFi.begin()` kicks off the connection
and returns immediately, and `loop()` polls `WiFi.status()` rather than
waiting for it — the same lesson just learned fixing War Driving's
blocking-scan bug, applied proactively here instead of hitting it again.
Once connected, the screen shows IP/gateway/RSSI. Credentials save to
`Preferences` on first successful connect, offering a one-key reconnect
shortcut afterward.

**Deliberately breaks with every other Wi-Fi screen in this codebase**:
leaving this screen does **not** disconnect — the whole point is a
connection that stays up in the background for future tools to use.
Disconnecting is the explicit `D` key. Known, accepted limitation: every
*other* Wi-Fi feature (Discovery, Sniffer, deauth, War Drive) still resets
Wi-Fi mode for its own use and will silently drop this connection if used
while connected — reconciling that is future work once network-scanning
tools actually need to run alongside a live connection.

Free-text entry required a small but real change to the global input
dispatch: every other screen treats single letters as navigation
shortcuts (`q`/`b`/`r`/`w`/`s`/`k`/`j` etc.), which would have corrupted
anything typed into a password field. The new password screen is now
handled in its own branch at the very top of `handleInput()`, entirely
before those shortcuts are computed, rather than trying to carve out
exceptions to them piecemeal.

## 0.3.4.2 — War Driving (new "War Drive" menu category)

New top-level **War Drive** category (`Wi-Fi / BLE / RFID / GPS / Mesh /
War Drive / Tools / Settings`, 8 items now) — combined, GPS-tagged Wi-Fi
AP and BLE device logging. Unlike every other category, War Drive goes
straight from the main menu into its own screen with no one-item
submenu — it's a single unified mode, not a category expected to grow
more items the way BLE/RFID did.

Since the ESP32-S3 has one radio, Wi-Fi and BLE scanning alternate: one
Wi-Fi AP scan, log every result with the current GPS position to
`wardrive_wifi` (`timestamp_utc,lat,lon,fix,ssid,bssid,channel,rssi_dbm,
security`), then one BLE scan logged the same way to `wardrive_ble`
(`timestamp_utc,lat,lon,fix,name,address,rssi_dbm,connectable,
service_uuid`), repeat. Both CSVs start automatically when War Drive is
switched on (`R`) and stop when switched off or backed out of.
Deliberately **no dedup** in the CSVs — the same AP or device seen again
later, from a different position as the vehicle moves, is useful data,
not noise, so every scan result gets logged every phase. If GPS has no
fix yet, rows still get logged with `fix=0` rather than losing that
scan's data outright.

A new `WarDriveService` (`include/war_drive_service.h` /
`src/war_drive_service.cpp`) drives the whole thing **non-blockingly**:
`WiFi.scanNetworks(true, true)` + polled `WiFi.scanComplete()` for the
Wi-Fi phase, `NimBLEScan::start()` + polled `isScanning()` + the no-arg
`getResults()` for the BLE phase — both confirmed genuinely asynchronous
by reading this project's own installed `WiFiScan.h`/`.cpp` and
`NimBLEScan.h`. On-screen, the two counters (`Unique APs`, `Unique
devices`) track distinct BSSIDs/addresses seen this session (same
fixed-array-plus-linear-scan idiom as
`WifiSnifferService::noteUniqueMac()`), separately from the CSV rows,
which log every observation.

**First attempt at this (reverted before it was ever committed) called
the existing blocking `scanWifiNetworks()`/`scanBleDevices()` directly
from `loop()`.** Both block for several seconds and draw their own
full-screen UI as a side effect — on hardware this hijacked the War Drive
screen every phase and, worse, meant `handleInput()` never got a chance
to run mid-scan, so Backspace/Q presses queued up instead of being
handled (reported as "can't exit without rebooting"). The non-blocking
`WarDriveService` redesign above replaces that approach entirely; the
screen now never shows anything but its own live counters, and
Backspace/Q works instantly regardless of which phase is in flight.

Two more bugs found and fixed on the first hardware test of the redesign:
- The BLE phase's `scanner->start(duration, ...)` call was passing
  seconds where NimBLE expects **milliseconds** (`ble_scanner.cpp`'s own
  blocking scan multiplies its seconds parameter by 1000 before passing
  it down — this new code didn't). The BLE scan window was effectively
  5ms, not 5 seconds, so it only ever caught whichever single
  advertisement happened to already be mid-flight. Fixed; BLE devices
  found now match what a normal BLE Discovery scan finds nearby.
- `drawWarDrive()` redrew the whole screen (via `drawHeader()`'s
  full-screen clear) on every ~500ms tick, causing a visible flicker.
  Split into a static part (header/footer, drawn once on screen entry)
  and `drawWarDriveDynamic()` (targeted `fillRect` + reprint of just the
  status/GPS/phase/counter lines), called for every periodic tick and key
  press instead.
- One more found on a second hardware pass: the Wi-Fi phase's unique
  count stayed stuck at 0 even though a normal Wi-Fi Discovery scan
  worked fine. `WiFiScanClass::scanComplete()` self-aborts as
  `WIFI_SCAN_FAILED` after `max_ms_per_chan*20` (6s at the default
  300ms/channel) — shorter than `scanWifiNetworks()`'s own blocking wait
  (10s) for the identical scan, so a full-channel scan taking 6-10s
  reported failed on the async/polled path here even though it would
  have succeeded synchronously. Fixed by passing 500ms/channel to
  `WiFi.scanNetworks()`, raising the internal timeout to 10s to match.

**Confirmed on hardware, all three issues resolved**: BLE and Wi-Fi
unique counts both increase correctly, the screen no longer flickers, and
Backspace/Q still exits instantly mid-scan.

## 0.3.3.1 — Chameleon Ultra pairing (new "RFID" menu category)

New top-level **RFID** category (`Wi-Fi / BLE / RFID / GPS / Mesh / Tools /
Settings`, 7 items now) with one item, **Chameleon Ultra**. This board has
no onboard RFID chip, so RFID assessment is done by pairing with the
user's own Chameleon Ultra over BLE, mirroring Bruce firmware's RFID menu.

This increment is deliberately small and connectivity-only: connect over
BLE, query firmware version and battery, nothing tag-related yet. The
[[cardputer_adv_ble_chameleon_research]] memory flagged an unverified
assumption from the upstream `bmorcelli/ESP-ChameleonUltra` library — that
the Chameleon Ultra accepts a plain BLE `connect()` with no bonding/pairing
step — and that needed confirming against real hardware before building
the full read/clone/emulate UI on top of it.

`ChameleonUltraClient` (`include/chameleon_ultra_client.h` /
`src/chameleon_ultra_client.cpp`) implements the device's binary command
protocol independently from the **official** protocol specification
(`RfidResearchGroup/ChameleonUltraDocs/protocol.md`, read directly for this
increment — GPL-3.0-licensed docs, so the implementation below is written
from the factual protocol description rather than copied text or code):

- Frame format: `SOF(0x11) | LRC1 | CMD(2, BE) | STATUS(2, BE) | LEN(2, BE)
  | LRC2 | DATA | LRC3`, where LRC is the 8-bit two's-complement of the sum
  of the bytes it covers.
- `GET_APP_VERSION` (1000) and `GET_BATTERY_INFO` (1025), the two commands
  used this round.
- Transport: the device exposes the standard, industry-wide **Nordic UART
  Service** (BLE central connect, write commands to one characteristic,
  subscribe to notifications on another) — confirmed via the AGPL
  reference library, but these are public/generic UUIDs, not
  Chameleon-specific, so reusing them directly is no different from
  reusing the public company/service IDs in the BLE Spam work.

Radio handoff and teardown follow the same discipline established by the
BLE Spam crash fix: Wi-Fi off before NimBLE init, a settle delay before
`NimBLEDevice::deinit(true)` on disconnect.

**Confirmed on hardware against the user's real Chameleon Ultra**: the
flagged assumption held — it connects with a plain `connect()`, no
bonding/pairing step required. One practical note: the Chameleon Ultra
needs its own button held long enough to be actively awake/advertising
when you press R, or the connect attempt won't find it; once connected
normally, the link stays up (no unexpected drops).

## 0.3.3.3 — Chameleon Ultra continuous scan + automatic CSV export

Two additions to the Chameleon Ultra screen, both requested before this
work gets committed:

- **Continuous scan** (`C` key): instead of only scanning on a manual `S`
  press, toggles a ~500ms polling loop (paced in `loop()`, each attempt
  still a blocking BLE round-trip like every other radio call in this
  codebase) that keeps trying `HF14A_SCAN`/`EM410X_SCAN` until turned off.
- **Automatic CSV export**: a new `chameleonLogger` (`SdLogger`, same class
  every other CSV feature in this project uses) starts the moment a
  connection succeeds — no manual log-toggle needed, unlike this
  project's usual `L`-key convention. Every *distinct* tag capture (not
  every poll) gets one row: `timestamp_utc,tag_type,id,atqa,sak` under
  `/ghostwire/logs`. Dedup is signature-based (tag type + ID) and resets
  whenever no tag is present, so presenting the same card again after
  removing it logs a fresh row, but leaving one card sitting on the reader
  during continuous scanning doesn't spam the file every 500ms.

Both the manual and continuous scan paths now share one
`performChameleonScan()` function so their logging behavior can't drift
apart.

**Confirmed on hardware**: single scan, continuous scan, and CSV export
all work — connection stays up, `Logged` counts correctly (once per
distinct capture, not per poll), and the CSV rows are readable via the
Files browser afterward. (An earlier test session on this same build
appeared to show the Chameleon Ultra losing connection/powering off after
a scan; a follow-up build added a "Connecting..." status message and a
keyboard-recovery call after the blocking connect — the user could then
tell when the connection attempt had actually finished, and the earlier
symptom didn't reproduce. Root cause was never conclusively pinned to a
firmware bug — noting this rather than claiming a fix that wasn't
verified against a captured cause.)

## 0.3.3.2.1 — Fixed tag scan finding nothing (device stuck in emulator mode)

0.3.3.2's `S` scan found no tags at all on hardware, even though the
user's phone app connected to the same Chameleon Ultra and read cards
fine over BLE — proving the device and radio were working, so the bug had
to be in Ghostwire's firmware. Root cause: the protocol has a
`CHANGE_DEVICE_MODE` command (1001, emulator=0x00/reader=0x01) that
`ChameleonUltraClient::connect()` never called. `HF14A_SCAN`/`EM410X_SCAN`
only work in reader mode; if the device was last left in emulator mode
(e.g. from prior use with an app), our scans would find nothing no matter
what. Fixed by sending `CHANGE_DEVICE_MODE(reader)` right after connecting
and subscribing, before returning control to the caller, rather than
assuming whatever mode the device happened to be in.

**Confirmed on hardware**: tag scanning now finds cards correctly.

## 0.3.3.2 — Chameleon Ultra card read (HF14A + EM410x scan)

Extends 0.3.3.1's proven BLE connection with an actual read: pressing `S`
on the Chameleon Ultra screen now runs `HF14A_SCAN` (2000) then, if no HF
tag was found, `EM410X_SCAN` (3000) — command IDs and response layouts
read from the same official `protocol.md` as 0.3.3.1. A found HF tag shows
UID/ATQA/SAK; a found LF (EM410x) tag shows its 5-byte ID; neither shows
"No tag found". The user doesn't need to know which antenna a given card
uses — both get tried.

Found and fixed a real bug while adding this: `ChameleonUltraClient`'s BLE
notify handler was overwriting its response buffer on every callback
instead of appending. Harmless for 0.3.3.1's two fixed-size responses
(they always fit in one BLE notification), but `HF14A_SCAN` responses
include a variable-length ATS field that can span multiple notifications
— fixed to accumulate and only signal "response ready" once the frame's
own `LEN` field says enough bytes have arrived. Also added a capacity
check in `sendCommand()` so a larger-than-expected response can't overflow
a caller's buffer, now that response size is genuinely variable rather
than fixed.

## 0.3.2.3 — BLE Spam (Apple / Fast Pair / Swift Pair pairing-popup spam)

New `BLE → Spam` feature: a mode-select screen (**Apple**, **Fast Pair**,
**Swift Pair**, **All**) followed by a live spam screen showing the current
protocol, packets-sent counter, and the spoofed MAC address in use.
`BleSpamService` (`include/ble_spam_service.h` / `src/ble_spam_service.cpp`)
cycles a new advertisement every 300 ms with a freshly randomized static
BLE address, so each cycle looks like a distinct new device to a nearby
phone/PC.

Implemented independently from public/official protocol documentation, not
copied from Bruce firmware's AGPL-3.0 source:

- **Apple Continuity Proximity Pairing** (manufacturer ID `0x004C`, message
  type `0x07`) — cross-verified against two independent sources (the
  academic `furiousMAC/continuity` project and the `librepods` project),
  cycling four confirmed AirPods model IDs.
- **Google Fast Pair** (Service UUID `0xFE2C`, per Google's own official
  spec) — cycles three real, publicly registered device Model IDs (Bose NC
  700, JBL Buds Pro, JBL Live 300TWS); Fast Pair is designed so any phone
  can look these up, that's the point of the advertisement.
- **Microsoft Swift Pair** (Vendor ID `0x0006`, Beacon ID `0x03`) — built
  from Microsoft's own Swift Pair developer documentation, cycling a few
  display-name strings.

**Samsung Buds/Watch "Easy Setup" spam is deliberately not included in this
release.** Only one raw hex capture was found (from the `GalaxyBudsClient`
project) with no reliable independent field-level breakdown — shipping a
guessed byte layout without hardware to verify against isn't worth the
risk (same discipline already applied to the Chameleon Ultra pairing
caveat). Revisit if a real Samsung device becomes available to test
against.

Radio handoff follows the same discipline the NimBLE migration's crash fix
established: `begin()` turns Wi-Fi off before initializing NimBLE, and
`end()` fully calls `NimBLEDevice::deinit(true)` on every exit path so a
later Wi-Fi scan doesn't crash.

**Hardware-tested, fixed one real crash, but not confirmed to trigger real
popups — shelved in this state.** First flash rebooted the device every
time spam was stopped; root-caused to a genuine asymmetry in
`NimBLEDevice::deinit()` (it calls `m_pScan->onHostDeinit()` to let a scan
settle before host teardown, but has no equivalent hook for advertising —
it just deletes the `NimBLEAdvertising` object outright). Fixed with a
settle delay between `stop()` and `deinit()` in `BleSpamService::end()`;
confirmed on hardware that stopping no longer reboots the device. However,
the Fast Pair advertisement (Service UUID `0xFE2C` + Model ID, now also
with an explicit Complete-Services declaration added to try to help
detection) did not produce a notification on the user's Android phone in
this test session. Root cause undetermined — could be the phone's
Nearby/Fast Pair setting being off, a phone-specific quirk, or a subtle
protocol detail this session's research didn't catch. Apple and Swift Pair
were not verified against a real iPhone/Windows PC either. Parking this
feature here rather than iterating further blind; revisit with either a
confirmed-working reference device to compare against, or once real
popups can be confirmed on at least one platform.

## 0.3.2.2 — Reorganized main menu into Wi-Fi / BLE / GPS / Mesh / Tools / Settings

Cross-cutting navigation change, not BLE-specific — the flat 13-item main
menu was only going to keep growing as BLE Spam/Sniffer/HID and Chameleon
Ultra RFID pairing land. The main menu is now 6 categories: **Wi-Fi**
(Discovery, Sniffer), **BLE** (Discovery), **GPS** (GNSS Monitor), **Mesh**
(LoRa/Meshtastic), **Tools** (Infrared, USB/HID, Audio, Logs/Sessions,
Motion/IMU, Files, System, About), **Settings** (unchanged, no wrapper
needed).

Generalizes the existing `Screen::CapMenu` pattern (which grouped GNSS and
LoRa together) to all six categories, splitting GPS and Mesh into separate
top-level entries rather than one combined "GNSS / LoRa Cap" item. BLE,
GPS, and Mesh each get their own one-item submenu today by design — each
is expected to grow a second item soon (BLE Spam next) — rather than
special-casing single-item categories to skip straight to their one
feature, keeping navigation depth consistent everywhere.

No behavior changes to any individual screen — every leaf screen (Wi-Fi
Discovery scan, deauth, handshake capture, BLE Discovery scan, GNSS
monitor, LoRa receive, and all 8 Tools items) works exactly as before,
reached through one extra keypress from its new category menu instead of
directly from the main menu.

**Confirmed on hardware**: every category opens correctly, every leaf
screen still works and returns to the right submenu on Backspace/Q, and
the deeper Wi-Fi flows (detail → deauth → handshake capture) still chain
through properly.

## 0.3.2.1.1 — Fixed crash on BLE scan followed by Wi-Fi scan

0.3.2.1 introduced a genuine crash (confirmed via on-device boot telemetry:
`Last reset: Panic/crash`, not a hang) doing a BLE scan followed by a Wi-Fi
scan. Root cause: `BleScanner` initialized NimBLE once and never released
it, matching the old code's behavior with the classic Bluedroid BLE stack
-- but unlike Bluedroid, leaving NimBLE's controller/memory initialized
while a Wi-Fi scan starts corrupts something in the shared radio
coexistence layer. Fixed by calling `BLEDevice::deinit(true)` at the end of
every scan (and in `stop()`), fully releasing BLE's resources -- symmetric
with how starting a BLE scan already turns Wi-Fi off first. Diagnosed via
this project's own boot/reset telemetry (`CHANGELOG.md`'s 0.2.18 feature)
rather than live serial capture, since the crash-triggered reboot killed
the USB CDC connection before any panic backtrace could be captured.

**Confirmed fixed on hardware**: BLE scan followed by Wi-Fi scan no longer
crashes. This completes 0.3.2.1's BLE stack migration.

## 0.3.2.1 — Migrated BLE Discovery to NimBLE-Arduino

First step of the BLE assessment phase (after the completed 0.3.1.x Wi-Fi
arc): swapped `src/ble_scanner.cpp`'s implementation from the classic
Bluedroid-backed ESP32 Arduino BLE stack to `h2zero/NimBLE-Arduino@2.5`,
with no behavior change — same fields, same sort order, same public
`BleScanner`/`BleDeviceInfo` interface, so `src/main.cpp` needed no changes
at all. This is the prerequisite for every planned BLE feature (spam,
sniffer, HID, and the Chameleon Ultra RFID pairing client), since only one
BLE host stack can be active on the radio at a time.

Verified against Bruce firmware's own NimBLE scan code (same pinned
library version) rather than guessed: `getResults(timeoutMs, false)`
replaces `start(durationSeconds, false)`, and `getDevice(i)` returns a
`const NimBLEAdvertisedDevice*` (null-checked) instead of a value, both
confirmed from their working `ble_common.cpp` before writing this. An
empty `NimBLEScanCallbacks` override is registered defensively, matching
their configuration. One real API gap Bruce's reference didn't cover:
`NimBLEAdvertisedDevice` has no `haveRSSI()` (checked the installed
library header directly) — RSSI is unconditionally available, so
`getRSSI()` is called directly with no guard.

**Measured, not just expected**: RAM usage dropped from 26.9% to 22.5% and
flash from 53.9% to 42.4% after the swap — NimBLE's smaller footprint than
the classic Bluedroid stack, confirmed rather than assumed.

## 0.3.1.3 — PMKID/handshake capture to .pcap

Third and final 0.3.1.x Wi-Fi assessment release. Adds targeted WPA2
handshake/PMKID capture, chained directly off deauth: from an AP's detail
view, `H` starts capturing EAPOL traffic for that specific AP and enters a
new "Handshake Capture" screen where `D` fires the same deauth used in
0.3.1.2 without leaving the screen, to force a fresh handshake.

- `WifiSnifferService` extended (not replaced) to also match 802.11 Data
  frames against a target BSSID and detect EAPOL (LLC/SNAP + EtherType
  0x888E) traffic, queuing raw frame bytes into a second, small ring
  buffer. Probe-request sniffing keeps working unchanged; the promiscuous
  filter mask gains `WIFI_PROMIS_FILTER_MASK_DATA` only while a handshake
  target is set.
- New `EapolParser` (stateless, mirroring `MeshtasticDecoder`): identifies
  which of the WPA2 4-way handshake's 4 messages a captured frame is (via
  the EAPOL-Key `key_info` ACK/MIC/Secure bits) and extracts the PMKID from
  Message 1's vendor KDE when the AP provides one (the modern "PMKID
  attack" shortcut that doesn't require a full handshake).
- New `PcapLogger` (sibling to `SdLogger`, binary rather than CSV):
  streams standard libpcap-format records straight to SD one frame at a
  time, never buffering a capture in RAM. Files open directly in
  Wireshark/aircrack-ng.
- The capture screen shows live message-number coverage (M1-M4), the
  PMKID in hex as soon as one is seen, and REC/frame-count status.
- Captures everything matching the target BSSID regardless of whether our
  own parsing understood it — a raw pcap is still useful even if parsing
  missed something.

**Confirmed working on hardware**: a full 4-way handshake (all of M1-M4)
captured correctly on the first hardware test after a deauth-forced
reconnection (~40 EAPOL frames total); no PMKID from this particular AP,
which is expected since not all APs include one in Message 1. Plain
probe-sniffing and Wi-Fi/BLE Discovery confirmed unaffected. This completes
the 0.3.1.x Wi-Fi assessment arc: sniffer → deauth → handshake/PMKID
capture.

## 0.3.1.2.3 — Root cause found: closed-source driver blocks deauth frames

0.3.1.2.2's fix didn't help either — identical 258, and `esp_wifi_set_channel`
reported success (0), ruling out the radio-mode race too. The real cause:
Espressif's closed-source WiFi driver deliberately rejects deauthentication/
disassociation frame types in `esp_wifi_80211_tx()` (their own stated reason,
per `espressif/esp-idf` issue #1256, is specifically to prevent raw deauth
transmission). No amount of adjusting call sequencing or frame content could
have changed that outcome, which is exactly why three different targeted
fixes produced the identical error.

Studied how Bruce firmware (AGPL-3.0) achieves deauth: `objcopy
--weaken-symbol` on `ieee80211_raw_frame_sanity_check` inside `libnet80211.a`,
paired with their own definition of that symbol so the linker prefers it.
Their exact code isn't reusable here — it was reverse-engineered against a
materially newer Arduino-ESP32 core (3.x) than Ghostwire's pinned 2.0.16, and
disassembling *our* actual `libnet80211.a` (Arduino-ESP32 2.0.16, ESP32-S3)
showed a different parameter layout and return-value polarity than their
override assumes. Added an independently-written override
(`src/wifi_raw_frame_override.cpp`) calibrated to what was actually verified
in our binary via `objdump`/`readelf`: `esp_wifi_80211_tx(ifx, buffer, len,
en_sys_seq)` calls the check with exactly those four arguments, and 0 (not 1)
means "allowed" — the override keeps the real length-bound check (24-1500
bytes) and removes only the frame-type restriction. Applied via a new
PlatformIO pre-build step (`patch_wifi_lib.py`) that weakens the symbol in
the toolchain's own bundled library — no toolchain swap, no third-party
binary download. Reverted the earlier `en_sys_seq=true` change back to
`false` (matching the untouched original documented behavior, since the
actual blocker was always the frame-type check) and added the `ESP_ERR_NO_MEM`
retry Bruce's own wrapper uses.

This is inherently fragile: it relies on undocumented internal behavior of a
closed-source binary tied to this exact toolchain version. Any future
`platform =`/framework version bump in `platformio.ini` needs re-verification
by disassembly before trusting this still works — see the extensive comment
in `wifi_raw_frame_override.cpp`.

**Confirmed working on hardware**: deauth successfully disconnected other
Wi-Fi devices from a test AP, and Wi-Fi Discovery, BLE Discovery, and the
Wi-Fi Sniffer all still work normally afterward — the radio is left in a
sane state and nothing else was destabilized by patching the library. 0.3.1.2
is complete.

## 0.3.1.2.2 — Deauth radio-mode race fix

0.3.1.2.1's `en_sys_seq` fix didn't help — identical `ESP_ERR_INVALID_ARG`
(258) both before and after, which means it was never the actual cause.
`transmitWifiDeauth()` was the only place in the file that calls
`WiFi.mode(...)` and immediately uses low-level radio APIs with no settle
delay; `scanWifiNetworks()` and `BleScanner::scan()` both wait after
changing Wi-Fi mode before touching the radio further, since the mode
switch is asynchronous. Added the same `delay(150)` here. Also now logs
(Serial and the on-screen status) both `esp_wifi_set_channel()`'s and
`esp_wifi_80211_tx()`'s return codes, not just the latter, in case this
isn't the whole story.

## 0.3.1.2.1 — Deauth frame fix

Fixes 0.3.1.2 always failing with `ESP_ERR_INVALID_ARG` (258) on hardware.
`esp_wifi_80211_tx`'s own header documents why: `en_sys_seq` must be `true`
once Wi-Fi's connection state is set up (which scanning already does) —
passing `false`, as the initial implementation did, is only valid before
that point. Switched to `true`; the driver fills in the sequence number
instead of the frame's own zeroed field, which doesn't matter for a deauth
frame.

## 0.3.1.2 — Single-target deauthentication

Second 0.3.x assessment-tool release. Adds a confirmation-gated
deauthentication action to the existing Wi-Fi Discovery AP detail view —
the first feature in the firmware that transmits disruptive frames rather
than only listening.

- `D` on an AP's detail screen opens a dedicated confirm screen ("Deauth
  AP?", showing SSID/BSSID, an "Authorized targets only." reminder) before
  anything is sent — following Evil-M5Project's confirm-before-disruptive-
  action pattern from the earlier prior-art research, not Bruce's
  fire-immediately approach. Backspace/Q cancels with no frames sent.
- Enter sends a bounded burst (8 frames, ~560ms total) of a standard 802.11
  deauthentication frame to the target AP's BSSID (broadcast destination,
  reason code 7), on the AP's own channel. This is a single explicit
  action, not a persistent auto-deauth loop.
- Extracted `drawWifiDetail()` out of what was previously inline-only
  drawing code, since the new confirm/cancel flow needs to redraw that
  screen from more than one place.
- Status ("Deauth sent"/"Deauth failed") shown in the AP detail screen's
  footer afterward.

## 0.3.1.1 — Wi-Fi probe-request sniffer

First 0.3.x assessment-tool release. Adds a new "Wi-Fi Sniffer" main-menu
screen: passive, read-only 802.11 management-frame sniffing for probe
requests (device MAC, SSID requested, RSSI, channel), hopping channels
1/6/11 every ~400ms. No frame injection — that's reserved for a future
release once this promiscuous-mode/channel-hop foundation has been used and
tested.

- New `WifiSnifferService` (`include/wifi_sniffer_service.h`,
  `src/wifi_sniffer_service.cpp`): registers an `esp_wifi` promiscuous-mode
  callback filtered to management frames only, parses probe-request frames
  directly (frame-control byte check, SSID information element), and hands
  parsed records to the main loop through a fixed-size ring buffer — no
  heap allocation in the callback, and nothing is ever buffered in RAM
  beyond that fixed ring (this board has no PSRAM).
- Logs to `/ghostwire/logs/wifi_probes_NNNN.csv` via the existing
  `SdLogger`, toggled with `L` like the GNSS/LoRa loggers.
- Establishes the first symmetric radio-teardown path in the firmware: the
  screen's exit path explicitly disables promiscuous mode before returning
  to the main menu, so Wi-Fi/BLE discovery are unaffected afterward. Prior
  Wi-Fi code never needed this since nothing else held a promiscuous
  callback registration.

## 0.2.19.6 — Longer title hold, Fast boot setting

- Extended the boot title card's post-reveal hold by one second (1.2s to
  2.2s) so it's easier to actually read.
- Added a "Fast boot" Settings toggle (default off). When enabled, skips the
  animated diagnostic console, the title card's decrypt-reveal animation,
  and the fixed screen holds, going straight to the final diagnostics/title
  content with only a brief pause. Boot sound is unaffected — it's a
  separate setting, and the chime still plays in full if enabled, since
  hearing it is functional feedback, not decorative delay.

## 0.2.19.5 — Renamed to Ghostwire, boot title rework

- Renamed the product from Fieldnote to Ghostwire throughout: `branding.h`,
  all on-device strings, the `/ghostwire` SD folder convention (was
  `/fieldnote`), the `ghostwire` Preferences namespace (was `fieldnote`), and
  documentation. Existing SD cards with `/fieldnote/...` data need that
  folder renamed to `/ghostwire/...` manually to keep showing up in Logs and
  the Files browser; saved settings under the old Preferences namespace
  won't carry over and will reset to defaults once.
- Reworked the final boot title card: HUD-style corner brackets replace the
  plain rounded rectangle border, and the product name resolves out of
  scrambled characters left-to-right (decrypt/brute-force style) instead of
  appearing all at once, ending in a blinking terminal cursor after the
  version string. Still skippable with any keypress, and total boot-title
  duration is unchanged (~1.85s, close to the prior fixed 2s hold).

## Known issue — boot chime silent on some cold boots (root cause found, not fixable in firmware)

0.2.19.4's diagnostic showed `Speaker.begin()` reporting "Played immediately"
on a cold boot where nothing was actually heard, ruling out every software
theory tried in 0.2.19.1–0.2.19.4 (codec I2C enable timing, retry budgets).
The official Cardputer ADV schematic, page 3, shows why: the speaker is driven
by a discrete amplifier, `U5` (NS4150B),
whose enable pin (`CTRL` / net `AMP_EN`) is wired to the headphone jack's
mechanical detect switch (`J7`, via `Q4`/`HP_DET`/`R40`/`D3`) — not to any
ESP32 GPIO. Firmware has no code path to this signal at all. A physical
reset button doesn't touch that analog network, which is why it only shows
up on a true cold power-on. Fixing this needs a hardware change (e.g.
bridging `AMP_EN` to a spare GPIO) — not a firmware change. Left as-is
pending hardware access to confirm and fix.

## 0.2.19.4 — Boot chime status visible in System Diagnostics

- 0.2.19.3's deferred retry still didn't produce sound on a cold boot, and
  live serial capture isn't viable for diagnosing it: reproducing the actual
  cold-boot path requires disconnecting USB, so nothing is listening when it
  happens. Added a "Boot chime" row to System Diagnostics instead, recording
  whether it played immediately, played late (with elapsed ms), gave up after
  20 seconds, or is disabled — checkable on-device after the fact, with no
  USB connection needed during the test itself.

## 0.2.19.3 — Boot chime deferred until the speaker is actually ready

- 0.2.19.2's five retries (100ms total) still weren't enough: confirmed on
  hardware that on a cold boot the speaker isn't ready for several seconds
  after `Speaker.begin()` first starts succeeding — long after the sound
  test in the Audio menu succeeds on the same session, well past this boot
  screen's time budget. Blocking boot for that long isn't acceptable, so the
  chime attempt no longer blocks: if the first attempt fails, boot continues
  normally and `loop()` retries every 250ms for up to 10 seconds, playing the
  chime as soon as the speaker comes up rather than giving up on the first
  boot screen's schedule. Given a 20-second retry budget rather than 10, since
  retrying costs nothing once the speaker is ready and stops immediately.

## 0.2.19.2 — Boot chime reliability, corrected (superseded, see 0.2.19.3)

- 0.2.19.1's fix was based on an incorrect assumption (that
  `M5Cardputer.begin()` calls `Speaker.begin()` early) and had no effect.
  `Speaker.begin()` is actually first called at the boot chime itself, and
  its single, non-retried internal codec-enable I2C write can fail there on
  a cold boot without M5Unified retrying or surfacing the failure. Retried
  `Speaker.begin()` up to five times before giving up on the chime, since
  the returned success/failure is now checked instead of discarded. Not
  enough retry budget in practice — see 0.2.19.3.

## 0.2.19.1 — Boot chime reliability (superseded, see 0.2.19.2)

- Attempted fix forced the ES8311 codec enable sequence to re-run
  immediately before the boot chime plays. Ineffective: `Speaker.begin()`
  had not been called before this point in boot, so re-issuing it changed
  nothing.

## 0.2.19 — Foundation polish

- Added a persistent Settings toggle for a short two-note boot-ready chime.
- Removed the duplicate heap-allocated System Diagnostics row representation,
  reducing work and heap churn on every diagnostic redraw.
- Removed high-frequency serial printing from the microphone sampling path.
- Reduced ESP32 framework logging from debug to error-only for the release
  build while retaining Ghostwire's explicit serial status output.
- Added a graceful low-memory fallback if the smooth boot-console sprite
  cannot be allocated.
- Kept the boot summary and title timing at one and two seconds respectively.

## 0.2.18 — Boot history and recovery foundation

- Added persistent boot and abnormal-reset counters in internal preferences.
- Classifies panic, watchdog, and brownout resets as stability events while
  leaving normal power-on, upload, and software resets unflagged.
- Appends startup telemetry to `/ghostwire/logs/boot_history.csv`.
- Added boot count, stability-event count, and boot-history write state to
  System Diagnostics, diagnostic exports, and the scrolling boot console.

## 0.2.17.2.1 — Boot-card timing

- Set the System Ready summary duration to exactly one second.
- Set the final Ghostwire/Zetascrub title-card duration to exactly two seconds.

## 0.2.17.2 — Enhanced boot console

- Added a smooth sprite-rendered scrolling console covering firmware, reset
  cause, CPU, heap, application space, battery, and subsystem health.
- Tied boot progress to completed diagnostic entries.
- Added a colour-coded ready/deferred/warning summary before the Ghostwire
  title card.
- Preserved deferred LoRa and IMU initialization for a quick, safe boot.

## 0.2.17.1.1 — Boot rendering hotfix

- Replaced full-region boot redraws with incremental rendering.
- Draws static content and each diagnostic result only once.
- Updates only the spinner glyph and newly filled progress-bar pixels during
  the animation, eliminating visible full-screen flicker.

## 0.2.17.1 — Diagnostic-driven boot

- Reworked system diagnostics into structured health records shared by the
  System screen, SD reports, and boot sequence.
- Boot now uses the same microSD, USB HID, GNSS, LoRa, and IMU states shown in
  System Diagnostics.
- Added consistent green readiness, amber warning, and muted not-probed state
  presentation.

## 0.2.17 — System diagnostics foundation

- Replaced the compact System dashboard with a scrollable health report.
- Added firmware, uptime, reset cause, battery, heap, application-space, chip,
  SD, USB HID, GNSS, LoRa, IMU, and clock diagnostics.
- Added one-key text-report export to `/ghostwire/logs`.
- Centralized the diagnostic rows for reuse by future boot-screen checks.

## 0.2.16.4.1 — Title-card timing

- Extended the final Ghostwire/Zetascrub boot title card by one second, from
  0.9 seconds to 1.9 seconds.

## 0.2.16.4 — Live boot screen

- Removed the embedded GIF and its decoder from the firmware image.
- Replaced it with a live terminal-style initialization screen, animated
  spinner, staged checks, progress bar, version, and final title card.
- Retained key-to-skip support for the diagnostic stage.

## 0.2.16.3 — Boot diagnostics

- Added the running firmware version and staged hardware checks over the
  animated Matrix boot screen.
- Reports USB HID, GNSS interface, microSD, and battery status from the real
  startup state.
- Added a final Ghostwire and Zetascrub title card before entering the menu.

## 0.2.16.2 — Matrix UI theme

- Reworked the shared Ghostwire palette around the boot animation's black and
  green Matrix styling.
- Added bright green terminal separators to headers and footers.
- Kept amber reserved for warnings so important fault states remain distinct.

## 0.2.16.1 — Zetascrub boot animation

- Embedded the 240×135 `zetascrub_cardputer_boot.gif` as the full-screen boot
  animation.
- Added frame-delay-aware playback, key-to-skip support, and a five-second
  safety limit.
- Retained the original Ghostwire splash as a decoder-failure fallback.

## 0.2.16 — Logging session manager

- Added a dedicated main-menu browser for `/ghostwire/logs`.
- Added subsystem identification, file size, and CSV data-row metadata.
- Added direct session preview using the bounded text viewer.
- Added exact-file deletion behind a separate irreversible-action
  confirmation screen.

## 0.2.15 — Discovery export

- Added one-key CSV export of the current Wi-Fi discovery results.
- Added one-key CSV export of the current BLE advertisement results.
- Included synchronized UTC timestamps, radio metadata, safe CSV quoting, and
  sequential non-overwriting filenames under `/ghostwire/logs`.

## 0.2.14.4 — Reserved 0.2.11 integration

- Integrated the completed 0.2.11 Meshtastic public-channel decoder into the
  latest foundation build without regressing the installed version number.
- Made LoRa packet reception binary-safe and added decoded mesh metadata to
  LoRa event logs.

## 0.2.14.3 — Automatic UI status refresh

- Added a lightweight one-second refresh for the global clock and battery
  region without repainting complete screens.
- Added automatic once-per-second local and UTC clock readout updates.
- Retained event-driven rendering for static content to minimize flicker.

## 0.2.14.2 — Header clock

- Added synchronized UK local time in `HH:MM` format to the global top bar.
- Leaves the clock area blank until valid GNSS time has synchronized, avoiding
  misleading boot-time values.

## 0.2.14.1 — UK local-time display

- Added UK local time to the clock screen with automatic GMT/BST transitions.
- Kept a separate UTC readout and retained ISO-8601 UTC timestamps in logs.

## 0.2.14 — UTC time foundation

- Added a UTC clock/status screen under System.
- Added manual and automatic system-clock synchronization from valid GNSS
  date/time data.
- Added ISO-8601 UTC timestamps to new IMU, GNSS, and LoRa CSV logs while
  retaining elapsed milliseconds.
- Explicitly reports that Cardputer-Adv has no documented battery-backed RTC,
  so synchronization is required after a full power-off.

## 0.2.13.3 — LoRa event logging

- Added event-driven CSV logging for newly received SX1262 packets.
- Recorded packet number, active profile, frequency, RSSI, SNR, and a
  bounded CSV-safe payload preview.
- Added live LoRa recording state and row count, with safe close on stop or
  menu exit.

## 0.2.13.2 — GNSS logging

- Extended the reusable SD logger to the GNSS monitor.
- Added one-second CSV samples containing elapsed time, UTC, fix state,
  latitude, longitude, altitude, satellites, and HDOP.
- Added live GNSS recording state and row count, with safe close on stop or
  menu exit.

## 0.2.13.1 — Text and CSV preview

- Added read-only previews for CSV, TXT, and LOG files in the SD browser.
- Added vertical line scrolling and horizontal column scrolling.
- Bounded previews to 128 lines of 160 characters to protect firmware memory;
  larger files are clearly marked as truncated.

## 0.2.13 — SD logging foundation

- Added a reusable CSV logger with sequential filenames under
  `/ghostwire/logs`.
- Added 10 Hz IMU recording of elapsed time, native acceleration axes, and
  calibrated gyroscope axes.
- Added one-second periodic flushing and safe close-on-stop or menu exit.
- Added an on-screen recording indicator and persistent row counter.

## 0.2.12.2 — IMU pitch direction hotfix

- Inverted the landscape pitch sign so lifting the Cardputer's SD-card/top
  edge reports positive pitch.

## 0.2.12.1 — IMU orientation hotfix

- Rotated the human-facing orientation frame 90 degrees clockwise to match
  the Cardputer's landscape screen and keyboard.
- Kept the displayed accelerometer axes in their native sensor frame for
  diagnostics.

## 0.2.12 — IMU foundation

- Added Cardputer-Adv motion-sensor detection and identification.
- Added live accelerometer and gyroscope axes, pitch/roll estimates, and
  stationary/moving status.
- Added a non-blocking 100-sample stationary gyro calibration.
- Reserved 0.2.11 for future read-only Meshtastic public-channel decoding.

## 0.2.11 — Meshtastic public-channel decoding

- Added receive-only parsing of the 16-byte Meshtastic radio header.
- Added AES-CTR decoding using the standard public-channel key represented by
  the `AQ==` alias.
- Added bounded protobuf parsing for application port and payload fields.
- Added sender, port type, plaintext message display, and decoded LoRa log
  fields without transmitting or joining the mesh.

## 0.2.10.1 — Meshtastic receive profile

- Added the Meshtastic EU_868 LongFast receive parameters: 869.525 MHz,
  BW250, SF11, CR4/5, sync word 0x2B, and 16-symbol preamble.
- Made Meshtastic LongFast the default receive-only profile.
- Added a profile toggle retaining the M5Stack generic LoRa example profile.
- Kept payload handling passive; encrypted mesh traffic is counted and shown
  through radio metadata without attempting decryption.

## 0.2.10 — LoRa receive foundation

- Added passive SX1262 receive monitoring at the UK/EU 868 MHz band.
- Enabled the Cap LoRa-1262 RF switch through its PI4IOE5V6408 I/O expander.
- Added radio initialisation status, packet count, RSSI, SNR, and payload
  preview.
- Added a GNSS/LoRa cap submenu and intentionally omitted transmission.

## 0.2.9 — GNSS foundation

- Added receive-only ATGM336H/AT6668 GNSS support for Cap LoRa-1262.
- Added UART/NMEA detection, fix status, satellites, HDOP, UTC, coordinates,
  and altitude.
- Used the documented Cardputer-Adv UART pins G15/G13 at 115200 baud.
- Left the SX1262 radio uninitialised pending antenna confirmation.

## 0.2.8.1 — Battery reporting hotfix

- Averaged GPIO10 battery ADC samples and applied an exponential filter.
- Limited the displayed estimate to one percentage-point movement per second
  so transient audio/radio loads do not appear as capacity jumps.
- Replaced ambiguous charge status with an explicit unsupported indication;
  Cardputer-Adv hardware does not expose charger-state sensing.

## 0.2.8 — Power foundation

- Added a Li-ion discharge-curve battery percentage and low-battery header
  warning.
- Added battery voltage and charge-state information to System diagnostics.
- Added a persistent Off/15/30/60/120-second screen timeout.
- Added keyboard wake from display sleep without risking unavailable
  deep-sleep keyboard wake.

## 0.2.7 — SD browser foundation

- Added a file-details screen with type, size, and directory information.
- Added direct MP3 playback from any browsed SD-card directory.
- Returned playback to its originating browser screen when stopped or
  completed.
- Kept directory-first, case-insensitive sorting and safer parent navigation.

## 0.2.6 — Settings foundation

- Added persistent speaker-volume and screen-brightness preferences.
- Added live settings adjustment and a confirmed restore-defaults action.
- Routed audio playback and tone tests through the shared stored volume.
- Kept settings in internal flash so they do not depend on the SD card.

## 0.2.5.7 — Persistent audio worker hotfix

- Replaced per-track FreeRTOS task creation and deletion with one persistent
  audio worker.
- Made the worker the sole owner of active decoder teardown.
- Blocked replay until that worker has fully completed the previous track,
  following the lifecycle used by the Winamp Cardputer ADV reference.

## 0.2.5.6 — Speaker settle hotfix

- Waited for M5Unified channel 0 to acknowledge a stop before permitting
  another playback.
- Added a short I2S DMA guard interval between consecutive tracks.
- Made rapid stop-and-replay follow the same safe state reached by waiting
  manually for several seconds.

## 0.2.5.5 — Rapid replay hotfix

- Kept the speaker output and its PCM buffers alive across tracks, matching
  M5Unified's asynchronous buffer ownership model.
- Removed repeated speaker/I2S teardown during normal stop-and-replay cycles.
- Reused a stable audio output while recreating only each track's file and
  MP3 decoder.

## 0.2.5.4 — Audio replay stability hotfix

- Fully joined the M5Unified speaker worker before releasing PCM buffers.
- Ensured rapid stop-and-replay cycles start with a clean I2S speaker state.
- Prevented use-after-free corruption that could cause choppy replay or a
  device reboot.

## 0.2.5.3 — Audio stop hotfix

- Paced MP3 decoding against the speaker's asynchronous playback queue.
- Cleared the speaker channel immediately when leaving the playback screen.
- Made buffer waits cancellable so stop requests cannot be trapped behind
  queued PCM audio.

## 0.2.5.2 — Audio controls hotfix

- Moved MP3 decoding to a dedicated FreeRTOS task on the ESP32-S3's other
  core.
- Kept keyboard scanning and UI updates responsive throughout playback.
- Added task-safe stop requests so key presses are handled immediately rather
  than replayed after the track ends.

## 0.2.5.1 — Audio hotfix

- Fixed MP3 starvation and choppy playback with larger output buffers and a
  faster decoder service cadence.
- Fixed microphone metering by waiting for completed 17 kHz captures and
  measuring peak amplitude.
- Reduced microphone UI flicker by updating only the meter region.
- Prioritized keyboard polling ahead of MP3 decoding so playback stop controls
  remain responsive.

## 0.2.5 — Audio foundation

- Added speaker tone testing, live microphone monitoring, and MP3 playback
  from `/ghostwire/audio`.
- Pinned ESP8266Audio to 1.9.7 for the ESP-IDF 4.4 M5Stack toolchain.
- Initial known issues: microphone metering, choppy playback, and unresponsive
  playback controls.

## 0.2.4 — USB/HID foundation

- Added composite USB CDC/HID and confirmed text-only demonstrations.

## 0.2.3 — Infrared foundation

- Added and physically verified the GPIO44 38 kHz transmitter self-test.

## 0.2.2.1 — Radio keyboard hotfix

- Fixed navigation after blocking Wi-Fi and BLE scans.

## 0.2.2 — BLE foundation

- Added and physically verified BLE advertisement discovery and details.

## 0.2.1.1 — Wi-Fi navigation hotfix

- Added reliable return controls and post-scan keyboard recovery.

## 0.2.1 — Wi-Fi foundation

- Added and physically verified 2.4 GHz network discovery and details.

## 0.2.0 — Application foundation

- Added the menu, splash, branding, dashboard, and read-only SD browser.
- Added the `Zetascrub` creator mark.
- Confirmed display, keyboard, battery, and microSD hardware.
- Identified the GPIO5-high prerequisite for Cardputer-Adv microSD access.
