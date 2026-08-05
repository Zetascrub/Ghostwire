#include "wifi_sniffer_screen.h"

#include <M5Cardputer.h>
#include <algorithm>

#include "branding.h"
#include "screen_chrome.h"

void WifiSnifferScreen::draw(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Wi-Fi Sniffer", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(service_.isActive() ? Branding::accent
                                              : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("RF %s  CH %-2u%s  %-9s",
                   service_.isActive() ? "ON" : "OFF",
                   service_.currentChannel(),
                   service_.channelLocked() ? "L" : "H",
                   service_.captureModeName());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Probes: %lu  Unique: %u",
                   static_cast<unsigned long>(service_.probeCount()),
                   static_cast<unsigned>(service_.uniqueDeviceCount()));
    const uint32_t dropped = service_.droppedProbeCount();
    if (dropped > 0) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(184, 46);
        display.printf("D:%lu", static_cast<unsigned long>(dropped));
    }
    if (captureLogger_.isActive()) {
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(8, 58);
        display.printf("PCAP %lu frames  %llu KiB  drop %lu",
                       static_cast<unsigned long>(captureLogger_.rowCount()),
                       captureLogger_.byteCount() / 1024ULL,
                       static_cast<unsigned long>(
                           service_.droppedRawFrameCount()));
    } else if (captureLogger_.rowCount() > 0) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 58);
        display.printf("PCAP SAVED  %lu frames  %llu KiB",
                       static_cast<unsigned long>(captureLogger_.rowCount()),
                       captureLogger_.byteCount() / 1024ULL);
    }

    if (recentProbes_.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 72);
        display.print("Waiting for probe requests...");
    } else {
        constexpr int kLineHeight = 13;
        constexpr int kFirstLineY = 72;
        constexpr size_t kVisibleLines = 4;
        const size_t total = recentProbes_.size();
        const size_t shown = std::min(kVisibleLines, total);
        display.setTextColor(Branding::text, Branding::background);
        for (size_t row = 0; row < shown; ++row) {
            const WifiProbeRecord& probe = recentProbes_[total - 1 - row];
            String ssid = String(probe.ssid);
            if (ssid.isEmpty()) ssid = "<wildcard>";
            display.setCursor(8,
                              kFirstLineY + static_cast<int>(row) * kLineHeight);
            display.printf("%02X:%02X:%02X %-12s %d", probe.mac[3],
                           probe.mac[4], probe.mac[5],
                           ssid.substring(0, 12).c_str(), probe.rssi);
        }
    }
    if (fullDraw) ScreenChrome::drawFooter("R: restart   Tab: actions   Q: back");
}
