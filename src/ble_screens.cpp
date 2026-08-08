#include "ble_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

void BleScreens::drawDiscovery() {
    ScreenChrome::drawHeader("BLE Advertisement Sniffer");
    ScreenChrome::normalizeListPosition(devices_.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, devices_.size());
    if (devices_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print(bleStatus_);
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < devices_.size();
            ++row) {
            const BleDeviceInfo& device = devices_[row + listOffset_];
            ScreenChrome::drawListRow(row, device.name,
                                      row + listOffset_ == listSelection_,
                                      String(device.rssi));
        }
    }
    if (scanner_.isContinuous()) {
        ScreenChrome::drawFooter(("Tab: actions LIVE:" +
                                  String(scanner_.advertisementCount()) + " D:" +
                                  String(scanner_.droppedCount())).c_str());
    } else {
        ScreenChrome::drawFooter(bleExportStatus_.isEmpty()
                                     ? "R:scan Enter:details Tab:menu Q:back"
                                     : bleExportStatus_.c_str());
    }
}

void BleScreens::drawDetail() {
    if (devices_.empty() || listSelection_ >= devices_.size()) {
        currentScreen_ = Screen::BleDiscovery;
        drawDiscovery();
        return;
    }
    const BleDeviceInfo& device = devices_[listSelection_];
    ScreenChrome::drawHeader("BLE Device");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 27);
    display.printf("Name: %s", device.name.substring(0, 31).c_str());
    display.setCursor(8, 42);
    display.printf("%s  %d dBm", device.address.c_str(), device.rssi);
    display.setCursor(8, 57);
    display.printf("Addr type: %u  ADV: %u  %s", device.addressType,
                   device.advertisementType,
                   device.connectable ? "CONNECT" : "BEACON");
    display.setCursor(8, 72);
    display.printf("Mfr: %s", device.manufacturer.c_str());
    display.setCursor(8, 87);
    display.printf("Services (%u): %s", device.serviceCount,
                   device.service.isEmpty()
                       ? "none"
                       : device.service.substring(0, 22).c_str());
    display.setCursor(8, 102);
    display.printf("Payload %uB: %s", device.payloadLength,
                   device.payloadData.isEmpty()
                       ? "not available"
                       : device.payloadData.substring(0, 24).c_str());
    ScreenChrome::drawFooter("Backspace/Q: results");
}

void BleScreens::drawKeyboard(bool fullDraw) {
    const uint32_t signature =
        (static_cast<uint32_t>(keyboard_.isActive()) << 31) ^
        (static_cast<uint32_t>(keyboard_.isConnected()) << 30) ^
        keyboard_.charactersSent();
    if (!fullDraw && signature == lastKeyboardSignature_) return;
    lastKeyboardSignature_ = signature;
    ScreenChrome::beginContentUpdate("BLE Keyboard", fullDraw);
    auto& display = M5Cardputer.Display;
    const bool connected = keyboard_.isConnected();
    display.setTextColor(connected ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 30);
    if (!keyboard_.isActive()) {
        display.print("STOPPED");
    } else if (connected) {
        display.print("CONNECTED - LIVE INPUT");
    } else {
        display.print("ADVERTISING / WAITING");
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 49);
    display.print("Device: Ghostwire Keyboard");
    display.setCursor(8, 67);
    display.printf("Characters sent: %lu",
                   static_cast<unsigned long>(keyboard_.charactersSent()));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 86);
    display.print(keyboard_.isActive()
                      ? "Typed keys go to the paired host."
                      : "Enter starts pairing/advertising.");
    display.setCursor(8, 102);
    display.print("Use only on a device you control.");
    ScreenChrome::drawFooter(keyboard_.isActive()
                                 ? "Esc: stop/disconnect"
                                 : "Enter: start   Esc: back");
}
