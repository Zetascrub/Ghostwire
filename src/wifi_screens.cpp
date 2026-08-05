#include "wifi_screens.h"

#include <M5Cardputer.h>
#include <algorithm>
#include <climits>

#include "branding.h"
#include "screen_chrome.h"

const char* WifiScreens::authName(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA+2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2+3";
        default: return "SEC";
    }
}

void WifiScreens::drawRecon() {
    ScreenChrome::drawHeader("Wi-Fi Discovery");
    ScreenChrome::normalizeListPosition(accessPoints_.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, accessPoints_.size());
    if (accessPoints_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print(wifiStatus_);
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < accessPoints_.size();
            ++row) {
            const auto& ap = accessPoints_[row + listOffset_];
            String ssid = reinterpret_cast<const char*>(ap.ssid);
            if (ssid.isEmpty()) ssid = "<hidden>";
            String suffix = String(ap.primary) + "/" + String(ap.rssi);
            ScreenChrome::drawListRow(row, ssid, row + listOffset_ == listSelection_,
                                      suffix);
        }
    }
    ScreenChrome::drawFooter(
        wifiExportStatus_.isEmpty()
            ? "R: scan  Enter: details  Tab: actions  Q: back"
            : wifiExportStatus_.c_str());
}

void WifiScreens::drawChannelAnalyzer() {
    ScreenChrome::drawHeader("2.4 GHz Channels");
    auto& display = M5Cardputer.Display;
    uint8_t counts[14]{};
    int strongest[14];
    for (int channel = 0; channel < 14; ++channel) strongest[channel] = -100;
    for (const auto& ap : accessPoints_) {
        if (ap.primary < 1 || ap.primary > 13) continue;
        ++counts[ap.primary];
        strongest[ap.primary] =
            std::max(strongest[ap.primary], static_cast<int>(ap.rssi));
    }

    int bestChannel = 1;
    int bestScore = INT_MAX;
    static constexpr int candidates[] = {1, 6, 11};
    for (int candidate : candidates) {
        int score = 0;
        for (int channel = 1; channel <= 13; ++channel) {
            const int distance = abs(candidate - channel);
            if (distance > 4) continue;
            const int signal = strongest[channel] <= -100
                                   ? 0
                                   : std::max(1, strongest[channel] + 101);
            score += counts[channel] * signal * (5 - distance);
        }
        if (score < bestScore) {
            bestScore = score;
            bestChannel = candidate;
        }
    }

    const int baseline = 103;
    const int barWidth = 13;
    const int gap = 4;
    const int startX = 9;
    display.drawFastHLine(startX, baseline, 13 * (barWidth + gap) - gap,
                          Branding::muted);
    for (int channel = 1; channel <= 13; ++channel) {
        const int x = startX + (channel - 1) * (barWidth + gap);
        const int height = std::min(62, static_cast<int>(counts[channel]) * 9);
        const uint16_t colour = channel == bestChannel
                                    ? Branding::accent
                                    : (counts[channel] >= 4 ? Branding::warning
                                                            : Branding::text);
        if (height > 0) {
            display.fillRect(x, baseline - height, barWidth, height, colour);
        }
        display.setTextColor(
            channel == bestChannel ? Branding::accent : Branding::muted,
            Branding::background);
        display.setCursor(x + (channel < 10 ? 4 : 1), baseline + 4);
        display.print(channel);
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 27);
    if (accessPoints_.empty()) {
        display.print("No scan data - press R");
    } else {
        display.printf("%u APs  Suggested channel: %d",
                       static_cast<unsigned>(accessPoints_.size()), bestChannel);
    }
    ScreenChrome::drawFooter("R: rescan   Q: back");
}

void WifiScreens::drawDetail() {
    if (accessPoints_.empty() || listSelection_ >= accessPoints_.size()) {
        currentScreen_ = Screen::WifiRecon;
        drawRecon();
        return;
    }
    const auto& ap = accessPoints_[listSelection_];
    ScreenChrome::drawHeader("Access Point");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 32);
    display.printf("SSID: %s",
                   ap.ssid[0] ? reinterpret_cast<const char*>(ap.ssid)
                              : "<hidden>");
    display.setCursor(8, 50);
    display.printf("Channel: %u   RSSI: %d", ap.primary, ap.rssi);
    display.setCursor(8, 68);
    display.printf("Security: %s", authName(ap.authmode));
    display.setCursor(8, 86);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0],
                   ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
                   ap.bssid[5]);
    ScreenChrome::drawFooter(wifiDeauthStatus_.isEmpty()
                                 ? "Tab: actions   Esc: back"
                                 : wifiDeauthStatus_.c_str());
}

