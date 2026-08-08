#include "familiar_mission_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void FamiliarMissionScreens::drawSelect() {
    ScreenChrome::drawHeader("Handshake Mission: Targets");
    ScreenChrome::normalizeListPosition(accessPoints_.size());
    if (accessPoints_.empty()) {
        auto& display = M5Cardputer.Display;
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 38);
        display.print("No scan results. Run Wi-Fi Discovery first.");
        ScreenChrome::drawFooter("Q: back");
        return;
    }
    size_t chosen = 0;
    for (bool value : selected_) {
        if (value) ++chosen;
    }
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, accessPoints_.size());
    for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                         row + listOffset_ < accessPoints_.size();
        ++row) {
        const size_t item = row + listOffset_;
        const auto& ap = accessPoints_[item];
        String ssid = reinterpret_cast<const char*>(ap.ssid);
        if (ssid.isEmpty()) ssid = "<hidden>";
        const bool isSelected = item < selected_.size() && selected_[item];
        const String label =
            String(isSelected ? "[x] " : "[ ] ") + ssid.substring(0, 20);
        ScreenChrome::drawListRow(row, label, item == listSelection_,
                                  "CH " + String(ap.primary));
    }
    const String footer =
        chosen == 0 ? String("Enter: toggle   Q: back")
                   : "Enter: toggle   Tab: start (" + String(chosen) +
                         ")   Q: back";
    ScreenChrome::drawFooter(footer.c_str());
}

void FamiliarMissionScreens::drawConfirm() {
    ScreenChrome::drawHeader("Start Handshake Mission?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 32);
    display.printf("Sends deauth frames to %u network%s:",
                   static_cast<unsigned>(targets_.size()),
                   targets_.size() == 1 ? "" : "s");
    display.setTextColor(Branding::text, Branding::background);
    for (size_t row = 0; row < 4 && row < targets_.size(); ++row) {
        display.setCursor(8, 50 + static_cast<int>(row) * 15);
        display.print(("- " + targets_[row].ssid).substring(0, 34));
    }
    if (targets_.size() > 4) {
        display.setCursor(8, 110);
        display.setTextColor(Branding::muted, Branding::background);
        display.printf("...and %u more", static_cast<unsigned>(targets_.size() - 4));
    }
    ScreenChrome::drawFooter("Enter: confirm and start   Q: cancel");
}

void FamiliarMissionScreens::drawMissionDynamic() {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 24, display.width(), display.height() - 39,
                     Branding::background);
    if (currentIndex_ >= targets_.size()) return;
    const auto& target = targets_[currentIndex_];
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  CH %u", target.ssid.substring(0, 20).c_str(),
                   target.channel);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", target.bssid[0],
                   target.bssid[1], target.bssid[2], target.bssid[3],
                   target.bssid[4], target.bssid[5]);
    display.setCursor(8, 63);
    const uint32_t elapsedSeconds = (millis() - targetStartMs_) / 1000;
    display.printf("Elapsed: %lus   EAPOL: %lu",
                   static_cast<unsigned long>(elapsedSeconds),
                   static_cast<unsigned long>(handshakeEapolFrameCount_));
    display.setCursor(8, 80);
    display.print("Messages: ");
    for (uint8_t message = 1; message <= 4; ++message) {
        display.setTextColor(handshakeMessageSeen_[message] ? Branding::accent
                                                             : Branding::muted,
                             Branding::background);
        display.printf("M%u ", message);
    }
    display.setCursor(8, 97);
    display.setTextColor(handshakePmkidFound_ ? Branding::accent
                                              : Branding::muted,
                         Branding::background);
    display.print(handshakePmkidFound_ ? "PMKID captured" : "No PMKID yet");
}

void FamiliarMissionScreens::drawMission() {
    size_t captured = 0;
    for (const auto& target : targets_) {
        if (target.status == FamiliarMissionTarget::Status::Captured) {
            ++captured;
        }
    }
    ScreenChrome::drawHeader(("Mission " + String(currentIndex_ + 1) + "/" +
                              String(targets_.size()) + "  (" +
                              String(captured) + " captured)")
                                 .c_str());
    drawMissionDynamic();
    ScreenChrome::drawFooter(status_.isEmpty() ? "Esc: stop mission"
                                               : status_.c_str());
}
