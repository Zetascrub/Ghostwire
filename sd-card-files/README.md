# Ghostwire SD-card files

Copy the contents of this directory to the root of the Cardputer's microSD
card. The directory structure here mirrors the intended paths on the card.

- `ghostwire/audio`: MP3 format and placement guidance. Familiar voice-bank
  clips are optional and must be supplied under terms that permit your use and
  redistribution; selecting **No sound** requires no audio assets.
- `ghostwire/scripts`: guarded DuckyScript placement and supported commands.
- `ghostwire/secrets`: blank AI configuration example and credential guidance.
- `ghostwire/mesh`: private Meshtastic receive-profile example. Rename
  `channels.example.json` to `channels.json`, replace the example with the
  exact authorised channel name and Base64 PSK, and remove the placeholder.
  Ghostwire accepts 16-byte AES-128 or 32-byte AES-256 keys and never displays
  key material on-device; public LongFast remains enabled.

Runtime log, assessment, and saved-tag directories are created by the firmware
as needed. Their contents appear together under the on-device **Evidence**
mission, while the raw **Files** utility remains available in the Field kit.
