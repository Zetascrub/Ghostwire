#pragma once

#include <Arduino.h>

// QR generator screens: text entry and the rendered code. Draw-only (see
// docs/screen-extraction.md); text entry input handling stays in main.cpp.
class QrScreens {
public:
    explicit QrScreens(String& qrText) : qrText_(qrText) {}

    void drawEntry();
    void drawDisplay();

private:
    String& qrText_;
};
