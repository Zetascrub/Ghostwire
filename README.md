# Ghostwire

**A pocket network and radio scout for the M5Stack Cardputer ADV.**

Ghostwire turns the Cardputer into a field companion for authorised security
work: it observes nearby Wi-Fi/BLE/mesh signals, scouts a connected network,
and keeps SD-backed evidence you can pull onto a full workstation later. A
persistent on-screen companion — the Familiar — reacts to what the deck finds
and helps you notice changes, rather than pretending a handheld replaces a
laptop-class assessment suite.

Current development firmware: **Ghostwire 0.6.0-dev**. Full version history
lives in [CHANGELOG.md](CHANGELOG.md).

> Ghostwire is for use on equipment you own or have explicit authorization to
> test. Read [Authorized use and data handling](docs/authorized-use.md) before
> collecting radio, network, RFID, terminal, or location data.

## Quick links

- [What it can do](#what-it-can-do) — feature overview
- [Full feature reference](docs/features.md) — every option, screen by screen
- [Build and release](docs/build-and-release.md) — download mode, packaging, hardware test checklist
- [Hardware support](docs/hardware-support.md) — target board and pinout
- [Authorized use and data handling](docs/authorized-use.md)
- [Roadmap](docs/roadmap.md)
- [Changelog](CHANGELOG.md)

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
Copy the contents of [`sd-card-files`](sd-card-files/README.md) to the root of
a microSD card for scripts, audio, and AI configuration — the repository keeps
blank examples and setup instructions, while local credentials such as
`ghostwire/secrets/ai.json` stay ignored by git.

See [Build and release](docs/build-and-release.md) for download-mode
instructions, release packaging, and the hardware test checklist.

## What it can do

The home screen is organised around intent rather than subsystems: **My
Familiar / Observe signals / Scout network / Evidence / Field kit /
Settings**. Two navigation styles are available in Settings — compact lists or
a card-based interface — alongside a handful of built-in visual themes.

- **Wi-Fi** — AP discovery, channel analysis, passive/full PCAP capture, Guardian deauth watch, handshake/PMKID capture, connect with saved profiles
- **BLE** — advertisement inspection with continuous capture, HID keyboard emulation
- **GPS & Mesh** — GNSS logging, LoRa reception, a full Meshtastic chat client (channels, DMs, node radar)
- **War Drive** — combined GPS-tagged Wi-Fi + BLE capture, WiGLE-compatible export
- **Network** — live connection dashboard, host discovery, port scanning, Telnet/SSH clients
- **Devices** — Biscuit Pro and Chameleon Ultra (RFID) workflows over BLE
- **AI Chat** — OpenAI/Claude chat with voice input and speech replies
- **Cyber Familiar** — a persistent companion that reacts to discoveries and can run an unattended network Patrol
- **Tools** — IR, USB/HID + DuckyScript runner, audio, QR generator, IMU, file browser, diagnostics
- **Settings** — display, audio, boot experience, connectivity, themes, safe reset

See the [full feature reference](docs/features.md) for exact behaviour, key
bindings, and file formats for every screen.

## Controls

Move with the arrow keys (`W`/`S`, `K`/`J`, and `;`/`.` work too), select with
`Enter`, and go back with `Escape`, `Backspace`, Left, `Q`, or `B`. `R`
refreshes or starts/stops the current operation, and `Tab` opens a screen's
contextual action menu where one is available. Text-entry and live terminal
screens show their own exit key in the footer.

Press **`Ctrl` + `Alt` + `Backspace`** at any time for the global emergency
stop — it halts active radio operations, sockets, playback, and logging,
disconnects Wi-Fi, and returns to the main menu.

The full shortcut list, including AI Chat and BLE Keyboard sessions, is
implemented centrally in `handleInput()` in [`src/main.cpp`](src/main.cpp).

## Inspired by

Ghostwire owes a debt to the projects that made a keyboard-driven pocket
security toolkit feel possible: [Bruce](https://github.com/BruceDevices/firmware),
[Evil-Cardputer](https://github.com/7h30th3r0n3/Evil-M5Project), and
[Meshtastic](https://meshtastic.org/). Ghostwire is a separate codebase built
around the Cardputer ADV's specific hardware and an "authorised field
companion" workflow rather than a port of any of them — see the
[roadmap](docs/roadmap.md) for how feature ideas are chosen.

## Roadmap

Ghostwire's product direction, release gates, and deliberately shelved ideas
(jammers, broad floods, ARP poisoning, and similar) are maintained in
[docs/roadmap.md](docs/roadmap.md).

## License

Ghostwire is released under the [MIT License](LICENSE).
