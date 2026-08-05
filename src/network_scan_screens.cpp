#include "network_scan_screens.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <algorithm>

#include "branding.h"
#include "screen_chrome.h"

void NetworkScanScreens::drawWarDriveDynamic() {
    auto& display = M5Cardputer.Display;
    const int width = display.width();

    display.fillRect(0, 27, width, 13, Branding::background);
    display.setTextColor(warDrive_.isActive() ? Branding::accent
                                              : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.print(warDrive_.isActive() ? "ACTIVE" : "STOPPED");

    display.fillRect(0, 42, width, 13, Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 44);
    if (gnss_.hasFix()) {
        display.printf("Lat: %.6f  Lon: %.6f", gnss_.latitude(),
                       gnss_.longitude());
    } else {
        display.print("GPS: no fix yet");
    }

    display.fillRect(0, 56, width, 13, Branding::background);
    display.setCursor(8, 58);
    display.printf("Phase: %s", warDrive_.currentPhaseName());

    display.fillRect(0, 70, width, 13, Branding::background);
    display.setCursor(8, 72);
    display.printf("Unique APs: %lu",
                   static_cast<unsigned long>(warDrive_.wifiUniqueCount()));

    display.fillRect(0, 84, width, 13, Branding::background);
    display.setCursor(8, 86);
    display.printf("Unique devices: %lu",
                   static_cast<unsigned long>(warDrive_.bleUniqueCount()));
}

void NetworkScanScreens::drawWarDrive() {
    ScreenChrome::drawHeader("War Drive");
    drawWarDriveDynamic();
    ScreenChrome::drawFooter("R: start/stop   Backspace/Q: back");
}

void NetworkScanScreens::drawNetworkDashboard() {
    ScreenChrome::drawHeader("Network Dashboard");
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);
    if (WiFi.status() != WL_CONNECTED) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 36);
        display.print("Wi-Fi is not connected");
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 55);
        display.print("Use Wi-Fi > Connect first.");
        ScreenChrome::drawFooter("R: refresh   Q: back");
        return;
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 28);
    display.printf("SSID: %s  %d dBm", WiFi.SSID().c_str(), WiFi.RSSI());
    display.setCursor(8, 44);
    display.printf("IP:   %s", WiFi.localIP().toString().c_str());
    display.setCursor(8, 60);
    display.printf("GW:   %s", WiFi.gatewayIP().toString().c_str());
    display.setCursor(8, 76);
    display.printf("Mask: %s", WiFi.subnetMask().toString().c_str());
    display.setCursor(8, 92);
    display.printf("DNS:  %s", WiFi.dnsIP().toString().c_str());
    display.setCursor(8, 108);
    display.printf("MAC:  %s", WiFi.macAddress().c_str());
    ScreenChrome::drawFooter("R: refresh   Q: back");
}

void NetworkScanScreens::drawNetworkHostScan(bool fullDraw) {
    const uint32_t signature =
        (static_cast<uint32_t>(WiFi.status()) << 28) ^
        (static_cast<uint32_t>(hostScan_.isActive()) << 27) ^
        (static_cast<uint32_t>(hostScan_.scannedCount()) << 12) ^
        static_cast<uint32_t>(hostResults_.size());
    if (!fullDraw && signature == lastHostScanSignature_) return;
    lastHostScanSignature_ = signature;
    ScreenChrome::beginContentUpdate("Host Discovery", fullDraw);
    auto& display = M5Cardputer.Display;
    if (WiFi.status() != WL_CONNECTED) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 36);
        display.print("Connect to Wi-Fi first");
        ScreenChrome::drawFooter("Wi-Fi > Connect   Backspace/Q: back");
        return;
    }
    ScreenChrome::normalizeListPosition(hostResults_.size());
    if (!hostScan_.isActive()) {
        ScreenChrome::drawHeaderPosition(listSelection_ + 1, hostResults_.size());
    }
    if (hostResults_.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 36);
        if (hostScan_.isActive()) {
            display.printf("Scanning... %u/%u",
                           static_cast<unsigned>(hostScan_.scannedCount()),
                           static_cast<unsigned>(hostScan_.totalCount()));
        } else {
            display.print("No hosts found. Press R to scan.");
        }
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < hostResults_.size();
            ++row) {
            const auto& host = hostResults_[row + listOffset_];
            ScreenChrome::drawListRow(row, host.ip.toString(),
                                      row + listOffset_ == listSelection_);
        }
    }
    String footer;
    if (!hostScanExportStatus_.isEmpty()) {
        footer = hostScanExportStatus_;
    } else if (hostScan_.isActive()) {
        footer = "Scanning " + String(hostScan_.scannedCount()) + "/" +
                 String(hostScan_.totalCount()) +
                 "  Found: " + String(hostResults_.size());
    } else {
        footer = hostResults_.empty() ? "R: start/stop  Backspace/Q: back"
                                      : "Enter: scan  Tab: actions  Q: back";
    }
    ScreenChrome::drawFooter(footer.c_str());
}

void NetworkScanScreens::drawNetworkPortScan(bool fullDraw) {
    const uint32_t signature =
        (static_cast<uint32_t>(portScan_.isActive()) << 31) ^
        (static_cast<uint32_t>(portScan_.scannedCount()) << 12) ^
        static_cast<uint32_t>(portResults_.size());
    if (!fullDraw && signature == lastPortScanSignature_) return;
    lastPortScanSignature_ = signature;
    const String title = "Port Scan " + portScanTarget_.toString();
    ScreenChrome::beginContentUpdate(title.c_str(), fullDraw);
    auto& display = M5Cardputer.Display;
    const bool fullActive = portScanIsFull_ && portScan_.isActive();
    if (portResults_.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 36);
        if (fullActive) {
            display.printf("Scanning... %u/%u",
                           static_cast<unsigned>(portScan_.scannedCount()),
                           static_cast<unsigned>(portScan_.totalCount()));
        } else {
            display.print("No open ports found");
        }
    } else {
        ScreenChrome::normalizeListPosition(portResults_.size());
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < portResults_.size();
            ++row) {
            ScreenChrome::drawListRow(row, String(portResults_[row + listOffset_]),
                                      row + listOffset_ == listSelection_);
        }
    }
    String footer;
    if (!portScanExportStatus_.isEmpty()) {
        footer = portScanExportStatus_;
    } else if (fullActive) {
        footer = "Scanning " + String(portScan_.scannedCount()) + "/" +
                 String(portScan_.totalCount()) +
                 "  Found: " + String(portResults_.size()) + "  Q: stop";
    } else {
        footer = "R: rescan  Tab: menu  Q: back";
    }
    ScreenChrome::drawFooter(footer.c_str());
}
