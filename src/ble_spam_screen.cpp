#include "ble_spam_screen.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

namespace {
const char* const kModeLabels[BleSpamScreen::kModeCount] = {
    "Apple", "Fast Pair", "Swift Pair", "All",
};
}  // namespace

const char* BleSpamScreen::modeLabel(size_t selection) {
    return kModeLabels[selection < kModeCount ? selection : kModeCount - 1];
}

BleSpamMode BleSpamScreen::modeForSelection(size_t selection) {
    switch (selection) {
        case 0: return BleSpamMode::Apple;
        case 1: return BleSpamMode::FastPair;
        case 2: return BleSpamMode::SwiftPair;
        default: return BleSpamMode::All;
    }
}

void BleSpamScreen::drawSelect(size_t selection) {
    ScreenChrome::drawHeader("BLE Spam");
    for (size_t row = 0; row < kModeCount; ++row) {
        ScreenChrome::drawListRow(row, kModeLabels[row], row == selection);
    }
    ScreenChrome::drawFooter("Enter: start   Backspace/Q: back");
}

void BleSpamScreen::drawActive(bool fullDraw) {
    ScreenChrome::beginContentUpdate("BLE Spam", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(service_.isActive() ? Branding::accent
                                              : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("%s", service_.isActive() ? "SPAMMING" : "STOPPED");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Type: %s", service_.currentTypeName());
    display.setCursor(8, 64);
    display.printf("Sent: %lu",
                   static_cast<unsigned long>(service_.packetsSent()));
    const uint8_t* mac = service_.currentAddress();
    display.setCursor(8, 81);
    display.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4],
                   mac[3], mac[2], mac[1], mac[0]);
    if (fullDraw) ScreenChrome::drawFooter("Q: stop");
}
