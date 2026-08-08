#include "menu_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

const char* const MenuScreens::kMenuItems[] = {
    "My Familiar", "Observe signals", "Scout network", "Evidence",
    "Field kit", "Settings",
};

void MenuScreens::drawMain() {
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Your companion and scout", "Watch the air around you",
            "Explore a connected network", "Review what Ghostwire saved",
            "Accessories, notes and tools", "Make the deck feel like yours",
        };
        String badge;
        if (menuSelection_ == 0) {
            badge = "LEVEL " + String(familiar_.level());
        } else if (menuSelection_ == 1 && wifiGuardian_.isActive()) {
            badge = "WATCHING";
        } else if (menuSelection_ == 2 && patrol_.isActive()) {
            badge = "PATROL LIVE";
        } else if (menuSelection_ == 3) {
            badge = sdAvailable_ ? "SD READY" : "NO SD";
        }
        const String title =
            String(Branding::productName) + " // " + familiar_.name();
        ScreenChrome::drawNavigationCard(
            title.c_str(), kMenuItems[menuSelection_],
            descriptions[menuSelection_], menuSelection_, kMenuCount,
            static_cast<uint8_t>(menuSelection_), badge);
        return;
    }
    const String title =
        String(Branding::productName) + " // " + familiar_.name();
    ScreenChrome::drawHeader(title.c_str());
    ScreenChrome::drawHeaderPosition(menuSelection_ + 1, kMenuCount);
    const size_t offset = menuSelection_ >= ScreenChrome::kVisibleRows
                              ? menuSelection_ - ScreenChrome::kVisibleRows + 1
                              : 0;
    for (size_t row = 0;
        row < ScreenChrome::kVisibleRows && row + offset < kMenuCount;
        ++row) {
        const size_t item = row + offset;
        String suffix;
        if (item == 0) {
            suffix = "Lv" + String(familiar_.level());
        } else if (item == 1 && wifiGuardian_.isActive()) {
            suffix = "WATCH";
        } else if (item == 2 && patrol_.isActive()) {
            suffix = "LIVE";
        } else if (item == 3) {
            suffix = sdAvailable_ ? "SD" : "NO SD";
        }
        ScreenChrome::drawListRow(row, kMenuItems[item], item == menuSelection_,
                                  suffix);
    }
    ScreenChrome::drawFooter("Choose a mission   Enter: open");
}

void MenuScreens::drawObserve() {
    static const char* const items[] = {
        "Wi-Fi airspace", "Bluetooth nearby", "Position / GPS",
        "Mesh signals", "War drive survey",
    };
    constexpr size_t count = sizeof(items) / sizeof(items[0]);
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Discover networks and watch Wi-Fi",
            "Inspect nearby Bluetooth devices",
            "Find position and record a trail",
            "Listen to LoRa and mesh traffic",
            "Survey Wi-Fi, BLE and position",
        };
        static constexpr uint8_t icons[] = {1, 6, 7, 1, 7};
        String badge;
        if (listSelection_ == 0 && wifiGuardian_.isActive()) badge = "WATCHING";
        if (listSelection_ == 4 && warDrive_.isActive()) badge = "SURVEY LIVE";
        ScreenChrome::drawNavigationCard(
            "Observe signals", items[listSelection_],
            descriptions[listSelection_], listSelection_, count,
            icons[listSelection_], badge);
        return;
    }
    ScreenChrome::drawHeader("Observe signals");
    ScreenChrome::normalizeListPosition(count);
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, count);
    for (size_t row = 0;
        row < ScreenChrome::kVisibleRows && row + listOffset_ < count; ++row) {
        const size_t item = row + listOffset_;
        String suffix;
        if (item == 0 && wifiGuardian_.isActive()) suffix = "WATCH";
        if (item == 4 && warDrive_.isActive()) suffix = "LIVE";
        ScreenChrome::drawListRow(row, items[item], item == listSelection_,
                                  suffix);
    }
    ScreenChrome::drawFooter("Observe first. Save what matters.");
}

