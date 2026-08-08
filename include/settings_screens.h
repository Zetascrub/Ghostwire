#pragma once

#include <Arduino.h>

// Settings sub-screens (Display, Boot, Connectivity, Familiar LED, Reset
// confirm), plus the standalone About and Placeholder screens -- grouped as
// one module
// since they're all reached from the Settings/Tools menus and none carry
// state beyond the shared list cursor and the preference values they show.
// Adjusting a value (Left/Right), saving to flash, and the boot preview
// actions stay in main.cpp. See docs/screen-extraction.md; the root
// Settings menu itself lives in MenuScreens (it's a plain list menu, no
// unique state).
class SettingsScreens {
public:
    SettingsScreens(size_t& listSelection, size_t& listOffset,
                    uint8_t& speakerVolume, uint8_t& screenBrightness,
                    uint16_t& screenTimeoutSeconds, bool& cyberdeckIdleEnabled,
                    size_t& themeIndex, uint8_t& familiarCueIndex,
                    bool& cardNavigationEnabled, uint8_t& cyberdeckIdleStyle,
                    bool& bootSoundEnabled, uint8_t& bootSoundIndex,
                    uint8_t& bootAnimationIndex, uint8_t& bootSpeedIndex,
                    bool& saveWifiCredentials, bool& autoConnectWifi,
                    bool& familiarLedEnabled, uint8_t& familiarLedStartedColor,
                    uint8_t& familiarLedHostColor,
                    uint8_t& familiarLedServiceColor,
                    uint8_t& familiarLedWarningColor,
                    uint8_t& familiarLedCompleteColor,
                    uint8_t& familiarLedErrorColor, String& placeholderTitle)
        : listSelection_(listSelection),
          listOffset_(listOffset),
          speakerVolume_(speakerVolume),
          screenBrightness_(screenBrightness),
          screenTimeoutSeconds_(screenTimeoutSeconds),
          cyberdeckIdleEnabled_(cyberdeckIdleEnabled),
          themeIndex_(themeIndex),
          familiarCueIndex_(familiarCueIndex),
          cardNavigationEnabled_(cardNavigationEnabled),
          cyberdeckIdleStyle_(cyberdeckIdleStyle),
          bootSoundEnabled_(bootSoundEnabled),
          bootSoundIndex_(bootSoundIndex),
          bootAnimationIndex_(bootAnimationIndex),
          bootSpeedIndex_(bootSpeedIndex),
          saveWifiCredentials_(saveWifiCredentials),
          autoConnectWifi_(autoConnectWifi),
          familiarLedEnabled_(familiarLedEnabled),
          familiarLedStartedColor_(familiarLedStartedColor),
          familiarLedHostColor_(familiarLedHostColor),
          familiarLedServiceColor_(familiarLedServiceColor),
          familiarLedWarningColor_(familiarLedWarningColor),
          familiarLedCompleteColor_(familiarLedCompleteColor),
          familiarLedErrorColor_(familiarLedErrorColor),
          placeholderTitle_(placeholderTitle) {}

    void drawDisplay();
    void drawBoot();
    void drawConnectivity();
    void drawFamiliarLed();
    void drawResetConfirm();
    void drawPlaceholder();
    void drawAbout();

private:
    size_t& listSelection_;
    size_t& listOffset_;
    uint8_t& speakerVolume_;
    uint8_t& screenBrightness_;
    uint16_t& screenTimeoutSeconds_;
    bool& cyberdeckIdleEnabled_;
    size_t& themeIndex_;
    uint8_t& familiarCueIndex_;
    bool& cardNavigationEnabled_;
    uint8_t& cyberdeckIdleStyle_;
    bool& bootSoundEnabled_;
    uint8_t& bootSoundIndex_;
    uint8_t& bootAnimationIndex_;
    uint8_t& bootSpeedIndex_;
    bool& saveWifiCredentials_;
    bool& autoConnectWifi_;
    bool& familiarLedEnabled_;
    uint8_t& familiarLedStartedColor_;
    uint8_t& familiarLedHostColor_;
    uint8_t& familiarLedServiceColor_;
    uint8_t& familiarLedWarningColor_;
    uint8_t& familiarLedCompleteColor_;
    uint8_t& familiarLedErrorColor_;
    String& placeholderTitle_;
};
