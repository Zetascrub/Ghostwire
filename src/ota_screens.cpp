#include "ota_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void OtaScreens::drawCheck() {
    ScreenChrome::drawHeader("Firmware Update");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 32);
    display.print(service_.statusMessage().substring(0, 37));

    if (service_.hasVerifiedUpdate()) {
        display.setCursor(8, 54);
        display.printf("Size: %lu KiB",
                       static_cast<unsigned long>(service_.totalBytes() / 1024));
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 76);
        display.print("Signature will be verified before");
        display.setCursor(8, 92);
        display.print("anything is written to flash.");
        ScreenChrome::drawFooter("Enter: install   Backspace/Q: back");
    } else {
        ScreenChrome::drawFooter("R: check again   Backspace/Q: back");
    }
}

void OtaScreens::drawInstalling(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Installing Update", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 32);
    display.print(service_.statusMessage().substring(0, 37));

    const size_t downloaded = service_.downloadedBytes();
    const size_t total = service_.totalBytes();
    const int barX = 8;
    const int barY = 54;
    const int barW = display.width() - 16;
    const int barH = 14;
    display.drawRect(barX, barY, barW, barH, Branding::muted);
    if (total > 0) {
        const int fillW = static_cast<int>(
            (static_cast<uint64_t>(barW - 4) * downloaded) / total);
        display.fillRect(barX + 2, barY + 2, fillW, barH - 4, Branding::accent);
    }
    display.setCursor(8, barY + barH + 8);
    display.printf("%lu / %lu KiB", static_cast<unsigned long>(downloaded / 1024),
                   static_cast<unsigned long>(total / 1024));

    if (fullDraw) ScreenChrome::drawFooter("Esc: cancel");
}
