#include "network_scan_screens.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <algorithm>
#include <cstring>

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

namespace {
// Shared by the summary and detail screens: Grove carries the same live
// telemetry as the network status fetch (see GroveCompanionLink's
// status/identity frames) and takes priority whenever it's fresh -- it's
// faster (~1 Hz vs. the network's 10 s poll) and needs no Wi-Fi. Network
// stays the fallback source, and stays the only source for fields Grove
// doesn't carry (gateway/DNS/port).
bool poeUseGrove(const GroveCompanionLink& groveLink) {
    return groveLink.statusFresh();
}
bool poeHaveDetail(const PoeCompanionService& poeCompanion,
                   const GroveCompanionLink& groveLink) {
    const PoeCompanionState state = poeCompanion.state();
    return poeUseGrove(groveLink) || state == PoeCompanionState::Ready ||
           state == PoeCompanionState::Stale ||
           state == PoeCompanionState::Offline;
}

// FNV-1a, folded into the signatures below alongside the plain int/bool
// fields -- same purpose as lastHostScanSignature_ (drawNetworkHostScan):
// skip a periodic redraw entirely when nothing actually displayed would
// change, rather than repainting the content pane every time a fresh (but
// possibly identical) Grove/network sample arrives.
uint32_t hashString(const char* text) {
    uint32_t hash = 2166136261u;
    while (*text) {
        hash ^= static_cast<uint8_t>(*text++);
        hash *= 16777619u;
    }
    return hash;
}
}  // namespace

void NetworkScanScreens::drawPoeCompanion(bool fullDraw) {
    const PoeCompanionState state = poeCompanion_.state();
    const bool useGrove = poeUseGrove(groveLink_);
    const bool haveDetail = poeHaveDetail(poeCompanion_, groveLink_);
    const bool linkUp =
        haveDetail && (useGrove ? groveLink_.ethernetLinkUp() : poeCompanion_.ethernetLinkUp());
    const bool internetReachable =
        haveDetail &&
        (useGrove ? groveLink_.internetReachable() : poeCompanion_.internetReachable());
    const String statusText =
        useGrove ? String("Grove companion online") : poeCompanion_.statusMessage();
    // Grove carries the P4's DHCP-assigned IP directly, so it's available
    // even with no network path on the Cardputer's side at all -- fall back
    // to the network fetch's address, then note there's no source yet.
    const bool groveHasIp = useGrove && groveLink_.ip()[0] != '\0';
    String ipText;
    if (groveHasIp) {
        ipText = String("IP: ") + groveLink_.ip();
    } else if (!poeCompanion_.companionIp().isEmpty()) {
        ipText = "IP: " + poeCompanion_.companionIp() + ":" + String(poeCompanion_.port());
    } else {
        ipText = "IP: (no network path)";
    }

    // Grove drives a redraw roughly once a second while connected (see
    // updatePoeCompanionScreen() in main.cpp) -- skip repainting when
    // nothing above actually changed from the last draw, same pattern as
    // lastHostScanSignature_ (drawNetworkHostScan).
    const uint32_t signature = (static_cast<uint32_t>(state) << 24) ^
                               (static_cast<uint32_t>(useGrove) << 23) ^
                               (static_cast<uint32_t>(haveDetail) << 22) ^
                               (static_cast<uint32_t>(linkUp) << 21) ^
                               (static_cast<uint32_t>(internetReachable) << 20) ^
                               hashString(ipText.c_str()) ^
                               (hashString(statusText.c_str()) << 1);
    if (!fullDraw && signature == lastPoeCompanionSignature_) return;
    lastPoeCompanionSignature_ = signature;

    // This must repaint only the content pane on periodic updates -- a full
    // drawHeader() every redraw visibly flickers the header/footer chrome
    // for no reason.
    ScreenChrome::beginContentUpdate("Ghostwire Relay", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);

    display.setTextColor((useGrove || state == PoeCompanionState::Ready)
                             ? Branding::accent
                             : (state == PoeCompanionState::Discovering
                                    ? Branding::text
                                    : Branding::warning),
                         Branding::background);
    display.setCursor(8, 29);
    display.print(statusText);

    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 50);
    display.print(ipText);

    display.setCursor(8, 71);
    if (haveDetail) {
        display.printf("LAN %s   Internet %s", linkUp ? "UP" : "DOWN",
                       internetReachable ? "YES" : "NO");
    } else {
        display.print("LAN --   Internet --");
    }

    if (!poeLootExtractStatus_.isEmpty()) {
        ScreenChrome::drawFooter(poeLootExtractStatus_.c_str());
    } else {
        ScreenChrome::drawFooter("R: refresh   Tab: details   Q: back");
    }
}

