#include "wifi_guardian_screen.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void WifiGuardianScreen::draw(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Familiar Guardian", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(guardian_.isActive() ? Branding::accent
                                               : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  CH %u  %s",
                   guardian_.isActive() ? "WATCHING" : "STOPPED",
                   sniffer_.currentChannel(), guardian_.sensitivityName());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Mgmt:%lu  Beacon:%lu Probe:%lu",
                   static_cast<unsigned long>(guardian_.managementFrames()),
                   static_cast<unsigned long>(guardian_.beaconFrames()),
                   static_cast<unsigned long>(guardian_.probeFrames()));
    display.setCursor(8, 63);
    display.printf("Deauth:%lu Disassoc:%lu Recent:%u",
                   static_cast<unsigned long>(guardian_.deauthFrames()),
                   static_cast<unsigned long>(guardian_.disassocFrames()),
                   guardian_.recentDisruptionFrames());
    display.setCursor(8, 80);
    display.printf("Alerts:%lu Evidence:%lu Drop:%lu",
                   static_cast<unsigned long>(guardian_.alertCount()),
                   static_cast<unsigned long>(evidenceLogger_.rowCount()),
                   static_cast<unsigned long>(sniffer_.droppedRawFrameCount()));
    display.setTextColor(
        guardian_.alertCount() > 0 ? Branding::warning : Branding::muted,
        Branding::background);
    display.setCursor(8, 97);
    display.print(lastEvent_.substring(0, 37));
    if (fullDraw) {
        ScreenChrome::drawFooter("S: sensitivity  R: restart  Q: save/exit");
    }
}
