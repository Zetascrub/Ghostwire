#pragma once

#include <Arduino.h>
#include <vector>

// USB/HID self-test and DuckyScript screens: grouped as one module (see
// docs/screen-extraction.md) since they're both reached from the USB/HID
// tool and share the list cursor. Running a preset/script, the 3-second
// confirm countdown, and script preflighting all stay in main.cpp -- these
// are draw-only.
class UsbHidScreens {
public:
    UsbHidScreens(size_t& listSelection, size_t& listOffset,
                 std::vector<String>& duckyScripts, size_t& duckyCommandCount,
                 size_t& duckyUnsupportedCount, uint32_t& duckyDeclaredDelayMs,
                 String& duckyRunStatus)
        : listSelection_(listSelection),
          listOffset_(listOffset),
          duckyScripts_(duckyScripts),
          duckyCommandCount_(duckyCommandCount),
          duckyUnsupportedCount_(duckyUnsupportedCount),
          duckyDeclaredDelayMs_(duckyDeclaredDelayMs),
          duckyRunStatus_(duckyRunStatus) {}

    void drawUsbHid();
    void drawUsbHidConfirm();
    void drawDuckyScripts();
    void drawDuckyConfirm();
    void drawDuckyResult();

private:
    size_t& listSelection_;
    size_t& listOffset_;
    std::vector<String>& duckyScripts_;
    size_t& duckyCommandCount_;
    size_t& duckyUnsupportedCount_;
    uint32_t& duckyDeclaredDelayMs_;
    String& duckyRunStatus_;
};
