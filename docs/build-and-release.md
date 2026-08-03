# Build and release

## Local verification

PlatformIO stores all platforms and packages under `.pio-core` in this
checkout. Dependencies are pinned in `platformio.ini`.

```sh
pio test -e native
pio run -e cardputer_adv
pio run -e cardputer_adv --target upload
```

PlatformIO auto-detects the serial port. To enter download mode, power the
Cardputer ADV off, hold `G0`, connect USB, then release `G0`.

## Release checklist

1. Start from a clean tagged commit.
2. Run the native tests and firmware build.
3. Test boot, keyboard, display, SD, audio, Wi-Fi connect/disconnect, BLE
   lifecycle, GPS/LoRa cap, logging, and USB serial/HID on hardware.
4. Confirm every active transmit feature still has a confirmation/cancel path.
5. Package `firmware.bin`, `bootloader.bin`, and `partitions.bin`.
6. Generate `SHA256SUMS` and publish it with the binaries and release notes.
7. Package `sd-card-files/` separately and state its required layout/version.

The CI workflow performs steps 2, 5, and 6 and publishes a downloadable build
artifact for every reviewed change.
