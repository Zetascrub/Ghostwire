// The ESP32 Arduino core's closed-source WiFi driver (libnet80211.a)
// contains an internal frame-type check, ieee80211_raw_frame_sanity_check,
// that esp_wifi_80211_tx() calls before transmitting any raw 802.11 frame.
// That check deliberately rejects deauthentication/disassociation frame
// types -- Espressif's own stated reason (github.com/espressif/esp-idf,
// issue #1256) is specifically to prevent raw deauth transmission through
// the public API. Ghostwire is an authorized security assessment tool
// where sending a deauth frame to an operator-selected, confirmation-gated
// target (see transmitWifiDeauth() in main.cpp) is an intentional feature,
// not something that should be silently blocked by the driver.
//
// The technique of making the linker prefer this file's definition over
// the library's (via weakening the library symbol at build time -- see
// patch_wifi_lib.py) is documented prior art in the ESP32 community, e.g.
// github.com/GANESH-ICMC/esp32-deauther and its adoption in Bruce firmware
// (github.com/pr3y/Bruce, AGPL-3.0). This file does not reuse their code:
// the exact parameter layout and pass/fail return contract are specific to
// the *compiled* WiFi library binary shipped with a given Arduino-ESP32
// core version, and Bruce's build targets a materially newer core (3.x)
// than Ghostwire's pinned 2.0.16. This implementation was written after
// disassembling this project's actual libnet80211.a (Arduino-ESP32 core
// 2.0.16, ESP32-S3 target, via xtensa-esp32s3-elf-objdump/readelf) to
// confirm, independently of any other project's assumptions:
//   - esp_wifi_80211_tx(ifx, buffer, len, en_sys_seq) calls this function
//     with exactly those four arguments, in that order.
//   - Its return value is an esp_err_t: 0 (ESP_OK) means "allowed, proceed
//     with transmission"; a nonzero value (e.g. ESP_ERR_INVALID_ARG) is
//     returned directly by esp_wifi_80211_tx as the rejection.
//   - The original implementation enforces a length bound (24-1500 bytes,
//     matching esp_wifi_80211_tx's documented contract) before checking
//     frame type. We keep the length bound -- a real, content-independent
//     safety check -- and remove only the frame-type restriction.
//
// This relies on undocumented internal behavior of a closed-source binary
// and is inherently fragile: any future Arduino-ESP32 core upgrade could
// change this function's signature, calling convention, or return
// contract without notice. The safe failure mode is a link/behavior
// mismatch that leaves raw TX rejecting everything again (as it does
// today); re-verify by disassembly before ever bumping the platform/
// framework version in platformio.ini.

#include <cstdint>
#include <esp_err.h>

extern "C" esp_err_t ieee80211_raw_frame_sanity_check(int32_t ifx,
                                                      const uint8_t* buffer,
                                                      int32_t len) {
    (void)ifx;
    (void)buffer;
    if (len < 24 || len > 1500) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}
