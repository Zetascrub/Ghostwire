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
