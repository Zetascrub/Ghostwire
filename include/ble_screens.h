#pragma once

#include <vector>

#include "app_screen.h"
#include "ble_keyboard_service.h"
#include "ble_scanner.h"

// BLE discovery/detail and BLE-keyboard screens: grouped as one module (see
// docs/screen-extraction.md) since Discovery/Detail share the bleDevices
// scan-results list and list cursor, and BleKeyboard is small enough not to
// warrant its own file. drawDetail() redirects back to drawDiscovery()/
// Screen::BleDiscovery if the selection goes stale, same as WifiScreens.
class BleScreens {
public:
    BleScreens(std::vector<BleDeviceInfo>& devices, size_t& listSelection,
              size_t& listOffset, Screen& currentScreen, String& bleStatus,
              String& bleExportStatus, BleScanner& scanner,
              BleKeyboardService& keyboard)
        : devices_(devices),
          listSelection_(listSelection),
          listOffset_(listOffset),
          currentScreen_(currentScreen),
          bleStatus_(bleStatus),
          bleExportStatus_(bleExportStatus),
          scanner_(scanner),
          keyboard_(keyboard) {}

    void drawDiscovery();
    void drawDetail();
    void drawKeyboard(bool fullDraw = true);

private:
    std::vector<BleDeviceInfo>& devices_;
    size_t& listSelection_;
    size_t& listOffset_;
    Screen& currentScreen_;
    String& bleStatus_;
    String& bleExportStatus_;
    BleScanner& scanner_;
    BleKeyboardService& keyboard_;
    uint32_t lastKeyboardSignature_ = UINT32_MAX;
};
