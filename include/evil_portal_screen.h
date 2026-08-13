#pragma once

#include <Arduino.h>

#include "evil_portal_service.h"

// Evil Portal confirm + live-session screens. Draw-only (see
// docs/screen-extraction.md); starting/stopping the service and logging
// captures stay in main.cpp since they also touch SdLogger, FamiliarCue,
// and the Loot Board counters -- more than just this screen's display.
//
// `pendingSsid`/`pendingChannel` are the operator's chosen target, set by
// main.cpp when entering the confirm screen (from WifiDetail's selected AP)
// and read by both drawConfirm() and the Enter handler that calls
// service.begin(). `captureCount`/`lastCapture` are references to main.cpp's
// own running totals for this session, since the service's queue is drained
// (and its own counter reset) each begin() -- the screen needs numbers that
// persist across a stop/restart within the same visit, which the service
// alone doesn't track.
class EvilPortalScreen {
public:
    EvilPortalScreen(EvilPortalService& service, String& pendingSsid,
                     uint8_t& pendingChannel, uint32_t& captureCount,
                     String& lastCapture)
        : service_(service),
          pendingSsid_(pendingSsid),
          pendingChannel_(pendingChannel),
          captureCount_(captureCount),
          lastCapture_(lastCapture) {}

    void drawConfirm();
    void draw(bool fullDraw = true);

private:
    EvilPortalService& service_;
    String& pendingSsid_;
    uint8_t& pendingChannel_;
    uint32_t& captureCount_;
    String& lastCapture_;
};
