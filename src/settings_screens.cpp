#include "settings_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"
#include "settings_names.h"

void SettingsScreens::drawDisplay() {
    ScreenChrome::drawHeader("Settings: Display");
    constexpr size_t count = 8;
    ScreenChrome::normalizeListPosition(count);
    const String timeout = screenTimeoutSeconds_ == 0
                               ? "Off"
                               : String(screenTimeoutSeconds_) + "s";
    const char* const labels[count] = {
        "Speaker volume", "Screen brightness", "Screen timeout",
        "Cyberdeck idle", "Theme", "Familiar cues", "Navigation style",
        "Idle animation",
    };
    const String values[count] = {
        String((speakerVolume_ * 100U) / 255U) + "%",
        String((screenBrightness_ * 100U) / 255U) + "%",
        timeout, cyberdeckIdleEnabled_ ? "On" : "Off",
        Branding::kThemes[themeIndex_].name,
        kFamiliarCueNames[familiarCueIndex_],
        cardNavigationEnabled_ ? "Cards" : "Compact",
        kCyberdeckIdleStyleNames[cyberdeckIdleStyle_],
    };
    for (size_t row = 0; row < ScreenChrome::kVisibleRows && row + listOffset_ < count;
        ++row) {
        const size_t item = row + listOffset_;
        ScreenChrome::drawListRow(row, labels[item], item == listSelection_,
                                  values[item]);
    }
    ScreenChrome::drawFooter("Up/Down: select  Left/Right: adjust");
}

void SettingsScreens::drawBoot() {
    ScreenChrome::drawHeader("Settings: Boot");
    constexpr size_t count = 6;
    ScreenChrome::normalizeListPosition(count);
    const char* const labels[count] = {
        "Boot sound", "Sound style", "Animation style", "Boot speed",
        "Preview sound", "Preview animation",
    };
    const String values[count] = {
        bootSoundEnabled_ ? "On" : "Off",
        kBootSoundNames[bootSoundIndex_],
        kBootAnimationNames[bootAnimationIndex_],
        kBootSpeedNames[bootSpeedIndex_], "Enter", "Enter",
    };
    for (size_t row = 0; row < count; ++row) {
        ScreenChrome::drawListRow(row, labels[row], row == listSelection_,
                                  values[row]);
    }
    ScreenChrome::drawFooter("Left/Right: adjust  Enter: preview");
}

void SettingsScreens::drawConnectivity() {
    ScreenChrome::drawHeader("Settings: Connectivity");
    constexpr size_t count = 2;
    ScreenChrome::normalizeListPosition(count);
    ScreenChrome::drawListRow(0, "Save Wi-Fi login", listSelection_ == 0,
                              saveWifiCredentials_ ? "On" : "Off");
    ScreenChrome::drawListRow(1, "Auto-connect Wi-Fi", listSelection_ == 1,
                              autoConnectWifi_ ? "On" : "Off");
    ScreenChrome::drawFooter("Left/Right: toggle   Q: back");
}

void SettingsScreens::drawResetConfirm() {
    ScreenChrome::drawHeader("Restore Settings?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 42);
    display.print("Reset all preferences?");
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 64);
    display.print("This does not erase SD files.");
    ScreenChrome::drawFooter("Enter: restore   Backspace/Q: cancel");
}

void SettingsScreens::drawPlaceholder() {
    ScreenChrome::drawHeader(placeholderTitle_.c_str());
    M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
    M5Cardputer.Display.setCursor(8, 42);
    M5Cardputer.Display.print("Module slots ready.");
    M5Cardputer.Display.setCursor(8, 59);
    M5Cardputer.Display.print("Tools arrive in later builds.");
    ScreenChrome::drawFooter("Esc: back");
}

void SettingsScreens::drawAbout() {
    ScreenChrome::drawHeader("About");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 34);
    display.printf("%s %s", Branding::productName, Branding::version);
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(8, 54);
    display.printf("by %s", Branding::creatorName);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 76);
    display.print("Pocket network and radio scout");
    display.setCursor(8, 92);
    display.print("Observe. Scout. Bring evidence.");
    ScreenChrome::drawFooter("Esc: back");
}