void NetworkScanScreens::drawPoeCompanionDetail(bool fullDraw) {
    const bool useGrove = poeUseGrove(groveLink_);
    const bool haveDetail = poeHaveDetail(poeCompanion_, groveLink_);

    // Same field groupings the fixed-line layout used to have, just one
    // String per scrollable row instead of one printf per fixed y-position
    // -- drawListRow() safely truncates instead of running off the 240px
    // width the way a raw printf could. kMaxRows covers the larger of the
    // two branches below (8 for haveDetail, 4 otherwise).
    constexpr size_t kMaxRows = 8;
    String rows[kMaxRows];
    size_t rowCount = 0;
    uint32_t signature = 0;

    if (haveDetail) {
        const char* deviceId = (useGrove && groveLink_.identityFresh())
                                   ? groveLink_.deviceId()
                                   : poeCompanion_.deviceId().c_str();
        const char* firmware = (useGrove && groveLink_.identityFresh())
                                   ? groveLink_.firmwareVersion()
                                   : poeCompanion_.firmwareVersion().c_str();
        const uint64_t uptimeMs =
            useGrove ? groveLink_.uptimeMs() : poeCompanion_.relayUptimeMs();
        const unsigned long ageMs =
            useGrove ? groveLink_.statusAgeMs() : poeCompanion_.statusAgeMs();
        const uint32_t speedMbps =
            useGrove ? groveLink_.linkSpeedMbps() : poeCompanion_.linkSpeedMbps();
        const bool fullDuplex =
            useGrove ? groveLink_.fullDuplex() : poeCompanion_.fullDuplex();
        const bool hasTemperature =
            useGrove ? groveLink_.hasTemperature() : poeCompanion_.hasTemperature();
        const float temperatureC =
            useGrove ? groveLink_.temperatureC() : poeCompanion_.temperatureC();
        const uint32_t freeHeapBytes =
            useGrove ? groveLink_.freeHeapBytes() : poeCompanion_.freeHeapBytes();
        const uint32_t minimumFreeHeapBytes = useGrove
                                                  ? groveLink_.minimumFreeHeapBytes()
                                                  : poeCompanion_.minimumFreeHeapBytes();
        const char* indicatorState =
            useGrove ? groveLink_.indicatorState() : poeCompanion_.indicatorState().c_str();
        // "Ghostwire link" means the P4 has seen recent contact over the
        // network specifically; the Grove-sourced indicator reports the
        // same underlying condition as its "ghostwire" LED state.
        const bool ghostwireLink = useGrove
                                       ? strcmp(groveLink_.indicatorState(), "ghostwire") == 0
                                       : poeCompanion_.ghostwireConnected();
        // Command results ride regular status telemetry (Grove's S frame or
        // the network fetch's "payload" object), not a separate response --
        // same on both transports, matching how every other field here
        // already picks Grove-or-network via useGrove.
        const char* payloadState =
            useGrove ? groveLink_.payloadRunState() : poeCompanion_.payloadRunState();
        const size_t payloadFindings = useGrove ? groveLink_.payloadFindingCount()
                                                : poeCompanion_.payloadFindingCount();

        // Bucketing uptime to the nearest 5s (rather than excluding it, or
        // age, entirely) is what keeps this ticking-but-otherwise-idle
        // screen redrawing every ~5s instead of every ~1s -- age is left
        // out entirely since Grove keeps it near-zero on every fresh
        // sample anyway. Everything else here is a real, meaningful field.
        signature = (static_cast<uint32_t>(useGrove) << 31) ^
                    (static_cast<uint32_t>(fullDuplex) << 30) ^
                    (static_cast<uint32_t>(hasTemperature) << 29) ^
                    (static_cast<uint32_t>(ghostwireLink) << 28) ^
                    (static_cast<uint32_t>(groveLink_.connected()) << 27) ^
                    (static_cast<uint32_t>(poeCompanion_.groveConnected()) << 26) ^
                    (static_cast<uint32_t>(groveLink_.pairingState()) << 24) ^
                    (static_cast<uint32_t>(groveLink_.hasSessionKey()) << 23) ^
                    (static_cast<uint32_t>(uptimeMs / 5000) << 8) ^
                    (speedMbps << 1) ^
                    static_cast<uint32_t>(temperatureC * 10.0f) ^
                    freeHeapBytes ^ minimumFreeHeapBytes ^
                    hashString(deviceId) ^ hashString(firmware) ^
                    hashString(indicatorState) ^ hashString(payloadState) ^
                    (static_cast<uint32_t>(payloadFindings) << 4) ^
                    hashString(poeCompanion_.gateway().c_str()) ^
                    hashString(poeCompanion_.dns().c_str());
        if (!fullDraw && signature == lastPoeCompanionDetailSignature_) return;
        lastPoeCompanionDetailSignature_ = signature;

        char buffer[48];
        rows[rowCount++] = String("Device ") + deviceId;
        snprintf(buffer, sizeof(buffer), "FW %s  Up %llus Age %lus", firmware,
                uptimeMs / 1000ULL, ageMs / 1000UL);
        rows[rowCount++] = buffer;
        snprintf(buffer, sizeof(buffer), "Speed %luM %s",
                static_cast<unsigned long>(speedMbps), fullDuplex ? "FD" : "HD");
        rows[rowCount++] = buffer;
        rows[rowCount++] = String("GW ") + poeCompanion_.gateway() + "  DNS " +
                           poeCompanion_.dns();
        if (hasTemperature) {
            snprintf(buffer, sizeof(buffer), "CPU %.1fC  Heap %luK (min %luK)",
                    temperatureC, static_cast<unsigned long>(freeHeapBytes / 1024),
                    static_cast<unsigned long>(minimumFreeHeapBytes / 1024));
        } else {
            snprintf(buffer, sizeof(buffer), "CPU --  Heap %luK (min %luK)",
                    static_cast<unsigned long>(freeHeapBytes / 1024),
                    static_cast<unsigned long>(minimumFreeHeapBytes / 1024));
        }
        rows[rowCount++] = buffer;
        snprintf(buffer, sizeof(buffer), "LED %s GW %s", indicatorState,
                ghostwireLink ? "LINK" : "IDLE");
        rows[rowCount++] = buffer;
        snprintf(buffer, sizeof(buffer), "Grove %s/%s [%s]",
                groveLink_.connected() ? "RX" : "--",
                poeCompanion_.groveConnected() ? "ACK" : "--",
                useGrove ? "Grove" : "Net");
        rows[rowCount++] = buffer;

        if (groveLink_.pairingState() == GrovePairingState::InProgress) {
            rows[rowCount++] = "Key: pairing...";
        } else if (!groveLink_.hasSessionKey()) {
            rows[rowCount++] = groveLink_.pairingState() == GrovePairingState::Failed
                                   ? "Key: pair failed"
                                   : "Key: none (Tab to pair)";
        } else if (strcmp(payloadState, "success") == 0) {
            snprintf(buffer, sizeof(buffer), "Paired  Slot: ok (%u)",
                    static_cast<unsigned>(payloadFindings));
            rows[rowCount++] = buffer;
        } else {
            rows[rowCount++] = String("Paired  Slot: ") + payloadState;
        }
    } else {
        // frameAgeMs() ticks constantly while waiting; left out of the
        // signature for the same reason age is left out above.
        signature = (static_cast<uint32_t>(groveLink_.connected()) << 16) ^
                    groveLink_.lastSequence() ^ (groveLink_.validFrames() << 8) ^
                    groveLink_.crcErrors() ^ groveLink_.maximumGapMs();
        if (!fullDraw && signature == lastPoeCompanionDetailSignature_) return;
        lastPoeCompanionDetailSignature_ = signature;

        rows[rowCount++] = "Read-only protocol v1 discovery";
        rows[rowCount++] = "Service: _ghostwire._tcp";
        char buffer[48];
        snprintf(buffer, sizeof(buffer), "Grove: %s seq %lu age %lums",
                groveLink_.connected() ? "RECEIVING" : "WAITING",
                static_cast<unsigned long>(groveLink_.lastSequence()),
                groveLink_.frameAgeMs());
        rows[rowCount++] = buffer;
        snprintf(buffer, sizeof(buffer), "Frames %lu CRC %lu maxgap %lums",
                static_cast<unsigned long>(groveLink_.validFrames()),
                static_cast<unsigned long>(groveLink_.crcErrors()),
                groveLink_.maximumGapMs());
        rows[rowCount++] = buffer;
    }

    ScreenChrome::beginContentUpdate("Relay Detail", fullDraw);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(Branding::text, Branding::background);
    ScreenChrome::normalizeListPosition(rowCount);
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, rowCount);
    for (size_t row = 0; row < ScreenChrome::kVisibleRows && row + listOffset_ < rowCount;
        ++row) {
        ScreenChrome::drawListRow(static_cast<int>(row), rows[row + listOffset_],
                                  row + listOffset_ == listSelection_);
    }

    if (!poeLootExtractStatus_.isEmpty()) {
        ScreenChrome::drawFooter(poeLootExtractStatus_.c_str());
    } else {
        ScreenChrome::drawFooter(groveLink_.connected()
                                     ? "Up/Dn: scroll  R: refresh  Tab: actions"
                                     : "Up/Dn: scroll   R: refresh   Q: back");
    }
}

