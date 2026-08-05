#pragma once

#include "gnss_service.h"
#include "sd_logger.h"

// GNSS Foundation screen: fix/satellite/position readout plus the CSV log
// indicator. Second screen extracted out of src/main.cpp (see
// docs/screen-extraction.md); unlike IrScreen this one has a periodic
// partial-redraw path (fullDraw=false), which main.cpp's loop() still drives
// on its own 500ms tick.
//
// This extraction covers drawing only. Starting/stopping the CSV log and the
// once-a-second background row-append stay in main.cpp for now: the append
// runs whenever logging is active regardless of which screen is on screen,
// so it isn't really screen state -- entangling it here would widen this
// step well past "extract the view".
class GnssScreen {
public:
    GnssScreen(GnssService& service, SdLogger& logger)
        : service_(service), logger_(logger) {}

    void draw(bool fullDraw = true);

private:
    GnssService& service_;
    SdLogger& logger_;
};
