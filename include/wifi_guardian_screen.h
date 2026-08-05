#pragma once

#include "pcap_logger.h"
#include "wifi_guardian_service.h"
#include "wifi_sniffer_service.h"

// Familiar Guardian screen: passive management-frame monitor status,
// counters, and the most recent event line. Draw-only extraction (see
// docs/screen-extraction.md); starting/stopping the guardian
// (startWifiGuardian()/stopWifiGuardian()) and sensitivity cycling stay in
// main.cpp since they orchestrate several services together, not just this
// screen's display.
//
// `lastEvent` is a reference to main.cpp's shared guardianLastEvent String:
// several places besides this screen's own input handling write to it
// (guardian startup, the background alert-processing tick), so it stays
// owned by main.cpp rather than moving in here.
class WifiGuardianScreen {
public:
    WifiGuardianScreen(WifiGuardianService& guardian,
                       WifiSnifferService& sniffer, PcapLogger& evidenceLogger,
                       String& lastEvent)
        : guardian_(guardian),
          sniffer_(sniffer),
          evidenceLogger_(evidenceLogger),
          lastEvent_(lastEvent) {}

    void draw(bool fullDraw = true);

private:
    WifiGuardianService& guardian_;
    WifiSnifferService& sniffer_;
    PcapLogger& evidenceLogger_;
    String& lastEvent_;
};
