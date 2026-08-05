#pragma once

#include <Arduino.h>

#include "ota_service.h"

// Firmware update screens: check result/confirm, and install progress.
// Draw-only (see docs/screen-extraction.md); triggering the check, driving
// downloadAndInstall()'s blocking loop, and rebooting on success all stay
// in main.cpp, same split as every other extracted screen this project
// uses.
class OtaScreens {
public:
    explicit OtaScreens(OtaService& service) : service_(service) {}

    // Renders whichever state OtaService is currently in after a
    // checkForUpdate() call: checking failed, already up to date, or (when
    // service_.hasVerifiedUpdate()) the install confirmation prompt.
    void drawCheck();

    // Progress bar during downloadAndInstall(), reading live progress from
    // OtaService (same idiom as NetworkPortScanService::scannedCount()).
    // fullDraw draws the header/footer; pass false from the progress
    // callback's periodic redraws so only the bar itself repaints.
    void drawInstalling(bool fullDraw = true);

private:
    OtaService& service_;
};
