#include "qr_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void QrScreens::drawEntry() {
    ScreenChrome::drawHeader("QR Generator");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 34);
    display.print("Enter text, URL, or short note:");
    ScreenChrome::drawTextEntryRow(56, "> ", qrText_);
    display.setCursor(8, 78);
    display.printf("%u / 100 characters", static_cast<unsigned>(qrText_.length()));
    display.setCursor(8, 96);
    display.print("Generated entirely offline.");
    ScreenChrome::drawFooter("Enter: generate   Esc: back");
}

void QrScreens::drawDisplay() {
    ScreenChrome::drawHeader("QR Code");
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 22, display.width(), display.height() - 37, TFT_WHITE);
    display.qrcode(qrText_, 74, 25, 92, 6);
    ScreenChrome::drawFooter("Enter: edit   Esc: back");
}
