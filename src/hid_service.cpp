#include "hid_service.h"

#include <USBHIDKeyboard.h>

HidService::HidService() : keyboard_(new USBHIDKeyboard()) {}

void HidService::begin() {
    if (ready_) return;
    keyboard_->begin();
    keyboard_->releaseAll();
    ready_ = true;
}

void HidService::typeSlowly(const char* text, uint16_t delayMs) {
    while (*text != '\0') {
        keyboard_->write(static_cast<uint8_t>(*text++));
        delay(delayMs);
    }
}

void HidService::run(HidPreset preset) {
    begin();
    keyboard_->releaseAll();
    switch (preset) {
        case HidPreset::Signature:
            keyboard_->print("Ghostwire 0.2.4 HID test by Zetascrub");
            break;
        case HidPreset::LayoutSample:
            keyboard_->print(
                "Ghostwire layout: abcdefghijklmnopqrstuvwxyz 0123456789 "
                "[]{}() !@#$%&*");
            break;
        case HidPreset::SlowTyping:
            typeSlowly("Ghostwire slow typing test: 1 2 3 4 5", 90);
            break;
    }
    keyboard_->releaseAll();
}

bool HidService::ready() const {
    return ready_;
}

void HidService::typeText(const String& text) {
    begin();
    keyboard_->print(text);
}

void HidService::tapEnter() { begin(); keyboard_->write(KEY_RETURN); }
void HidService::tapTab() { begin(); keyboard_->write(KEY_TAB); }
void HidService::tapBackspace() { begin(); keyboard_->write(KEY_BACKSPACE); }
void HidService::tapSpace() { begin(); keyboard_->write(' '); }
void HidService::releaseAll() { begin(); keyboard_->releaseAll(); }
