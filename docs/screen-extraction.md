# Extracting screens out of src/main.cpp

`src/main.cpp` currently holds the draw/input logic for every screen in one
~9,000-line file, inside a single anonymous namespace. Every other subsystem
(Wi-Fi, BLE, GNSS, SSH, etc.) already lives in its own `include/x_service.h` +
`src/x_service.cpp`. This doc describes the pattern used to bring screens in
line with that, one at a time, per the roadmap's "gradual screen/controller
extraction from `src/main.cpp`" goal.

## Why gradual, not a rewrite

main.cpp's ~100+ globals (`listSelection`, `sdAvailable`, per-screen state,
...) are read and written across most of the file. Extracting everything at
once means touching all of it under one unreviewable diff. Extracting one
screen at a time keeps each change small, independently buildable, and
revertable, and lets the pattern get corrected after the first couple of
screens instead of being locked in early.

## The pattern

1. **New module per screen (or small screen family)**: `include/x_screen.h` +
   `src/x_screen.cpp`, following the same shape as the existing services — a
   class holding whatever state is specific to that screen, with a `draw()`
   and the input-handling entry points the screen needs (see
   `include/ir_screen.h` for the smallest possible example: a stateless
   screen that just displays and drives an existing service).

2. **Shared chrome via `include/screen_chrome.h`**: extracted screens need
   `drawHeader`/`drawFooter`/etc., which are defined in main.cpp's anonymous
   namespace and so have internal linkage — invisible to another translation
   unit. Rather than move them (they reach deep into main.cpp's global
   service instances via `drawHeaderStatus`), `screen_chrome.h` declares a
   small external-linkage surface, and main.cpp provides thin forwarders
   right after its anonymous namespace closes:

   ```cpp
   }  // namespace

   namespace ScreenChrome {
   void drawHeader(const char* title) { ::drawHeader(title); }
   void drawFooter(const char* text) { ::drawFooter(text); }
   }  // namespace ScreenChrome
   ```

   Add to `screen_chrome.h` only when an extracted screen actually needs a
   new primitive (e.g. `drawListRow`/`normalizeListPosition` once a screen
   with a list is extracted) — keep the surface hand-picked, not a dump of
   everything main.cpp has.

   This is a deliberate stopgap, not the end state: once enough screens are
   extracted that main.cpp's remaining share of the file shrinks
   substantially, revisit moving `drawHeader`/`drawFooter`/`drawHeaderStatus`
   and the service globals they read into their own module for real, instead
   of forwarding into main.cpp's anonymous namespace.

3. **Wire it into main.cpp**: instantiate the screen next to the service
   it wraps (e.g. `IrScreen irScreen(irService);` right after
   `IrService irService;`), delete the old free functions, and repoint the
   two call sites — the draw dispatch switch and the input-handling switch
   — at the new instance's methods.

