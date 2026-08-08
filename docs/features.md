# Feature details

This is the full, detailed feature reference. For a quick overview, see the
[feature summary](../README.md#what-it-can-do) in the README.

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

## Feature map

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

## Feature details

### Wi-Fi

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

### BLE

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

### GPS and Mesh

- GPS provides GNSS fix monitoring, live position/altitude/HDOP, and one-second
  CSV recording.
- Mesh provides a lightweight Meshtastic field client over the SX1262. Its
  app-style home presents Chats, Nodes, Map, and Settings instead of exposing
  radio utilities as the primary workflow. Chats groups the bounded 32-message
  working set into persistent channel and direct conversations, adds unread
  counts, timestamps, and direct-message delivery state, and decodes
  public-channel identities and positions, suppresses repeated messages, and
  retains raw radio metadata in event-driven CSV logs. Node and message detail
  views remain available while the radio continues listening. A GNSS-relative
  radar plots received positions and node details calculate range/bearing;
  device battery telemetry is shown when broadcast, with a small per-node trend
  history. Node actions can request fresh identity, position, or telemetry, and
  Mesh Settings can broadcast the Cardputer's current GNSS position. Peer keys
  are classified as identity-bound, legacy, unknown, or changed; a changed key
  is blocked until explicitly accepted on the node page. The bounded client
  state is restored from `/ghostwire/mesh/state.json` after reboot. Each message
  and later delivery update is also appended to
  `/ghostwire/mesh/messages.jsonl`, keeping long-term history on the SD card
  rather than in RAM. Entering a chat marks it read, shows its recent thread,
  and Enter composes in that channel or direct context.
  Ghostwire originates
  packets as a non-repeating `CLIENT_MUTE`-style endpoint, with a persistent
  adjustable hop limit of 1-7 (default 7), channel-activity checks, and an
  airtime guard; it does not route or rebroadcast other nodes' traffic.
  Mesh Settings provides persistent 24-character long and four-character
  short names, default-channel selection, hop-limit adjustment, key exchange,
  and access to channel profiles and the lower-level radio status screen. The
  page also makes the fixed
  `CLIENT_MUTE` role and EU_868 region visible. Optional background-client mode
  starts the SX1262 at boot and continues receiving away from Mesh screens;
  when disabled, the radio stops after leaving Mesh. A separate persistent
  message-alert toggle plays a short two-note cue for each newly decoded text
  message without interrupting active audio.
  Direct conversations can be opened from Chats or started from a node detail
  page. Direct messages use Meshtastic-compatible Curve25519/AES-CCM PKI and
  request a mesh acknowledgement. Pending, delivered, failed, and timed-out
  states are shown in the chat UI; they are not sent as legacy channel-encrypted
  DMs. Both peers must first learn one another's public key: let Ghostwire hear
  the peer's NodeInfo. Mesh Settings -> Advertise identity sends Ghostwire's
  persistent signed identity and requests NodeInfo replies from listening peers,
  completing both sides of the key exchange. Node detail reports when a peer's
  key is ready; pressing Enter on a node without a key requests identity
  exchange automatically.
- Channel Profiles always retains the public `LongFast` decoder and supports up
  to three private receive/transmit channels. They can be added and removed on
  the device using a `Name|Base64PSK` token, exported as a QR code for convenient
  transfer, and are stored in `/ghostwire/mesh/channels.json`. Profiles require
  the exact Meshtastic name
  and a Base64 AES-128/AES-256 PSK; malformed entries are rejected without
  exposing their key material. The Cap LoRa-1262 remains fixed to the EU_868
  LongFast carrier at 869.525 MHz.

### War Drive

Combined, GPS-tagged Wi-Fi AP and BLE device logging uses alternating scan
phases because the board has one radio. It remains non-blocking and displays
live unique AP and device counters.

### Network

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

### Devices

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

### AI Chat

Turn-based OpenAI or Claude chat runs over the active Wi-Fi connection. Copy
  [ai.example.json](../sd-card-files/ghostwire/secrets/ai.example.json) to
  `/ghostwire/secrets/ai.json` on microSD and add either or both API keys.
  See the [AI configuration notes](../sd-card-files/ghostwire/secrets/README.md)
  for the expected layout and credential precautions.
  `Tab` switches text provider, `Ctrl+R` records and transcribes a six-second
  voice prompt, `Ctrl+S` speaks the latest reply, and `Ctrl+N` clears the
  bounded in-memory conversation. Voice transcription and speech generation
  use OpenAI even when Claude is selected for text.
  Chat messages and diagnostic events are appended to
  `/ghostwire/logs/ai_chat.jsonl` on the SD card. The log rotates at 512 KiB,
  retaining one previous file, and never includes API keys. Because prompts,
  replies, and transcriptions are recorded, treat these files as private.

### Cyber Familiar

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

### Tools

  - Infrared: onboard 38 kHz transmitter self-test.
  - USB/HID: composite USB serial, confirmed text-only keyboard demos, and a
    guarded microSD DuckyScript runner. Put `.txt` or `.duck` scripts in
    `/ghostwire/scripts`; the [script template](../sd-card-files/ghostwire/scripts/README.md)
    lists the supported commands and execution limits.
  - Audio: speaker tone test, live microphone level, and MP3 playback from
    `/ghostwire/audio`. Format guidance is available in the
    [SD audio template](../sd-card-files/ghostwire/audio/README.md).
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

### Settings

Grouped Display & Audio, Boot Experience, and Connectivity submenus contain
persistent options for volume, brightness, screen timeout, eight boot animation
styles, five boot sound styles with on-device previews, Slow/Normal/Fast boot
speed, network-profile saving, boot auto-connect, the Cyberdeck idle mode,
themes, and restoring defaults. A seven-page first-run field guide introduces
Observe → Scout → Record, the Familiar, SD evidence, navigation style, and
network profiles; it can be skipped and replayed later from Settings.

Familiar LED lets the onboard RGB LED flash a per-event colour: Started, Host
found, Service found, Warning, Complete, and Error each cycle independently
through a shared named palette (including Off, to silence just one event), with
an immediate preview flash while adjusting. Off by default. It shares the same
Familiar Patrol and Wi-Fi Guardian trigger points as the audio cues but is
configured and toggled separately from them.
