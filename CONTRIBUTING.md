# Contributing

Keep changes small enough to review and test on a real Cardputer ADV. New
features should have a service/module boundary rather than adding unrelated
state directly to `main.cpp`.

Before submitting a change:

```sh
pio test -e native
pio run -e cardputer_adv
```

Document hardware used, firmware version, radio state before and after the
operation, and whether microSD, GPS/LoRa cap, BLE accessories, audio, and USB
HID were exercised. Never attach real credentials, identifying packet
captures, RFID dumps, or location logs.

Active transmit features require an explicit confirmation screen and an
obvious cancellation path. Long-running operations must remain responsive to
keyboard input, report dropped data, and release their radio/socket/logger
resources when leaving the screen.

Direct dependencies must be pinned to an exact version or commit. Updating the
Arduino-ESP32 platform requires revalidating `patch_wifi_lib.py` against the
new `libnet80211.a`; do not update its checksum without reviewing the symbol's
ABI and transmission behaviour.
