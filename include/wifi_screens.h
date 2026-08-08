#pragma once

#include <WiFi.h>
#include <vector>

#include "app_screen.h"
#include "pcap_logger.h"
#include "wifi_sniffer_service.h"
#include "wifi_profile.h"

// Wi-Fi discovery/connect flow screens: scan results, channel analyzer,
// per-AP detail, deauth confirmation, handshake capture, and the Connect
// picker/password/status trio. Grouped as one module (see
// docs/screen-extraction.md) since they all share the same accessPoints
// scan-results list and list cursor.
//
// A few of these (drawDetail, drawDeauthConfirm, drawHandshakeCapture)
// redirect back to drawRecon() and Screen::WifiRecon if accessPoints became
// empty or the selection went stale (e.g. a rescan happened elsewhere) --
// that's existing behavior, preserved here as a same-class sibling call
// rather than main.cpp reaching back in.
class WifiScreens {
public:
    WifiScreens(std::vector<wifi_ap_record_t>& accessPoints,
               size_t& listSelection, size_t& listOffset, Screen& currentScreen,
               String& wifiStatus, String& wifiExportStatus,
               String& wifiDeauthStatus, WifiSnifferService& wifiSniffer,
               uint32_t& handshakeEapolFrameCount, bool (&handshakeMessageSeen)[5],
               bool& handshakePmkidFound, uint8_t (&handshakePmkid)[16],
               PcapLogger& handshakeCaptureLogger, String& wifiConnectSavedSsid,
               String& wifiConnectSsid, String& wifiConnectPasswordInput,
               String& wifiConnectStatusText,
               std::vector<WifiProfile>& wifiProfiles,
               size_t& activeWifiProfile, String& wifiProfileStatus,
               String& wifiProfileNameInput)
        : accessPoints_(accessPoints),
          listSelection_(listSelection),
          listOffset_(listOffset),
          currentScreen_(currentScreen),
          wifiStatus_(wifiStatus),
          wifiExportStatus_(wifiExportStatus),
          wifiDeauthStatus_(wifiDeauthStatus),
          wifiSniffer_(wifiSniffer),
          handshakeEapolFrameCount_(handshakeEapolFrameCount),
          handshakeMessageSeen_(handshakeMessageSeen),
          handshakePmkidFound_(handshakePmkidFound),
          handshakePmkid_(handshakePmkid),
          handshakeCaptureLogger_(handshakeCaptureLogger),
          wifiConnectSavedSsid_(wifiConnectSavedSsid),
          wifiConnectSsid_(wifiConnectSsid),
          wifiConnectPasswordInput_(wifiConnectPasswordInput),
          wifiConnectStatusText_(wifiConnectStatusText),
          wifiProfiles_(wifiProfiles),
          activeWifiProfile_(activeWifiProfile),
          wifiProfileStatus_(wifiProfileStatus),
          wifiProfileNameInput_(wifiProfileNameInput) {}

    void drawRecon();
    void drawChannelAnalyzer();
    void drawDetail();
    void drawDeauthConfirm();
    void drawHandshakeCapture(bool fullDraw = true);
    void drawConnectSelect();
    void drawConnectPassword();
    void drawConnectStatus(bool fullDraw = true);
    void drawProfiles();
    void drawProfileRename();
    void drawProfileDeleteConfirm();

    // Single source of truth for the auth-mode label shown on-screen and
    // used in a couple of main.cpp's CSV exports.
    static const char* authName(wifi_auth_mode_t mode);

private:
    std::vector<wifi_ap_record_t>& accessPoints_;
    size_t& listSelection_;
    size_t& listOffset_;
    Screen& currentScreen_;
    String& wifiStatus_;
    String& wifiExportStatus_;
    String& wifiDeauthStatus_;
    WifiSnifferService& wifiSniffer_;
    uint32_t& handshakeEapolFrameCount_;
    bool (&handshakeMessageSeen_)[5];
    bool& handshakePmkidFound_;
    uint8_t (&handshakePmkid_)[16];
    PcapLogger& handshakeCaptureLogger_;
    String& wifiConnectSavedSsid_;
    String& wifiConnectSsid_;
    String& wifiConnectPasswordInput_;
    String& wifiConnectStatusText_;
    std::vector<WifiProfile>& wifiProfiles_;
    size_t& activeWifiProfile_;
    String& wifiProfileStatus_;
    String& wifiProfileNameInput_;
};
