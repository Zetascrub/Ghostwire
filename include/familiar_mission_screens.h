#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

// A single confirmed target for a Familiar handshake-capture mission,
// carried over from the operator's selection on Screen::FamiliarMissionSelect
// into the unattended run itself. Kept header-only (see
// docs/screen-extraction.md) since both main.cpp's mission state machine and
// this screen module need the type.
struct FamiliarMissionTarget {
    enum class Status : uint8_t { Pending, Active, Captured, TimedOut };

    String ssid;
    uint8_t bssid[6] = {};
    uint8_t channel = 0;
    Status status = Status::Pending;
};

// Familiar Mission screens: the multi-select target list, a confirm step
// naming exactly what's about to receive deauth frames (same "explicit
// confirm before anything disruptive transmits" convention as single-target
// deauth), and the live unattended-run progress view. Draw-only (see
// docs/screen-extraction.md); target selection, the mission state machine,
// deauth timing, and handshake-completion detection stay in main.cpp,
// reusing the exact same transmitWifiDeauth()/wifiSnifferService handshake
// watch/handshakeCaptureLogger machinery the single-target flow already
// uses -- a mission is just that flow driven unattended across a
// pre-approved list instead of once per manual confirm.
class FamiliarMissionScreens {
public:
    FamiliarMissionScreens(std::vector<wifi_ap_record_t>& accessPoints,
                           size_t& listSelection, size_t& listOffset,
                           std::vector<bool>& selected,
                           std::vector<FamiliarMissionTarget>& targets,
                           size_t& currentIndex,
                           unsigned long& targetStartMs,
                           uint32_t& handshakeEapolFrameCount,
                           bool (&handshakeMessageSeen)[5],
                           bool& handshakePmkidFound, String& status)
        : accessPoints_(accessPoints),
          listSelection_(listSelection),
          listOffset_(listOffset),
          selected_(selected),
          targets_(targets),
          currentIndex_(currentIndex),
          targetStartMs_(targetStartMs),
          handshakeEapolFrameCount_(handshakeEapolFrameCount),
          handshakeMessageSeen_(handshakeMessageSeen),
          handshakePmkidFound_(handshakePmkidFound),
          status_(status) {}

    void drawSelect();
    void drawConfirm();
    // Redraws just the current-target progress area -- called on every new
    // EAPOL frame and every deauth burst, so it must not touch drawHeader()'s
    // screen clear.
    void drawMissionDynamic();
    void drawMission();

private:
    std::vector<wifi_ap_record_t>& accessPoints_;
    size_t& listSelection_;
    size_t& listOffset_;
    std::vector<bool>& selected_;
    std::vector<FamiliarMissionTarget>& targets_;
    size_t& currentIndex_;
    unsigned long& targetStartMs_;
    uint32_t& handshakeEapolFrameCount_;
    bool (&handshakeMessageSeen_)[5];
    bool& handshakePmkidFound_;
    String& status_;
};