void WifiScreens::drawDeauthConfirm() {
    if (accessPoints_.empty() || listSelection_ >= accessPoints_.size()) {
        currentScreen_ = Screen::WifiRecon;
        drawRecon();
        return;
    }
    const auto& ap = accessPoints_[listSelection_];
    String ssid = reinterpret_cast<const char*>(ap.ssid);
    if (ssid.isEmpty()) ssid = "<hidden>";
    ScreenChrome::drawHeader("Deauth AP?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 32);
    display.print(ssid.substring(0, 30));
    display.setCursor(8, 50);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0],
                   ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
                   ap.bssid[5]);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 72);
    display.print("Sends deauth frames to this AP.");
    display.setCursor(8, 88);
    display.print("Authorized targets only.");
    ScreenChrome::drawFooter("Enter: DEAUTH   Backspace/Q: cancel");
}

void WifiScreens::drawHandshakeCapture(bool fullDraw) {
    if (accessPoints_.empty() || listSelection_ >= accessPoints_.size()) {
        currentScreen_ = Screen::WifiRecon;
        drawRecon();
        return;
    }
    const auto& ap = accessPoints_[listSelection_];
    String ssid = reinterpret_cast<const char*>(ap.ssid);
    if (ssid.isEmpty()) ssid = "<hidden>";
    ScreenChrome::beginContentUpdate("Handshake Capture", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(
        wifiSniffer_.isActive() ? Branding::accent : Branding::warning,
        Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  CH %u", ssid.substring(0, 20).c_str(), ap.primary);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0],
                   ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
                   ap.bssid[5]);
    display.setCursor(8, 63);
    display.printf("EAPOL frames: %lu",
                   static_cast<unsigned long>(handshakeEapolFrameCount_));

    display.setCursor(8, 80);
    display.setTextColor(Branding::text, Branding::background);
    display.print("Messages: ");
    for (uint8_t message = 1; message <= 4; ++message) {
        display.setTextColor(handshakeMessageSeen_[message] ? Branding::accent
                                                             : Branding::muted,
                             Branding::background);
        display.printf("M%u ", message);
    }

    display.setCursor(8, 97);
    if (handshakePmkidFound_) {
        String pmkidHex;
        pmkidHex.reserve(32);
        for (uint8_t index = 0; index < 16; ++index) {
            char byteText[3];
            snprintf(byteText, sizeof(byteText), "%02X", handshakePmkid_[index]);
            pmkidHex += byteText;
        }
        display.setTextColor(Branding::accent, Branding::background);
        display.printf("PMKID: %s", pmkidHex.c_str());
    } else {
        display.setTextColor(Branding::muted, Branding::background);
        display.print("PMKID: none yet");
    }

    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 114);
    if (handshakeCaptureLogger_.isActive()) {
        display.printf(
            "REC %lu frames",
            static_cast<unsigned long>(handshakeCaptureLogger_.rowCount()));
    } else {
        display.print("Not recording");
    }
    if (fullDraw) ScreenChrome::drawFooter("R: restart   Tab: actions   Q: stop");
}

void WifiScreens::drawConnectSelect() {
    ScreenChrome::drawHeader("Wi-Fi Connect");
    ScreenChrome::normalizeListPosition(accessPoints_.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, accessPoints_.size());
    if (accessPoints_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print(wifiStatus_);
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < accessPoints_.size();
            ++row) {
            const auto& ap = accessPoints_[row + listOffset_];
            String ssid = reinterpret_cast<const char*>(ap.ssid);
            if (ssid.isEmpty()) ssid = "<hidden>";
            String suffix = String(ap.primary) + "/" + String(ap.rssi);
            ScreenChrome::drawListRow(row, ssid, row + listOffset_ == listSelection_,
                                      suffix);
        }
    }
    ScreenChrome::drawFooter(
        wifiConnectSavedSsid_.isEmpty()
            ? "R: rescan   Enter: select   Backspace/Q: back"
            : "R: rescan  Enter: select  Tab: actions  Q: back");
}

void WifiScreens::drawConnectPassword() {
    ScreenChrome::drawHeader("Wi-Fi Connect");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 36);
    display.printf("SSID: %s", wifiConnectSsid_.c_str());
    ScreenChrome::drawTextEntryRow(54, "Password: ", wifiConnectPasswordInput_,
                                   true);
    ScreenChrome::drawFooter("Enter: connect   Esc: cancel");
}

void WifiScreens::drawConnectStatus(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Wi-Fi Connect", fullDraw);
    auto& display = M5Cardputer.Display;
    const bool connected = WiFi.status() == WL_CONNECTED;
    display.setTextColor(connected ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("SSID: %s", wifiConnectSsid_.c_str());
    display.setCursor(8, 44);
    display.print(wifiConnectStatusText_);
    display.setTextColor(Branding::text, Branding::background);
    if (connected) {
        display.setCursor(8, 58);
        display.printf("IP: %s", WiFi.localIP().toString().c_str());
        display.setCursor(8, 72);
        display.printf("Gateway: %s", WiFi.gatewayIP().toString().c_str());
        display.setCursor(8, 86);
        display.printf("RSSI: %d dBm", WiFi.RSSI());
    }
    ScreenChrome::drawFooter("Tab: actions   Backspace/Q: back");
}
