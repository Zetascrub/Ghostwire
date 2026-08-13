#include "evil_portal_screen.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void EvilPortalScreen::drawConfirm() {
    ScreenChrome::drawHeader("Start Evil Portal?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 32);
    display.print(("Clone \"" + pendingSsid_ + "\" (open AP, CH " +
                   String(pendingChannel_) + ")")
                       .substring(0, 240));
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 54);
    display.print("Serves a sign-in page to anyone");
    display.setCursor(8, 68);
    display.print("who connects. Submitted logins");
    display.setCursor(8, 82);
    display.print("are saved to SD as plain text.");
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 100);
    display.print("Authorized targets only.");
    ScreenChrome::drawFooter("Enter: start   Backspace/Q: cancel");
}

void EvilPortalScreen::draw(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Evil Portal", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(service_.isActive() ? Branding::accent
                                              : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  %s", service_.isActive() ? "LIVE" : "STOPPED",
                   service_.ssid().substring(0, 20).c_str());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 48);
    display.printf("Clients connected: %lu",
                   static_cast<unsigned long>(service_.clientCount()));
    display.setCursor(8, 65);
    display.setTextColor(captureCount_ > 0 ? Branding::accent
                                           : Branding::muted,
                         Branding::background);
    display.printf("Logins captured: %lu",
                   static_cast<unsigned long>(captureCount_));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 84);
    display.print(lastCapture_.isEmpty() ? "No submissions yet."
                                         : lastCapture_.substring(0, 37));
    if (fullDraw) {
        ScreenChrome::drawFooter("Esc: stop and back");
    }
}
