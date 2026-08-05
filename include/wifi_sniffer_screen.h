#pragma once

#include <vector>

#include "pcap_logger.h"
#include "wifi_sniffer_service.h"

// Wi-Fi Sniffer screen: capture status, probe/PCAP counters, and the most
// recent probe requests. Draw-only extraction (see
// docs/screen-extraction.md); capture-mode cycling, channel lock, and log
// start/stop stay in main.cpp.
class WifiSnifferScreen {
public:
    WifiSnifferScreen(WifiSnifferService& service, PcapLogger& captureLogger,
                      std::vector<WifiProbeRecord>& recentProbes)
        : service_(service),
          captureLogger_(captureLogger),
          recentProbes_(recentProbes) {}

    void draw(bool fullDraw = true);

private:
    WifiSnifferService& service_;
    PcapLogger& captureLogger_;
    std::vector<WifiProbeRecord>& recentProbes_;
};
