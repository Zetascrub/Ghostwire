# Ghostwire Unit PoE-P4 Companion

This directory is an independent ESP-IDF application for the M5Stack Unit
PoE-P4. It is intentionally separate from the root PlatformIO project, which
continues to target the Cardputer ADV's ESP32-S3.

The first image provides a read-only vertical slice: IP101 Ethernet with DHCP,
DNS-SD discovery, a JSON status endpoint, WebSocket link/address events, and
an onboard RGB status indicator.
The wire contract is documented in [`../shared/protocol`](../shared/protocol/README.md).

## Prerequisites

Install or activate an ESP-IDF release with ESP32-P4 support. The target is
stored in the generated local `sdkconfig`, not in the Cardputer configuration.

## Build

From an ESP-IDF shell:

```sh
cd poe-p4
idf.py set-target esp32p4
idf.py build
```

When running, the companion advertises `_ghostwire._tcp.local` as
`ghostwire-poe-p4.local` on port `8765`. Its status endpoint is:

```text
http://ghostwire-poe-p4.local:8765/v1/status
```

## Flash and monitor

Confirm the serial device before writing to hardware, then run:

```sh
idf.py -p /dev/ttyACM0 flash monitor
```

The device number can change after a reset or reflash. Exit the monitor with
`Ctrl+]`.

Do not run the root `pio run --target upload` command for this board: that
configuration builds the Cardputer ADV firmware for ESP32-S3.

## RGB status indicator

The common-anode onboard RGB LED reports the companion's highest active state:

| Colour | Meaning |
| --- | --- |
| Pulsing amber | Firmware is booting |
| Red | Firmware ready, but the Ethernet subsystem has stopped |
| Blue | Firmware ready; waiting for Ethernet/IP |
| Cyan | Ethernet link and DHCP address established |
| Green | An outbound TCP probe confirms internet reachability |
| Purple | A Ghostwire client contacted the status or event endpoint recently |

Internet reachability is checked against `1.1.1.1:443` every 30 seconds. The
purple Ghostwire indication remains active for 30 seconds after contact; the
Cardputer polls every 10 seconds while its PoE Companion screen is open.

## Grove UART link

The companion sends a CRC32-protected heartbeat once per second over the Grove
signal pins at 115200 baud (8N1), using GPIO53 for TX and GPIO54 for RX. A
Cardputer ADV replies with a sequence-matched acknowledgement. The link becomes
active after a valid acknowledgement and expires after three seconds without
one. `/v1/status` exposes the link state and frame/error counters.

This protocol revision is intentionally read-only. Pairing and authentication
will be implemented before remote commands or network operations are enabled.

Phase 1 health and recovery checks are documented in
[`../docs/relay-phase-1-validation.md`](../docs/relay-phase-1-validation.md).
