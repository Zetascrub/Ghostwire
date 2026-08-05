#include "gnss_screen.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void GnssScreen::draw(bool fullDraw) {
    ScreenChrome::beginContentUpdate("GNSS Foundation", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(service_.hasFix() ? Branding::accent
                                            : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    if (!service_.hasData()) {
        display.print("UART: waiting for NMEA data");
    } else if (!service_.hasFix()) {
        display.print("GNSS: data received, no fix");
    } else {
        display.printf("GNSS: position fix%s",
                       logger_.isActive() ? "  REC" : "");
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Sat: %lu  HDOP: %.1f  UTC: %s",
                   static_cast<unsigned long>(service_.satellites()),
                   service_.hdop(), service_.utcTime().c_str());
    display.setCursor(8, 64);
    if (service_.hasFix()) {
        display.printf("Lat: %.6f", service_.latitude());
        display.setCursor(8, 81);
        display.printf("Lon: %.6f", service_.longitude());
        display.setCursor(8, 98);
        display.printf("Altitude: %.1f m", service_.altitudeMetres());
        if (logger_.isActive()) {
            display.printf("  REC %lu",
                           static_cast<unsigned long>(logger_.rowCount()));
        }
    } else {
        display.printf("NMEA bytes: %lu",
                       static_cast<unsigned long>(service_.charactersProcessed()));
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 82);
        display.print("Move outdoors for first fix.");
        if (logger_.isActive()) {
            display.setCursor(8, 98);
            display.printf("REC %lu rows",
                           static_cast<unsigned long>(logger_.rowCount()));
        }
    }
    if (fullDraw) {
        ScreenChrome::drawFooter("R: restart GNSS   Tab: actions   Q: back");
    }
}
