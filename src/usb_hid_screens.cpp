#include "usb_hid_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "hid_presets.h"
#include "screen_chrome.h"

void UsbHidScreens::drawUsbHid() {
    ScreenChrome::drawHeader("USB / HID");
    constexpr size_t kUsbHidItemCount = kHidPresetCount + 1;
    ScreenChrome::normalizeListPosition(kUsbHidItemCount);
    for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                         row + listOffset_ < kUsbHidItemCount;
        ++row) {
        const size_t item = row + listOffset_;
        ScreenChrome::drawListRow(row,
                                  item < kHidPresetCount ? kHidPresetNames[item]
                                                         : "Run DuckyScript",
                                  item == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: review test  Backspace/Q: back");
}

void UsbHidScreens::drawUsbHidConfirm() {
    ScreenChrome::drawHeader("Confirm HID Test");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 31);
    display.print("Focus a blank text editor now.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 51);
    display.printf("Preset: %s", kHidPresetNames[listSelection_]);
    display.setCursor(8, 70);
    display.print("This sends text only.");
    display.setCursor(8, 88);
    display.print("No commands or shortcuts.");
    ScreenChrome::drawFooter("Enter: TYPE NOW  Backspace/Q: cancel");
}

void UsbHidScreens::drawDuckyScripts() {
    ScreenChrome::drawHeader("DuckyScript Files");
    ScreenChrome::normalizeListPosition(duckyScripts_.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, duckyScripts_.size());
    if (duckyScripts_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print("No .txt/.duck scripts in");
        M5Cardputer.Display.setCursor(8, 53);
        M5Cardputer.Display.print("/ghostwire/scripts");
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < duckyScripts_.size();
            ++row) {
            ScreenChrome::drawListRow(row, duckyScripts_[row + listOffset_],
                                      row + listOffset_ == listSelection_);
        }
    }
    ScreenChrome::drawFooter("Enter: inspect  R: reload  Q: back");
}

void UsbHidScreens::drawDuckyConfirm() {
    ScreenChrome::drawHeader("Confirm DuckyScript");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 28);
    display.print(duckyScripts_[listSelection_].substring(0, 35));
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("Commands: %u  Unsupported: %u",
                   static_cast<unsigned>(duckyCommandCount_),
                   static_cast<unsigned>(duckyUnsupportedCount_));
    display.setCursor(8, 65);
    display.printf("Declared delays: %lus",
                   static_cast<unsigned long>(duckyDeclaredDelayMs_ / 1000));
    display.setCursor(8, 83);
    display.print("Focus the authorized USB host.");
    display.setCursor(8, 101);
    display.print("3 second cancelable countdown.");
    ScreenChrome::drawFooter("Enter: RUN   Esc: cancel");
}

void UsbHidScreens::drawDuckyResult() {
    ScreenChrome::drawHeader("DuckyScript Result");
    M5Cardputer.Display.setTextColor(Branding::text, Branding::background);
    M5Cardputer.Display.setCursor(8, 42);
    M5Cardputer.Display.print(duckyRunStatus_);
    M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
    M5Cardputer.Display.setCursor(8, 65);
    M5Cardputer.Display.print("All HID keys released.");
    ScreenChrome::drawFooter("Enter/Esc: scripts");
}
