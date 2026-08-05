# Ghostwire

**A pocket network and radio scout for the M5Stack Cardputer ADV.**

Ghostwire helps an authorised operator observe nearby signals, scout a network,
notice changes, and bring useful evidence back to a full workstation. Its
persistent cyber familiar is the project's guide and memory: it reacts to what
the deck discovers without pretending the Cardputer is a replacement for a
laptop-class assessment suite.

Current development firmware: **Ghostwire 0.5.0-dev**. The firmware's
canonical version string is maintained in
[`include/branding.h`](include/branding.h).

Release history is recorded in [CHANGELOG.md](CHANGELOG.md).

Build/release instructions are in [docs/build-and-release.md](docs/build-and-release.md),
hardware assumptions in [docs/hardware-support.md](docs/hardware-support.md),
and authorized-use guidance in [docs/authorized-use.md](docs/authorized-use.md).

The ready-to-copy microSD template is in
[sd-card-files](sd-card-files/README.md). Copy its contents to the root of the
card. The repository keeps blank examples and setup instructions, while local
credentials such as `ghostwire/secrets/ai.json` remain ignored.

## Getting started

Ghostwire targets the M5Stack Cardputer ADV. Install the PlatformIO CLI, clone
this repository, and build or upload from its root:

```sh
git clone https://github.com/Zetascrub/Ghostwire.git
cd Ghostwire
pio test -e native
pio run -e cardputer_adv
pio run -e cardputer_adv --target upload
```

PlatformIO downloads the pinned toolchain and libraries into this checkout.
For download-mode instructions, release packaging, and the hardware test
checklist, see [Build and release](docs/build-and-release.md). Copy the contents
of [`sd-card-files`](sd-card-files/README.md) to the root of a microSD card when
you need scripts, audio, or AI configuration.

Use active assessment features only on equipment you own or have explicit
authorization to test. Review [Authorized use and data handling](docs/authorized-use.md)
before collecting radio, network, RFID, terminal, or location data.

## Current features

The home screen is organised around intent rather than subsystems:
**My Familiar / Observe signals / Scout network / Evidence / Field kit /
Settings**. Subsystem tools still exist, but they now sit inside the mission
where an operator would naturally look for them.

`Settings > Display & Audio > Navigation style` switches between the efficient
six-row **Compact** menus and a more characterful **Cards** interface. Cards
shows one mission at a time with a large theme-aware icon, plain-language
description, page indicators, and live operation badges. Left/Right browses and
Enter opens; Compact retains the standard Up/Down list controls. The choice is
stored across restarts and applies to the home, Observe, Field kit, Wi-Fi, BLE,
GPS, Mesh, Scout Network, Devices, Tools, and Settings category menus. Results,
telemetry, terminals, evidence, and editable preference screens retain their
denser list or dashboard layouts.

On navigation screens, the Cardputer's physical `; , . /` cluster acts as
Up/Left/Down/Right directly, without holding Fn. In text fields, passwords,
chat, and terminal sessions those keys remain normal punctuation.

Settings use the same navigation language: Up/Down selects a row and
Left/Right changes its value. Enter is reserved for actions such as previews
and confirmed resets. The **Night City 2077** theme adds a near-black,
electric-yellow, cyan, and danger-magenta palette alongside the existing
Cyberpunk theme.

Cyberdeck Idle offers three theme-aware screensavers under
`Settings > Display & Audio > Idle animation`: Data Rain, Signal Radar, and
Node Drift. Enter previews the selected style immediately and any key wakes the
deck. Idle frames are composed off-screen and transferred atomically to reduce
flicker; the temporary canvas is released on wake so active tools retain the
memory.

### Feature map

| Home mission | What it is for |
| --- | --- |
| My Familiar | Companion dashboard, Patrol, idle watch, journal and progression |
| Observe signals | Wi-Fi, BLE, GPS, mesh and combined war-drive observation |
| Scout network | Connected-network dashboard, host discovery and bounded service checks |
| Evidence | One SD-backed view of captures, logs and Patrol assessment files |
| Field kit | Connected accessories, AI field notes and supporting utilities |
| Settings | Display, audio, boot, connectivity and safe reset controls |

The detailed capabilities behind those missions are:

