#pragma once

#include <M5Cardputer.h>

#include "ir_service.h"

// Infrared self-test screen: shows the onboard transmitter's pin/carrier
// and emits a test burst on demand. This is the first screen extracted out
// of src/main.cpp's monolithic draw/input switch (see docs/screen-extraction.md)
// -- it owns no state of its own, only a reference to the IrService it
// displays, so it doubles as a template for extracting other stateless
// screens.
class IrScreen {
public:
    explicit IrScreen(IrService& service) : service_(service) {}

    // Draws the screen's current state. Safe to call repeatedly (e.g. from
    // the input loop's default redraw path).
    void draw();

    // Emits the self-test burst, showing progress, then redraws. Call this
    // from Screen::Infrared's Enter/refresh handling.
    void transmitSelfTest();

    // Moved from main.cpp's handleInput() -- see docs/screen-extraction.md.
    // The one case whose input handling touched nothing beyond this class's
    // own service_ reference already, so it needed no new members.
    void handleInput(const Keyboard_Class::KeysState& keys);

private:
    IrService& service_;
};