// Lists .txt files under /ghostwire/poe-scripts/ (loadPoeScripts(),
// main.cpp) -- a different SD directory/vocabulary from the BLE HID
// DuckyScript picker. Selecting one (Enter) uploads it to `targetSlot`,
// fixed by which Tab-menu item opened this screen.
void NetworkScanScreens::drawPoePayloadScripts(uint8_t targetSlot, bool fullDraw) {
    ScreenChrome::beginContentUpdate("Upload Script", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);
    ScreenChrome::normalizeListPosition(poeScripts_.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, poeScripts_.size());

    if (poeScripts_.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 36);
        display.print("No scripts found.");
        display.setCursor(8, 52);
        display.print("Save .txt files to");
        display.setCursor(8, 68);
        display.print("/ghostwire/poe-scripts/");
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows && row + listOffset_ < poeScripts_.size();
            ++row) {
            ScreenChrome::drawListRow(static_cast<int>(row), poeScripts_[row + listOffset_],
                                      row + listOffset_ == listSelection_);
        }
    }

    if (!poeScriptUploadStatus_.isEmpty()) {
        ScreenChrome::drawFooter(poeScriptUploadStatus_.c_str());
    } else {
        char footer[40];
        snprintf(footer, sizeof(footer), "Slot %u  Enter: upload  Q: back",
                static_cast<unsigned>(targetSlot));
        ScreenChrome::drawFooter(footer);
    }
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