| Area | Highlights |
| --- | --- |
| [Wi-Fi](#wi-fi) | Discovery, channel analysis, passive PCAP, Guardian, handshake capture, connection tools |
| [BLE](#ble) | Advertisement inspection, continuous capture, HID keyboard |
| [GPS and Mesh](#gps-and-mesh) | GNSS logging, LoRa reception, Meshtastic decoding |
| [War Drive](#war-drive) | GPS-tagged Wi-Fi and BLE capture |
| [Network](#network) | Dashboard, host discovery, port scanning, Telnet, SSH |
| [Devices](#devices) | Biscuit Pro and Chameleon Ultra workflows |
| [AI Chat](#ai-chat) | OpenAI and Claude chat, voice tools, diagnostic logs |
| [Cyber Familiar](#cyber-familiar) | Persistent companion, network scout and change-aware guide |
| [Tools](#tools) | IR, USB/HID, audio, QR, IMU, files and diagnostics |
| [Settings](#settings) | Display, audio, boot experience, connectivity, themes |

### Feature details

#### Wi-Fi

  - Discovery: scan for APs (SSID, BSSID, channel, RSSI, security), CSV
    export.
  - Channel Analyzer: graph 2.4 GHz AP congestion across channels 1-13
    and recommend the least-congested non-overlapping channel (1/6/11).
  - Sniffer: passive probe-request inspection plus management-frame and full
    802.11 PCAP modes, probe CSV logging, channel hopping/lock, and live
    frame/size/drop telemetry. Management capture, channel locking, capture
    stop/save, and Wi-Fi radio restoration are hardware-validated.
  - Familiar Guardian: passive channel-hopping management-frame watch with
    Relaxed, Balanced, and Watchful thresholds for deauthentication/
    disassociation bursts. It streams alert summaries to CSV and only the
    relevant disruption frames to a compact PCAP, while surfacing conservative
    Familiar warnings. Observations indicate unusual traffic, not proof of an
    attack. Guardian requires microSD and temporarily owns the Wi-Fi radio, so
    it cannot remain associated to an access point while watching channels.
  - Single-target deauthentication from an AP's detail view, gated behind
    an explicit confirm screen.
  - Targeted WPA2 handshake/PMKID capture to `.pcap`, chainable with
    deauth to force a fresh handshake.
  - Connect: pick a scanned network, enter its password, join it. The
    connection persists in the background (explicit disconnect only) so
    other tools can use it.
  - Network Profiles: optionally retain up to five named networks, connect
    directly without rescanning, rename or delete individual entries, and
    select the default used by the opt-in auto-connect setting. Profile saving
    is disabled by default. Existing pre-0.5 single-network credentials migrate
    into the first profile automatically.

#### BLE

  - Advertisement Sniffer: passive scans with name, address/address type,
    RSSI, connectability, advertisement type, payload length, multiple service
    UUIDs, decoded manufacturer/company ID, raw manufacturer data, and enriched
    CSV export. Continuous capture adds bounded callback queuing, RSSI filters,
    stable address-based updates, raw payload logging, and drop telemetry.
    Continuous capture and subsequent Wi-Fi radio handoff are hardware-validated.
  - BLE Keyboard: explicitly started, bondable live HID keyboard. After the
    host pairs with `Ghostwire Keyboard`, printable keys plus Enter, Backspace,
    and Tab are forwarded until Escape stops and disconnects the service.
  - Spam (**experimental; shelved from further roadmap work**): Apple
    Continuity / Google Fast Pair / Microsoft Swift Pair pairing-popup spam.
    It remains available in the BLE menu and is hardware-stable, but has not
    been confirmed to trigger a real popup on any tested device.

#### GPS and Mesh

- GPS provides GNSS fix monitoring, live position/altitude/HDOP, and one-second
  CSV recording.
- Mesh provides passive SX1262 LoRa reception with Meshtastic public-channel
  header/port/plaintext decoding and event-driven CSV logging.

#### War Drive

Combined, GPS-tagged Wi-Fi AP and BLE device logging uses alternating scan
phases because the board has one radio. It remains non-blocking and displays
live unique AP and device counters.

#### Network

Network Dashboard provides live SSID/RSSI, IP, gateway, subnet, DNS, and MAC
details. Host Discovery: ICMP ping sweep of the connected subnet,
  live progress, CSV export. Port Scan: pick a found host, scan either 13
  common ports (`Enter`, seconds) or a full 1-65535 range (`Tab` menu, up
  to ~35 minutes worst case per host, 8 concurrent non-blocking connects), CSV
  export. Telnet Client: connect to any host/port (manual entry, or
  pre-filled from a Host Discovery result), plain-text interactive
  session. It is not a full terminal emulator (no ANSI/VT100 or protocol
  negotiation). SSH Client: `user@host[:port]` + password, real shell
  session over libssh with SHA-256 trust-on-first-use host-key pinning,
  changed-key rejection, password auth, and no terminal emulation (same
  plain-text limits as Telnet). These tools require an active Wi-Fi Connect
  session.

#### Devices

Dedicated control panels support external hardware. Biscuit Pro support
includes device/firmware status, bounded read-only discovery tools,
  and explicitly started AP/BLE wardrive counters with stop-on-exit handling.
  Chameleon Ultra provides BLE connection, HF14A and EM410x tag scanning,
  continuous scan mode, automatic CSV export, saved identity records, automatic
  connection retry while its screen is open, and a confirmed slot-8
  identity-emulation workflow for supported tag types. Slot selection and
  reader/emulator mode are read back from the Ultra after staging. This copies
  the tag identity/anti-collision values only; it does not claim to clone
  authenticated or protected card data.

#### AI Chat

Turn-based OpenAI or Claude chat runs over the active Wi-Fi connection. Copy
  [ai.example.json](sd-card-files/ghostwire/secrets/ai.example.json) to
  `/ghostwire/secrets/ai.json` on microSD and add either or both API keys.
  See the [AI configuration notes](sd-card-files/ghostwire/secrets/README.md)
  for the expected layout and credential precautions.
  `Tab` switches text provider, `Ctrl+R` records and transcribes a six-second
  voice prompt, `Ctrl+S` speaks the latest reply, and `Ctrl+N` clears the
  bounded in-memory conversation. Voice transcription and speech generation
  use OpenAI even when Claude is selected for text.
  Chat messages and diagnostic events are appended to
  `/ghostwire/logs/ai_chat.jsonl` on the SD card. The log rotates at 512 KiB,
  retaining one previous file, and never includes API keys. Because prompts,
  replies, and transcriptions are recorded, treat these files as private.

#### Cyber Familiar

A passive virtual companion that develops through normal device use. It
  remembers XP, level, bond, age, discoveries, learned tools, name, and idle
  preference across restarts; reacts to Wi-Fi, BLE,
  LoRa, GNSS, uplink, battery, and abnormal-reset events; keeps a short journal;
  and evolves along different paths based on how the toolkit is used. Up/Down
  changes dashboard pages; `Tab` opens a compact action menu for interactions,
  log import, record export, or confirmed reset. Wi-Fi/BLE encounters use
  persistent compact hashes, preventing normal rescans from inflating totals
  without retaining raw device identifiers. Counter growth, reboot persistence,
  and duplicate-resistant rescanning are hardware-validated.

Familiar Patrol adds an explicitly confirmed, unattended assessment workflow
for the connected subnet. It streams ICMP-responsive hosts and TCP observations
to a dedicated microSD session, then checks 100 prioritized TCP ports on every
responsive host. The bounded scout pass covers common web, remote-access,
file-sharing, management, database, messaging, container, printer, and IoT
services without attempting an automatic 1-65535 scan. Only the current
operation is kept in RAM. The patrol pauses across Wi-Fi loss, refuses to resume
on a different subnet, checkpoints progress for reboot recovery, runs in the
background, and produces CSV, JSON Lines, and Markdown report files. It performs
discovery and port assessment only; it does not guess credentials, exploit
services, or extract data.

The Familiar uses procedural pixel animation for breathing, blinking, ear/tail
movement, and discovery radar. Host and service discoveries trigger short
expressive reactions; ports commonly associated with remote administration,
file sharing, databases, or exposed infrastructure trigger a warning reaction.
Timed speech bubbles identify the endpoint using a compact final-octet label
such as `.134`, and name recognised service types. These are investigation
leads rather than vulnerability claims. `Settings > Display & Audio > Familiar
cues` selects No sound, Subtle, Chirps, Arcade, or Mystic event tones. Cues are
rate-limited and do not interrupt active audio playback.

The Familiar action menu includes **Start idle watch now**, which enters the
animated idle display immediately; any key wakes it. Animated dashboard and
idle refreshes invalidate only their moving region, leaving static UI elements
in place to reduce whole-screen flicker.

Patrol confirmation can select a one-shot scout or **Continuous Watch**, with
1, 5, 15, 30, or 60 minute intervals. After the first pass, the watch repeats
host discovery but filters addresses already recorded in the SD-backed session
baseline. Only newly observed addresses receive the 100-port scout pass and
trigger new-host reactions. The patrol screen shows its cycle and countdown.

An experimental **Familiar Phrase Lab** under Audio composes representative
event phrases from individual MP3 word clips stored in
`/ghostwire/audio/Familiar/`. It reports total sequence time and remains
deliberately isolated from Familiar events while pacing and intelligibility are
evaluated. The supplied SD-card template includes phrases such as New host
discovered, Interesting service found, and Patrol completed.

Selecting **Voice pack** under Settings > Display & Audio connects this word
bank to patrol start, new-host, service, warning, completion, and error events.
Announcements play asynchronously with rate limiting and a one-item priority
queue, allowing warnings to replace stale routine chatter. The patrol view also
shows the Familiar's current mood, and repeat watches react differently to a
quiet cycle and one containing new hosts.

#### Tools

  - Infrared: onboard 38 kHz transmitter self-test.
  - USB/HID: composite USB serial, confirmed text-only keyboard demos, and a
    guarded microSD DuckyScript runner. Put `.txt` or `.duck` scripts in
    `/ghostwire/scripts`; the [script template](sd-card-files/ghostwire/scripts/README.md)
    lists the supported commands and execution limits.
  - Audio: speaker tone test, live microphone level, and MP3 playback from
    `/ghostwire/audio`. Format guidance is available in the
    [SD audio template](sd-card-files/ghostwire/audio/README.md).
  - QR Generator: create a scannable QR code entirely offline from up to 100
    characters of typed text, a URL, or a short note.
  - Logs/Sessions: browsing, metadata, preview, confirmed deletion of
    recorded CSV sessions.
  - Motion/IMU: live accelerometer/gyroscope axes, orientation, motion
    state, stationary calibration, CSV recording.
  - Files: read-only microSD browser with CSV/TXT/LOG preview and direct
    MP3 playback.
  - System: diagnostics dashboard, SD health-report export, persistent
    boot/reset telemetry, and system-clock sync from GNSS or NTP.
  - About.

#### Settings

Grouped Display & Audio, Boot Experience, and Connectivity submenus contain
persistent options for volume, brightness, screen timeout, eight boot animation
styles, five boot sound styles with on-device previews, Slow/Normal/Fast boot
speed, network-profile saving, boot auto-connect, the Cyberdeck idle mode,
themes, and restoring defaults. A seven-page first-run field guide introduces
Observe → Scout → Record, the Familiar, SD evidence, navigation style, and
network profiles; it can be skipped and replayed later from Settings.

## Controls

The current navigation shortcuts are implemented centrally in `handleInput()`
in [`src/main.cpp`](src/main.cpp):

- Move with the arrow keys. `W`/`S`, `K`/`J`, and `;`/`.` are equivalent
  up/down shortcuts on normal navigation screens.
- Select with `Enter`.
- Return with `Escape`, `Backspace`, Left, `Q`, or `B`. Individual text-entry
  and live terminal screens reserve some of these keys for editing or remote
  input and show their available exit key in the footer.
- Press `R` to refresh, restart, or start/stop the current operation where the
  screen footer offers it.
- Press `Tab` to open the contextual action menu where one is available. Use
  the normal movement keys and `Enter` inside it; `Tab` or any back key closes
  it.
- In AI Chat, `Tab` switches text provider, `Ctrl+R` records a six-second voice
  prompt, `Ctrl+S` speaks the latest reply, and `Ctrl+N` clears the in-memory
  conversation.
- In a live BLE Keyboard session, printable keys, `Enter`, `Backspace`, and
  `Tab` are forwarded to the paired host; `Escape` stops the service.

Press `Ctrl` + `Alt` + `Backspace` at any time for the global emergency stop.
It stops active radio operations, sockets, playback and logging, disconnects
Wi-Fi, and returns to the main menu.

## Roadmap

The organised roadmap, selection criteria, acceptance gates, and deliberately
shelved ideas are maintained in [docs/roadmap.md](docs/roadmap.md).

The first workflow batch is now implemented and hardware-validated:

1. passive Wi-Fi management/full PCAP capture;
2. continuous BLE capture and Wi-Fi/BLE handoff;
3. Cyber Familiar encounter integrity, persistence, import, and export;
4. Chameleon Ultra save/load and identity-emulation workflow.

The next candidate group is:

1. TCP/UDP socket workbench, time management, BLE scripts, QR utilities, and
   microphone spectrum diagnostics;
2. ESP-NOW exchange, a security-gated Web UI, WiGLE-compatible export,
   WireGuard evaluation, and optional accessory support.

Jammers, broad floods, novelty spam, combined evil-portal/deauth workflows,
Responder/ARP poisoning, and unsupported-hardware features are not active
roadmap work.

USB-C-to-Ethernet client connect was investigated and ruled out: this
board's USB-C port is hardwired device-only (5.1kΩ CC pull-downs, no
VBUS-out capability), and Arduino-ESP32 has no USB Host Ethernet driver
either. See `CHANGELOG.md`'s 0.3.5.1 entry.

## Foundation history: Ghostwire 0.2.x

The 0.2.x series established the keyboard-driven foundation for the current
authorized security field toolkit. It includes:

- A centralized product name, version, colour palette, and creator credit in
  `include/branding.h`.
- Wi-Fi discovery with signal, channel, security, and BSSID details.
- BLE discovery with advertised name, address, RSSI, and service UUID details.
- Onboard 38 kHz infrared transmitter self-test on GPIO44.
- Composite USB serial/HID support with explicitly confirmed, text-only
  keyboard demonstrations.
- Speaker tone test, live microphone level, and MP3 playback from
  `/ghostwire/audio`.
- A read-only microSD browser with details and direct MP3 playback.
- Persistent volume, brightness, and screen-timeout settings.
- Persistent boot-ready sound setting.
- Persistent Slow, Normal, and Fast boot-speed settings scale the complete
  animation, title reveal, summary, and hold sequence.
- Battery percentage, charge status, and low-battery indication.
- GNSS fix monitoring and passive SX1262 LoRa/Meshtastic packet reception.
- Live accelerometer/gyroscope data, orientation, motion state, and stationary
  gyro calibration.
- Reusable SD CSV logging, initially exposed as a 10 Hz IMU recorder under
  `/ghostwire/logs`.
- Read-only, scrollable previews for CSV, TXT, and LOG files.
- One-second GNSS CSV recording with fix quality and position data.
- Event-driven passive LoRa packet logging with radio metadata and payload
  previews.
- GNSS-synchronized UTC system time and ISO-8601 timestamps in new logs.
- Receive-only Meshtastic public-channel header, port, and plaintext message
  decoding.
- One-shot CSV export of Wi-Fi and BLE discovery results.
- Dedicated log-session browsing, metadata, preview, and confirmed deletion.
- Persistent boot/recovery counters and append-only SD startup history.
- Live terminal-style boot diagnostics with a Ghostwire/Zetascrub title card.
- A system dashboard and placeholders for future tool groups.

The 0.2.x series brings up one subsystem at a time:

- 0.2.1: Wi-Fi discovery and navigation reliability
- 0.2.2: BLE discovery and Wi-Fi/BLE radio handoff
- 0.2.3: onboard infrared transmitter self-test
- 0.2.4: USB enumeration and harmless HID keyboard demonstrations
- 0.2.5: speaker, microphone, and SD-based MP3 playback
- 0.2.6: persistent settings
- 0.2.7: SD browser and direct file playback
- 0.2.8: battery monitoring and display power management
- 0.2.9: GNSS receive and fix monitoring
- 0.2.10: passive LoRa and Meshtastic-profile reception
- 0.2.11: reserved for read-only Meshtastic public-channel decoding
- 0.2.12: IMU detection, live axes, orientation, and calibration
- 0.2.13: reusable SD logging and IMU CSV recording
- 0.2.14: GNSS-synchronized UTC system clock and timestamped logs
- 0.2.15: Wi-Fi and BLE discovery exports
- 0.2.16: logging session manager
- 0.2.17: system diagnostics and SD health-report export
- 0.2.18: persistent reset telemetry and SD boot history
- 0.2.19: performance, memory, resilience, and user-experience polish

## Ghostwire 0.3.x (current development series)

The 0.3.x series adds explicitly authorized Wi-Fi/BLE/RFID assessment tools
on top of the 0.2.x foundation, one small slice at a time.

**0.3.1.x: Wi-Fi (complete):**

- 0.3.1.1: Wi-Fi Sniffer provides passive 802.11 probe-request sniffing (device
  MAC, requested SSID, RSSI, channel), hopping channels 1/6/11, with CSV
  logging to `/ghostwire/logs`. Read-only; no frame injection yet.
- 0.3.1.2: single-target deauthentication from the Wi-Fi Discovery AP
  detail view, gated behind an explicit confirm screen before anything is
  sent.
- 0.3.1.3: targeted WPA2 handshake/PMKID capture to `.pcap`, chained with
  0.3.1.2's deauth to force a fresh handshake without leaving the screen.

**0.3.2.x: BLE (in progress):**

- 0.3.2.1: migrated BLE Discovery to NimBLE-Arduino, the prerequisite BLE
  stack for every planned BLE feature and the Chameleon Ultra RFID pairing
  client. No user-visible change.
- 0.3.2.2: reorganized the main menu into six categories: **Wi-Fi**, **BLE**,
  **GPS**, **Mesh**, **Tools**, and **Settings**, instead of a flat 13-item
  list, ahead of BLE Spam/Sniffer/HID and Chameleon Ultra RFID pairing
  adding more items.
- 0.3.2.3: BLE Spam includes Apple Continuity, Google Fast Pair, and Microsoft
  Swift Pair pairing-popup spam, cycling advertisements with a randomized
  MAC every 300 ms. Samsung Easy Setup spam deferred (no reliable
  independent protocol spec found yet). **Shelved**: hardware-stable (a
  real reboot-on-stop crash was found and fixed), but not confirmed to
  produce a real pairing popup on any tested device yet. See
  `CHANGELOG.md` for detail.

**0.3.3.x: RFID (hardware-validated identity workflow):**

- 0.3.3.1: new top-level **RFID** menu category with a **Chameleon Ultra**
  item. It connects over BLE and reads firmware version + battery, proving
  out BLE-central connectivity to the user's own Chameleon Ultra (this
  board has no onboard RFID chip).
- 0.3.3.2: Chameleon Ultra card read scans for an HF14A (ISO14443A) tag,
  falling back to an EM410x (125 kHz) tag if none found, and shows the
  UID/ATQA/SAK or EM410x ID.
- 0.3.3.3: Chameleon Ultra continuous scan (`C` key, ~500ms polling) and
  automatic CSV export of every distinct tag capture to
  `/ghostwire/logs`, with no manual log toggle needed.
- 0.3.3.4: persistent saved identity records plus confirmed slot-8 staging for
  EM410x and recognised MIFARE Classic Mini/1K/4K identities. The client retries
  BLE connection automatically, reads back the selected slot and operating
  mode, and can explicitly return the Ultra to reader mode. Identity emulation
  does not copy authenticated/protected card contents.

**0.3.4.x: War Driving (in progress):**

- 0.3.4.2: new top-level **War Drive** menu category with combined,
  GPS-tagged Wi-Fi AP and BLE device logging, alternating scan phases
  since the board has one radio. Fully non-blocking (`WarDriveService`),
  so the screen shows live "Unique APs"/"Unique devices" counters without
  ever hijacking the UI, and Backspace/Q works instantly. Logs every
  result every phase (no dedup) to WiGLE 1.6-compatible
  `wardrive_wigle` and detailed `wardrive_ble` CSVs.

**0.3.5.x: Client connect (in progress):**

- 0.3.5.1: Wi-Fi **Connect** lets you pick a scanned network, enter its password
  (masked, first free-text entry screen in this codebase), connect, see
  IP/gateway/RSSI. Connection persists in the background when leaving the
  screen (explicit `D` to disconnect) so future network tools can use it.
  USB-C-to-Ethernet was investigated and ruled out: the board's USB-C
  port is hardwired device-only (CC pull-downs, no VBUS-out), and
  Arduino-ESP32 has no USB Host Ethernet driver either.
- 0.3.5.2: connected-status dot in the header (next to the clock/battery)
  whenever Wi-Fi Connect is active, visible from any screen.

**0.3.6.x: Network tools (in progress):**

- 0.3.6.1: new top-level **Network** category with Host Discovery, an ICMP
  ping sweep of the connected subnet using ESP-IDF's raw lwIP ping API
  (`esp_ping_new_session`/`esp_ping_start`), fully non-blocking, CSV
  export.
- 0.3.6.2: Port Scan lets you pick a host found by Host Discovery and scan 13
  common ports via `WiFiClient::connect()`, CSV export.
- 0.3.6.3: full 1-65535 port scan option, 8 concurrent non-blocking
  connects via raw `lwip/sockets.h` calls (this board caps at 16 total
  sockets system-wide).

**0.3.7.x: UI overhaul (in progress):**

- 0.3.7.1: contextual action menu. `Tab` opens a per-screen menu of
  whatever one-off actions are available (export, deauth, disconnect,
  continuous-scan toggle, etc.), replacing individually memorized
  letters that had grown to 9 distinct keys across ~15 screens.
  Migrated the most letter-heavy screens first (Wi-Fi Discovery/Detail/
  Handshake Capture/Connect, BLE Discovery, Chameleon Ultra, Network
  Host Discovery/Port Scan, System Diagnostics/Clock).
- 0.3.7.2: fixed the action menu disappearing on screens that redraw
  themselves on a timer (Chameleon continuous scan, Network Host/Port
  Scan, War Drive, Wi-Fi Connect status, GNSS/LoRa/Sniffer/IMU live
  readouts, System Clock). Those periodic redraws now skip themselves
  while the menu is open instead of drawing over it. Also finished the
  letter-shortcut sweep: GNSS/LoRa/Wi-Fi Sniffer/IMU log toggles, LoRa
  profile switch, IMU calibrate, and Log Session delete all moved into
  the action menu. Text Preview's column-pan keys stay direct, since
  they're repeated navigation, not one-off actions.

**0.3.8.x: Theming:**

- 0.3.8.1: selectable UI themes include Matrix (default), Cyberpunk, Windows,
  Amber Terminal. Every screen already drew through six shared colours in
  `include/branding.h`, so this made them runtime-switchable
  (`Branding::applyTheme()`) instead of adding any per-screen code. New
  "Theme" row in Settings (`-`/`=` to cycle, same pattern as screen
  timeout), instant live preview, persisted, and applied before the boot
  sequence runs so a saved theme shows on the boot screen too.
- 0.3.8.2: "current/total" position readout in the header on every
  screen with a genuinely scrollable list (Main Menu, Wi-Fi Discovery/
  Connect, BLE Discovery, Network Host Discovery, MP3 Files, file
  browser, Log Sessions, System Diagnostics, Tools, Settings). This was found
  after Settings' new Theme row exposed a real bug where a 7th item
  scrolled with no visual feedback at all.

**0.3.9.x: More themes:**

- 0.3.9.1: three more themes: Space, Pastel (built as a dark background
  with soft pastel accents rather than a literal light theme, after a
  light attempt's contrast math came out too poor to be legible), and a
  joint Frog and Duck theme with a ribbit-then-quack boot sound
  (`Branding::Theme` gained two optional MP3-path fields so any theme
  can opt into a custom boot sound), falling back to the normal chime if
  the sound files aren't on the SD card.
- 0.3.9.2: split Frog and Duck into two separate themes. **Frog** has a
  green palette and ribbit boot sound. **Duck** has a blue-teal palette and
  quack boot sound. This was based on user feedback after testing 0.3.9.1.

**0.3.10.x: Network tools, round 2:**

- 0.3.10.1: Telnet Client connects to any host/port (manual entry from
  the Network menu, or pre-filled from a Host Discovery result's action
  menu), single bounded blocking connect (`WiFiClient`, 5s timeout) then
  non-blocking send/receive. Deliberately not a full terminal emulator
  (no ANSI/VT100 and no telnet option negotiation), with plain-text in and out,
  tested against a `nc -l <port>` listener since no real telnet server
  was available. Every key on the live session screen is forwarded to
  the remote host, including `Tab` (remote shell completion). `Esc` is
  the only way to disconnect.
- 0.3.11.1: SSH Client supports `user@host[:port]` + password and a real
  interactive shell over `LibSSH-ESP32`. Confirmed the library actually builds
  against this project's exact toolchain with a throwaway spike test
  before designing anything (it has a known, unresolved upstream
  PlatformIO compile issue that turned out not to reproduce here). Every
  libssh call up to opening the shell is bounded and blocking; session I/O is
  then polled non-blockingly through `SshService`.
  Same deliberate non-goals as Telnet (no terminal emulation), plus two
  more specific to SSH: trust-on-first-use SHA-256 host-key pinning and
  password auth only (no public-key auth).

Next candidates: TCP/UDP socket workbench, offline time controls, BLE
DuckyScript transport, QR presets, and microphone spectrum diagnostics. See the
roadmap for acceptance gates and deliberately shelved ideas.

## License

Ghostwire is released under the [MIT License](LICENSE).
