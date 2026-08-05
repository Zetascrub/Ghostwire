#pragma once

#include <Arduino.h>

// USB/HID self-test preset labels, indexed by HidPreset (see hid_service.h).
// Header-only shared constant (see docs/screen-extraction.md) since both
// main.cpp's runUsbHidPreset() and usb_hid_screens.cpp need it.
inline constexpr const char* const kHidPresetNames[] = {
    "Ghostwire signature",
    "Keyboard layout sample",
    "Slow typing cadence",
};
inline constexpr size_t kHidPresetCount =
    sizeof(kHidPresetNames) / sizeof(kHidPresetNames[0]);