4. **Verify**: `pio run -e cardputer_adv` should succeed with an unchanged
   RAM/Flash footprint (a pure refactor moves code, it doesn't add any), and
   `pio test -e native` should still pass.

## Picking the next screen

Start with screens that touch little shared state — a good proxy is how much
of `listSelection`/`listOffset` and other cross-screen globals the screen's
draw/input functions read. `Screen::Infrared` had none (see
`include/ir_screen.h`/`src/ir_screen.cpp`, the first extraction). Screens that
only wrap a single existing service and a small confirm/result flow (e.g.
`Screen::BleSpamSelect`/`Screen::BleSpam`, `Screen::Gnss`) are reasonable next
candidates; menu screens with shared list-cursor state are better done after
`drawListRow`/`normalizeListPosition` are exposed through `screen_chrome.h`.

## Status

- [x] `Screen::Infrared` — `include/ir_screen.h`, `src/ir_screen.cpp`.
- [x] `Screen::Gnss` — `include/gnss_screen.h`, `src/gnss_screen.cpp`. Draw
      only (full + partial-redraw); input handling and the once-a-second
      background CSV append stay in main.cpp since the append isn't gated on
      the screen being open. Added `ScreenChrome::beginContentUpdate` for the
      partial-redraw path.
- [x] `Screen::BleSpamSelect` / `Screen::BleSpam` — `include/ble_spam_screen.h`,
      `src/ble_spam_screen.cpp`. First extraction touching the shared list
      cursor: main.cpp still owns `listSelection`/`listOffset`/
      `normalizeListPosition`/`moveSelection` and passes the current
      selection in to draw; the screen owns the mode-index-to-`BleSpamMode`
      mapping as a single source of truth (previously duplicated between the
      select screen's item list and the input handler's switch). Added
      `ScreenChrome::drawListRow`.
- [x] `Screen::LoRa` — `include/lora_screen.h`, `src/lora_screen.cpp`. Same
      draw-only shape as GnssScreen, including the partial-redraw
      dedup-by-signature check (moved from a function-static to a member).
- [x] `Screen::WifiGuardian` — `include/wifi_guardian_screen.h`,
      `src/wifi_guardian_screen.cpp`. Takes references to two services, a
      logger, and a shared `String` (`guardianLastEvent`, written from
      several places outside this screen) -- draw-only extractions can take
      whatever set of references the screen displays, it doesn't have to be
      exactly one service.
- [x] `Screen::Imu` — `include/imu_screen.h`, `src/imu_screen.cpp`. Draw-only,
      like Gnss/LoRa, but the state it displays isn't a service object --
      main.cpp keeps the last IMU sample, calibration offsets, and
      availability/calibrating flags as separate globals. Took a reference to
      each rather than bundling them into a new struct (that would mean also
      touching `updateImu()`/`beginImuCalibration()`, past this step's
      scope). `imuTypeName()` moved into the screen wholesale since it was
      only ever called from the screen.
- [x] `Screen::WifiSniffer` — `include/wifi_sniffer_screen.h`,
      `src/wifi_sniffer_screen.cpp`. Draw-only; takes the service, the PCAP
      capture logger, and the recent-probes vector by reference. Capture
      mode cycling, channel lock, and log start/stop stay in main.cpp.
- [x] `Screen::MainMenu`, `ObserveMenu`, `FieldKitMenu`, `WifiMenu`, `BleMenu`,
      `DevicesMenu`, `RfidMenu`, `ToolsMenu`, `GpsMenu`, `MeshMenu`,
      `NetworkMenu` — `include/menu_screens.h`, `src/menu_screens.cpp`
      (`MenuScreens`). Grouped as one "small screen family" module per this
      doc's own guidance, since none of them carry state of their own: they
      only read the shared list cursor, the Cards/Compact toggle, and a
      handful of services for status badges. Added
      `ScreenChrome::normalizeListPosition`, `drawHeaderPosition`,
      `drawNavigationCard`, and `kVisibleRows` -- the remaining chrome
      primitives basically every menu/list screen needs.
- [x] `Screen::WifiRecon`, `WifiChannelAnalyzer`, `WifiDetail`,
      `WifiDeauthConfirm`, `WifiHandshakeCapture`, `WifiConnectSelect`,
      `WifiConnectPassword`, `WifiConnectStatus` — `include/wifi_screens.h`,
      `src/wifi_screens.cpp` (`WifiScreens`). Largest dependency set so far
      (scan results, list cursor, handshake capture state, connect state);
      a few methods redirect back to `drawRecon()`/`Screen::WifiRecon` when
      the AP selection goes stale, preserved as same-class sibling calls.
      `authName()` centralized as `WifiScreens::authName` (previously
      duplicated risk across 3 call sites). Added
      `ScreenChrome::drawTextEntryRow`.
- [x] `Screen::BleDiscovery`, `BleDetail`, `BleKeyboard` —
      `include/ble_screens.h`, `src/ble_screens.cpp` (`BleScreens`). Same
      shape as WifiScreens (scan results + stale-selection redirect) plus a
      LoRaScreen-style signature-dedup'd keyboard status screen.
- [x] `Screen::Biscuit`, `BiscuitTools`, `BiscuitResult`, `BiscuitWardrive`,
      `Chameleon`, `ChameleonEmulateConfirm` — `include/device_screens.h`,
      `src/device_screens.cpp` (`BiscuitScreens` + `ChameleonScreen`). Two
      unrelated subsystems in one file pair purely to hold file count down;
      no shared state between the two classes. `chameleonHexId()`
      centralized as `ChameleonScreen::hexId` (was called from several
      main.cpp sites beyond the screen itself).
- [x] `Screen::UsbHid`, `UsbHidConfirm`, `DuckyScripts`, `DuckyConfirm`,
      `DuckyResult` — `include/usb_hid_screens.h`, `src/usb_hid_screens.cpp`
      (`UsbHidScreens`).
- [x] `Screen::Audio`, `TtsLab`, `AudioMic`, `AudioFiles` (+ the shared "Now
      Playing" screen used from three different playback start points,
      which turned out to render different text at each call site --
      preserved exactly rather than assumed-identical) —
      `include/audio_screens.h`, `src/audio_screens.cpp` (`AudioScreens`).
- [x] `Screen::QrEntry`, `QrDisplay` — `include/qr_screens.h`,
      `src/qr_screens.cpp` (`QrScreens`).
- [x] Added `include/hid_presets.h` and `include/familiar_phrases.h`:
      small header-only `inline constexpr` constant tables that were
      previously main.cpp globals needed by both main.cpp (business logic)
      and a new screen module -- cheaper than a forwarder for compile-time
      data.
- [x] `Screen::Files`, `FileDetail`, `TextPreview`, `LogSessions`,
      `LogDetail`, `LogDeleteConfirm` — `include/file_screens.h`,
      `src/file_screens.cpp` (`FileScreens` + `LogScreens`). Moved the
      `FileEntry`/`LogEntry` structs into the header too (needed by both
      main.cpp's loading code and the screens). First screens with their
      *own* separate list cursor (`logSelection`/`logOffset`, distinct from
      the shared `listSelection`/`listOffset`) -- kept that distinction
      rather than unifying it as part of this extraction.
- [x] `Screen::System`, `TimeStatus` — `include/system_screens.h`,
      `src/system_screens.cpp` (`SystemScreens`). `systemDiagnostics()`
      (~22-row aggregator touching most subsystems) stays in main.cpp and is
      passed into `drawSystem()` as a snapshot rather than called from the
      screen -- same reasoning as elsewhere: keeps the screen from needing a
      reference to everything the diagnostics list touches. `DiagnosticState`/
      `SystemDiagnostic` moved into the header (both main.cpp and the screen
      need the type). `drawTimeReadouts()`'s UTC formatting is duplicated
      from main.cpp's `utcTimestamp()` (used by a dozen+ CSV loggers outside
      this screen) rather than exposed across the translation-unit boundary.
- [x] `Screen::Settings`, `SettingsDisplay`, `SettingsBoot`,
      `SettingsConnectivity`, `SettingsReset`, `Placeholder`, `About` — the
      root menu joined `MenuScreens::drawSettings` (it's a plain list menu,
      no unique state); the rest are `include/settings_screens.h`,
      `src/settings_screens.cpp` (`SettingsScreens`). Added
      `include/settings_names.h` for the boot/display preference label
      tables (needed by both main.cpp's adjust-value input handling and this
      screen).
- [x] `Screen::WarDrive`, `NetworkDashboard`, `NetworkHostScan`,
      `NetworkPortScan` — `include/network_scan_screens.h`,
      `src/network_scan_screens.cpp` (`NetworkScanScreens`).
- [x] `Screen::TelnetConnect`, `TelnetSession` — `include/telnet_screens.h`,
      `src/telnet_screens.cpp` (`TelnetScreens`).
- [x] `Screen::SshConnect`, `SshPassword`, `SshSession` —
      `include/ssh_screens.h`, `src/ssh_screens.cpp` (`SshScreens`). The
      riskiest batch on paper (live terminal sessions) turned out
      mechanically identical to the others once isolated: each session
      screen already split into a `*Dynamic()` partial-redraw half (called
      on every incoming byte) and a full-draw wrapper, same shape as every
      other partial-redraw screen already extracted. Byte-level terminal
      handling, connecting, and host-key trust all stayed in main.cpp.
- [x] `Screen::AiChat` — `include/familiar_screens.h`,
      `src/familiar_screens.cpp` (`AiChatScreen`).
- [x] `Screen::CyberFamiliar`, `CyberFamiliarResetConfirm`, `FamiliarPatrol`,
      `FamiliarPatrolConfirm` — same file pair (`FamiliarScreens`). The
      procedural creature/speech-bubble drawing (`drawCreature()`/
      `drawSpeechBubble()`) is also called from main.cpp's
      `drawCyberFamiliarIdle()` (the ambient idle-screen animation, outside
      the normal `Screen` enum) -- kept public and called from there rather
      than duplicated. `FamiliarReaction` and the patrol interval table
      moved into the header (both main.cpp and this module need them).

## Status: every `Screen` enum entry extracted

As of this pass, every `case Screen::X` in main.cpp's dispatch switch calls
a method on an extracted screen object -- none call a free function still
defined in main.cpp. `src/main.cpp` went from 9,120 to 6,913 lines (-24%)
across ~25 screens and 21 new module file pairs. What legitimately remains
in main.cpp under `void draw*`:

- The shared chrome primitives themselves (`drawHeader`, `drawFooter`,
  `drawListRow`, `drawNavigationCard`, `drawTextEntryRow`, ...) and their
  `ScreenChrome::` forwarders -- intentionally centralized, see the top of
  this doc.
- `drawCurrentScreen()` -- the dispatch switch itself.
- `drawCyberFamiliarIdle()` and `drawCyberdeckIdle()` -- ambient idle-mode
  animations that run outside normal screen navigation, not `Screen` enum
  entries.
- `drawBootConsole()`/`drawCornerBrackets()` -- the boot sequence, which
  runs before the screen-navigation loop starts.

Verified at every step: `pio run -e cardputer_adv` succeeded after each
batch, `pio test -e native` stayed at 3/3, and RAM/Flash grew by only ~700B
/ ~4.3KB (0.2% / 0.1% of budget) across the *entire* refactor -- the
expected cost of reference members and reduced cross-TU inlining, not a
behavior change.

Turning the remaining chrome/idle/boot code into further modules, and
deciding whether main.cpp's ~100+ globals should eventually move into a
proper `AppState` struct, are reasonable next steps but weren't required to
get every screen out.
