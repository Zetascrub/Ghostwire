#include "device_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void BiscuitScreens::drawMain() {
    ScreenChrome::drawHeader("Biscuit Pro");
    auto& display = M5Cardputer.Display;
    const bool connected = client_.isConnected();
    display.setTextColor(connected ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.print(connected ? "CONNECTED - READY" : client_.lastStatus());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 52);
    display.printf("%s  FW %s", client_.model().c_str(),
                   client_.firmware().c_str());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 72);
    display.printf("C5 %s", client_.c5Firmware().c_str());
    display.setCursor(8, 92);
    display.printf("Device: %s", client_.deviceStatus().c_str());
    ScreenChrome::drawFooter(connected ? "Enter: tools   R: reconnect   Q: back"
                                       : "Enter/R: connect   Q: back");
}

void BiscuitScreens::drawTools() {
    static const char* const items[] = {
        "Device information", "Wi-Fi AP scan", "Station scan",
        "Packet count", "Current channel", "Node list", "Wardrive monitor",
    };
    ScreenChrome::drawHeader("Biscuit: Read-only tools");
    ScreenChrome::normalizeListPosition(7);
    for (size_t row = 0; row < ScreenChrome::kVisibleRows && row + listOffset_ < 7;
        ++row) {
        const size_t index = row + listOffset_;
        ScreenChrome::drawListRow(row, items[index], index == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: run   Backspace/Q: back");
}

void BiscuitScreens::drawWardrive(bool fullDraw) {
    const uint32_t signature =
        (static_cast<uint32_t>(wardriveActive_) << 31) ^
        (wardriveApCount_ * 2654435761UL) ^ wardriveBleCount_;
    if (!fullDraw && signature == lastWardriveSignature_) return;
    lastWardriveSignature_ = signature;
    ScreenChrome::beginContentUpdate("Biscuit Wardrive", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(wardriveActive_ ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 28);
    display.print(wardriveActive_ ? "MONITORING" : "STOPPED");
    display.setTextColor(Branding::text, Branding::background);
    display.setTextSize(2);
    display.setCursor(18, 54);
    display.printf("AP  %lu", static_cast<unsigned long>(wardriveApCount_));
    display.setCursor(18, 82);
    display.printf("BLE %lu", static_cast<unsigned long>(wardriveBleCount_));
    display.setTextSize(1);
    ScreenChrome::drawFooter(wardriveActive_ ? "Enter: stop   Q: back"
                                             : "Enter: start   Q: back");
}

void BiscuitScreens::drawResult() {
    ScreenChrome::drawHeader(resultTitle_.c_str());
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                         resultOffset_ + row < resultLines_.size();
        ++row) {
        display.setCursor(8, 29 + row * 15);
        display.print(resultLines_[resultOffset_ + row]);
    }
    ScreenChrome::drawFooter("Up/Down: scroll   Backspace/Q: tools");
}

String ChameleonScreen::hexId(const uint8_t* data, size_t len) {
    String hex;
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) hex += ':';
        if (data[i] < 0x10) hex += '0';
        hex += String(data[i], HEX);
    }
    return hex;
}

void ChameleonScreen::draw(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Chameleon Ultra", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(client_.isConnected() ? Branding::accent
                                                : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.print(client_.lastStatus());
    display.setTextColor(Branding::text, Branding::background);
    if (hasReadings_) {
        display.setCursor(8, 44);
        display.printf("App version: %u.%u", appMajor_, appMinor_);
        display.setCursor(8, 58);
        display.printf("Battery: %u%%  (%u mV)", batteryPct_, batteryMv_);
    }
    if (scanAttempted_) {
        display.setCursor(8, 72);
        if (hfFound_) {
            display.printf("UID: %s",
                           hexId(hfTag_.uid, hfTag_.uidLen).c_str());
            display.setCursor(8, 86);
            display.printf("ATQA: 0x%04X  SAK: 0x%02X", hfTag_.atqa, hfTag_.sak);
        } else if (lfFound_) {
            display.printf("EM410x ID: %s", hexId(lfId_, 5).c_str());
        } else {
            display.print("No tag found");
        }
    }
    display.setCursor(8, 100);
    if (!workflowStatus_.isEmpty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.print(workflowStatus_.substring(0, 37));
    } else {
        display.printf("Continuous: %s", continuousScan_ ? "ON" : "OFF");
        if (logger_.isActive()) {
            display.printf("  Logged: %lu",
                           static_cast<unsigned long>(logger_.rowCount()));
        }
    }
    if (fullDraw) ScreenChrome::drawFooter("R: reconnect  Tab: actions  Q: back");
}

void ChameleonScreen::drawEmulateConfirm() {
    ScreenChrome::drawHeader("Confirm Identity Emulation");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 32);
    display.print("This changes Chameleon slot 8.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 50);
    display.print(hfFound_ ? "HF identity only; not card data."
                           : "EM410x ID will be staged.");
    display.setCursor(8, 68);
    display.print("Use only with an authorised tag.");
    display.setCursor(8, 88);
    display.print("Enter: stage + emulate");
    ScreenChrome::drawFooter("Enter: confirm   Esc: cancel");
}
