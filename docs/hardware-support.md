# Hardware support

The primary target is M5Stack Cardputer ADV (Stamp-S3A / ESP32-S3FN8, 8 MB
flash, no PSRAM). The build uses the `esp32-s3-devkitc-1` PlatformIO board
definition with Cardputer-specific support supplied by M5Cardputer.

Validated onboard assignments:

| Function | Pins |
| --- | --- |
| Battery ADC | GPIO10 |
| IR transmit | GPIO44 |
| Keyboard/IMU/audio I2C | GPIO8/GPIO9 |
| Keyboard interrupt | GPIO11 |
| microSD | CS12, MOSI14, SCK40, MISO39 |
| GPS/LoRa cap UART | RX13, TX15 |
| LoRa cap | CS5, reset3, DIO4, shared SPI |

The ADV shares buses and the ESP32-S3 Wi-Fi/BLE radio. Features must coordinate
bus chip-select lines and may not run incompatible Wi-Fi and BLE modes at the
same time. External Grove GPS wiring may require different UART pins from the
GPS/LoRa cap.

Hardware support is not inferred from compilation alone. Record real-device
validation in release notes whenever a pin map, M5Cardputer version,
Arduino-ESP32 platform, audio path, radio lifecycle, or power behaviour changes.

## Unit PoE-P4 companion

The optional M5Stack Unit PoE-P4 (SKU U213, original silicon revision family)
has its own ESP-IDF 5.4.2 build root under `poe-p4/`. The first hardware-
validated image uses the onboard IP101GRI PHY with MDC GPIO31, MDIO GPIO52,
reset GPIO51, and the ESP32-P4 default RMII data pins. It targets the board's
16 MB flash and advertises its read-only companion service over wired Ethernet.
The onboard common-anode RGB status LED uses green GPIO15, blue GPIO16, and red
GPIO17 with low-brightness PWM for boot, LAN, internet, and Ghostwire states.
Its Grove link uses GPIO53 TX and GPIO54 RX. A straight HY2.0-4P cable maps
these to Cardputer ADV GPIO2 RX and GPIO1 TX; it may also power the P4 from the
Cardputer's 5 V output when the P4 has no simultaneous USB or PoE supply.
Grove carries live companion telemetry (not just a heartbeat): a status frame
once a second (including the P4's DHCP-assigned IP) and an identity frame
every ten. The Cardputer prefers this over its network status fetch whenever
Grove is fresh, falling back to mDNS/HTTP otherwise -- see
`shared/protocol/grove_link.h` for the frame formats. `Scout > Network > PoE
Companion` shows a compact summary (status, IP, LAN/internet); `Tab` opens
the rest of the telemetry (firmware, uptime, temperature, heap, LED state,
Grove link counters) on a separate detail screen.

This is not a Cardputer PlatformIO target and must not be flashed with the root
`pio run --target upload` command. Unit PoE-P4X uses a different silicon family
and is not supported by this build.
