#pragma once

// Settings label tables for the boot/display preference screens. Header-only
// shared constants (see docs/screen-extraction.md): main.cpp's input
// handlers cycle the *_Count values, settings_screens.cpp renders the
// *_Names labels -- both need the same array, so it lives here rather than
// in either translation unit.
inline constexpr const char* const kBootAnimationNames[] = {
    "System Console", "Cipher Reveal", "Radar Sweep", "Minimal",
    "Neon Breach", "Hacker Terminal", "Silly Bounce", "Synthwave Grid",
};
inline constexpr size_t kBootAnimationCount =
    sizeof(kBootAnimationNames) / sizeof(kBootAnimationNames[0]);

inline constexpr const char* const kBootSoundNames[] = {
    "Classic Chime", "Hero Signal", "Arcade Ready",
    "Starship", "Mystic Spark",
};
inline constexpr size_t kBootSoundCount =
    sizeof(kBootSoundNames) / sizeof(kBootSoundNames[0]);

inline constexpr const char* const kBootSpeedNames[] = {"Slow", "Normal", "Fast"};
inline constexpr size_t kBootSpeedCount =
    sizeof(kBootSpeedNames) / sizeof(kBootSpeedNames[0]);

inline constexpr const char* const kFamiliarCueNames[] = {
    "No sound", "Subtle", "Chirps", "Arcade", "Mystic", "Voice pack",
};
inline constexpr size_t kFamiliarCueCount =
    sizeof(kFamiliarCueNames) / sizeof(kFamiliarCueNames[0]);

inline constexpr const char* const kCyberdeckIdleStyleNames[] = {
    "Data Rain", "Signal Radar", "Node Drift",
};
inline constexpr size_t kCyberdeckIdleStyleCount =
    sizeof(kCyberdeckIdleStyleNames) / sizeof(kCyberdeckIdleStyleNames[0]);

// Familiar LED alert palette. "Off" (index 0) disables the LED for whichever
// event it's assigned to without a separate per-event enable flag. The RGB
// values match tools/send_led_message.py's COLORS table so an operator
// testing over UDP and one reading the on-device palette see the same names.
struct LedColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};
inline constexpr const char* const kLedColorNames[] = {
    "Off", "Red", "Green", "Blue", "Cyan", "Amber", "Yellow", "Magenta",
    "Purple", "White",
};
inline constexpr size_t kLedColorCount =
    sizeof(kLedColorNames) / sizeof(kLedColorNames[0]);
inline constexpr LedColor kLedColorValues[] = {
    {0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 80, 255}, {0, 255, 255},
    {255, 120, 0}, {255, 220, 0}, {255, 0, 180}, {150, 40, 255},
    {255, 255, 255},
};
