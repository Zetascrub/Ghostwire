#include "ir_screen.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void IrScreen::draw() {
    ScreenChrome::drawHeader("Infrared Self-Test");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 32);
    display.printf("Onboard TX: GPIO %u", IrService::kTransmitPin);
    display.setCursor(8, 49);
    display.printf("Carrier:    %u kHz", IrService::kCarrierKhz);
    display.setCursor(8, 66);
    display.printf("Tests sent: %lu",
                   static_cast<unsigned long>(service_.transmissionCount()));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 86);
    display.print("RX requires an external receiver.");
    display.setCursor(8, 102);
    display.print("View emitter through phone camera.");
    ScreenChrome::drawFooter("Enter/R: test burst   Q: back");
}

void IrScreen::transmitSelfTest() {
    ScreenChrome::drawHeader("Infrared Self-Test");
    M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
    M5Cardputer.Display.setCursor(8, 45);
    M5Cardputer.Display.print("Transmitting test pattern...");
    ScreenChrome::drawFooter("Point the IR end toward a camera");
    service_.sendSelfTest();
    ScreenChrome::recoverKeyboard();
    Serial.printf("[ir] self-test burst=%lu pin=%u carrier=%u kHz\n",
                  static_cast<unsigned long>(service_.transmissionCount()),
                  IrService::kTransmitPin, IrService::kCarrierKhz);
    draw();
}