void MenuScreens::drawFieldKit() {
    static const char* const items[] = {
        "Connected devices", "AI field notes", "Utility tools",
    };
    constexpr size_t count = sizeof(items) / sizeof(items[0]);
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Work with supported accessories", "Capture and develop field notes",
            "Deck diagnostics and utilities",
        };
        static constexpr uint8_t icons[] = {4, 8, 9};
        ScreenChrome::drawNavigationCard(
            "Field kit", items[listSelection_], descriptions[listSelection_],
            listSelection_, count, icons[listSelection_]);
        return;
    }
    ScreenChrome::drawHeader("Field kit");
    ScreenChrome::normalizeListPosition(count);
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, count);
    for (size_t row = 0; row < count; ++row) {
        ScreenChrome::drawListRow(row, items[row], row == listSelection_);
    }
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 82);
    display.print("Accessories, notes and deck utilities.");
    ScreenChrome::drawFooter("Enter: open   Q: missions");
}

void MenuScreens::drawWifi() {
    static const char* const items[] = {
        "Discovery", "Channel Analyzer", "Sniffer", "Guardian", "Connect",
        "Network Profiles",
    };
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Find nearby access points", "Compare channel congestion",
            "Passively inspect Wi-Fi traffic", "Watch for disruption bursts",
            "Join a network for scouting", "Connect or manage saved networks",
        };
        String badge = listSelection_ == 3 && wifiGuardian_.isActive()
                           ? "WATCHING" : "";
        ScreenChrome::drawNavigationCard("Wi-Fi airspace", items[listSelection_],
                                         descriptions[listSelection_],
                                         listSelection_, 6, 1, badge);
        return;
    }
    ScreenChrome::drawHeader("Wi-Fi");
    ScreenChrome::normalizeListPosition(6);
    for (size_t row = 0; row < 6; ++row) {
        ScreenChrome::drawListRow(row, items[row], row == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawBle() {
    static const char* const items[] = {
        "Advertisement Sniffer", "BLE Keyboard", "Spam",
    };
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Inspect nearby advertisements", "Use the deck as a BLE keyboard",
            "Experimental pairing broadcasts",
        };
        ScreenChrome::drawNavigationCard("Bluetooth nearby", items[listSelection_],
                                         descriptions[listSelection_],
                                         listSelection_, 3, 6);
        return;
    }
    ScreenChrome::drawHeader("BLE");
    ScreenChrome::normalizeListPosition(3);
    for (size_t row = 0; row < 3; ++row) {
        ScreenChrome::drawListRow(row, items[row], row == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawDevices() {
    static const char* const items[] = {"Biscuit Pro", "Chameleon Ultra"};
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Connect to a Biscuit Pro", "Read and manage test identities",
        };
        ScreenChrome::drawNavigationCard("Connected devices", items[listSelection_],
                                         descriptions[listSelection_],
                                         listSelection_, 2, 4);
        return;
    }
    ScreenChrome::drawHeader("Devices");
    ScreenChrome::normalizeListPosition(2);
    for (size_t row = 0; row < 2; ++row) {
        ScreenChrome::drawListRow(row, items[row], row == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawRfid() {
    static const char* const items[] = {"Chameleon Ultra"};
    ScreenChrome::drawHeader("RFID");
    ScreenChrome::normalizeListPosition(1);
    ScreenChrome::drawListRow(0, items[0], listSelection_ == 0);
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawGps() {
    static const char* const items[] = {"GNSS Monitor"};
    if (cardNav_) {
        ScreenChrome::drawNavigationCard("Position / GPS", items[0],
                                         "Monitor fixes and record a trail",
                                         0, 1, 7);
        return;
    }
    ScreenChrome::drawHeader("GPS");
    ScreenChrome::normalizeListPosition(1);
    ScreenChrome::drawListRow(0, items[0], listSelection_ == 0);
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawMesh() {
    static const char* const items[] = {"Chats", "Nodes", "Map", "Settings"};
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Channels and direct conversations", "People and devices on the mesh",
            "Known positions around you", "Identity, channels and radio"};
        ScreenChrome::drawNavigationCard("Meshtastic", items[listSelection_],
                                         descriptions[listSelection_],
                                         listSelection_, 4, 1);
        return;
    }
    ScreenChrome::drawHeader("Meshtastic");
    ScreenChrome::normalizeListPosition(4);
    for (size_t index = 0; index < 4; ++index) {
        ScreenChrome::drawListRow(index, items[index], listSelection_ == index);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawNetwork() {
    static const char* const items[] = {
        "Network Dashboard",
        "Host Discovery",
        "Telnet Client",
        "SSH Client",
    };
    constexpr size_t kNetworkMenuCount = sizeof(items) / sizeof(items[0]);
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Review the current network", "Find responsive local hosts",
            "Open a plain-text session", "Open a secure shell session",
        };
        String badge = patrol_.isActive() ? "PATROL LIVE" : "";
        ScreenChrome::drawNavigationCard(
            "Scout network", items[listSelection_], descriptions[listSelection_],
            listSelection_, kNetworkMenuCount, 2, badge);
        return;
    }
    ScreenChrome::drawHeader("Network");
    ScreenChrome::normalizeListPosition(kNetworkMenuCount);
    for (size_t row = 0; row < kNetworkMenuCount; ++row) {
        ScreenChrome::drawListRow(row, items[row], row == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawTools() {
    static const char* const items[] = {
        "Infrared",     "USB / HID", "Audio",  "Logs / Sessions",
        "Motion / IMU", "Files",     "QR Generator",
    };
    constexpr size_t kToolsCount = sizeof(items) / sizeof(items[0]);
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Test the infrared transmitter", "Keyboard and guarded scripts",
            "Speaker, microphone and phrases", "Browse saved log sessions",
            "Inspect motion sensor data", "Browse the microSD card",
            "Create an offline QR code",
        };
        ScreenChrome::drawNavigationCard("Utility tools", items[listSelection_],
                                         descriptions[listSelection_],
                                         listSelection_, kToolsCount, 9);
        return;
    }
    ScreenChrome::drawHeader("Tools");
    ScreenChrome::normalizeListPosition(kToolsCount);
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, kToolsCount);
    for (size_t row = 0;
        row < ScreenChrome::kVisibleRows && row + listOffset_ < kToolsCount;
        ++row) {
        ScreenChrome::drawListRow(row, items[row + listOffset_],
                                  row + listOffset_ == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}

void MenuScreens::drawSettings() {
    static const char* const items[] = {
        "Display & Audio", "Boot Experience", "Connectivity", "Familiar LED",
        "System", "Firmware Update", "About Ghostwire",
        "Replay Introduction", "Restore Defaults",
    };
    constexpr size_t count = sizeof(items) / sizeof(items[0]);
    if (cardNav_) {
        static const char* const descriptions[] = {
            "Theme, sound and navigation", "Animation, sound and boot speed",
            "Saved Wi-Fi connection options",
            "Colour alerts for Familiar events", "Inspect deck health and time",
            "Check for a signed release", "Version and project identity",
            "Review the Ghostwire field guide",
            "Return preferences to defaults",
        };
        ScreenChrome::drawNavigationCard("Settings", items[listSelection_],
                                         descriptions[listSelection_],
                                         listSelection_, count, 5);
        return;
    }
    ScreenChrome::drawHeader("Settings");
    ScreenChrome::normalizeListPosition(count);
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, count);
    for (size_t row = 0;
         row < ScreenChrome::kVisibleRows && row + listOffset_ < count; ++row) {
        const size_t item = row + listOffset_;
        ScreenChrome::drawListRow(row, items[item], item == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open   Backspace/Q: back");
}
