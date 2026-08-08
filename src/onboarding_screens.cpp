#include "onboarding_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

namespace {

void paragraph(const char* line1, const char* line2, const char* line3 = nullptr) {
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 40);
    display.print(line1);
    display.setCursor(8, 58);
    display.print(line2);
    if (line3) {
        display.setCursor(8, 76);
        display.print(line3);
    }
}

}  // namespace

void OnboardingScreens::draw() {
    static const char* const titles[] = {
        "Welcome to Ghostwire", "Observe > Scout > Record",
        "Meet Your Familiar", "Evidence Storage", "Choose Navigation",
        "Network Profiles", "Ready for the Field",
    };
    if (page_ >= kPageCount) page_ = kPageCount - 1;
    ScreenChrome::drawHeader(titles[page_]);
    ScreenChrome::drawHeaderPosition(page_ + 1, kPageCount);

    switch (page_) {
        case 0:
            paragraph("A pocket radio and network scout.",
                      "Notice change, collect evidence,", "analyse deeper elsewhere.");
            break;
        case 1:
            paragraph("OBSERVE nearby signals.", "SCOUT connected networks.",
                      "RECORD useful evidence to SD.");
            break;
        case 2:
            paragraph("Your Familiar reacts to discoveries,",
                      "guides Patrol and remembers progress.",
                      "It is the face of your field work.");
            break;
        case 3: {
            auto& display = M5Cardputer.Display;
            paragraph("Captures, patrols and reports use", "the microSD card whenever possible.");
            display.setTextColor(sdAvailable_ ? Branding::accent
                                              : Branding::warning,
                                 Branding::background);
            display.setCursor(8, 82);
            display.print(sdAvailable_ ? "microSD: READY" : "microSD: NOT FOUND");
            break;
        }
        case 4:
            paragraph("Compact shows efficient lists.",
                      "Cards uses large icons and labels.");
            ScreenChrome::drawListRow(4, "Navigation", true,
                                      cardNavigationEnabled_ ? "Cards"
                                                             : "Compact");
            break;
        case 5:
            paragraph("Optionally retain up to five named",
                      "Wi-Fi networks on this device.",
                      "Turning this off erases saved profiles.");
            ScreenChrome::drawListRow(4, "Save profiles", true,
                                      saveWifiCredentials_ ? "On" : "Off");
            break;
        case 6:
            paragraph("Start with Observe signals, then",
                      "connect and Scout when authorised.",
                      "Your evidence remains available later.");
            break;
    }

    if (page_ == kPageCount - 1) {
        ScreenChrome::drawFooter("Enter:start  Q:back  S:skip");
    } else if (page_ == 4 || page_ == 5) {
        ScreenChrome::drawFooter("Left/Right:choose Enter:next S:skip");
    } else {
        ScreenChrome::drawFooter("Enter:next  Q:back  S:skip");
    }
}
