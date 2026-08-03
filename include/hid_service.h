#pragma once

#include <Arduino.h>

class USBHIDKeyboard;

enum class HidPreset : uint8_t {
    Signature,
    LayoutSample,
    SlowTyping,
};

class HidService {
public:
    HidService();
    void begin();
    void run(HidPreset preset);
    void typeText(const String& text);
    void tapEnter();
    void tapTab();
    void tapBackspace();
    void tapSpace();
    void releaseAll();
    bool ready() const;

private:
    void typeSlowly(const char* text, uint16_t delayMs);

    USBHIDKeyboard* keyboard_;
    bool ready_ = false;
};
