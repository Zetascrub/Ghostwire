#include "ssh_screens.h"

#include <M5Cardputer.h>
#include <algorithm>

#include "branding.h"
#include "screen_chrome.h"

void SshScreens::drawConnect() {
    ScreenChrome::drawHeader("SSH Client");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    ScreenChrome::drawTextEntryRow(36, "Target: ", hostInput_);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 54);
    display.print("Format: user@host or user@host:port");
    ScreenChrome::drawFooter(status_.isEmpty() ? "Enter: next   Tab: history"
                                               : status_.c_str());
}

void SshScreens::drawPassword() {
    ScreenChrome::drawHeader("SSH Client");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 36);
    display.printf("%s@%s:%u", username_.c_str(), host_.c_str(), port_);
    ScreenChrome::drawTextEntryRow(54, "Password: ", passwordInput_, true);
    ScreenChrome::drawFooter(status_.isEmpty() ? "Enter: connect   Esc: back"
                                               : status_.c_str());
}

void SshScreens::drawSessionDynamic() {
    auto& display = M5Cardputer.Display;
    const int top = 24;
    const int bottom = display.height() - 15;
    display.fillRect(0, top, display.width(), bottom - top, Branding::background);
    display.setTextColor(Branding::text, Branding::background);

    std::vector<const String*> visible;
    for (const auto& line : lines_) visible.push_back(&line);
    visible.push_back(&pendingLine_);
    const size_t total = visible.size();
    const size_t shown = std::min(ScreenChrome::kVisibleRows, total);
    for (size_t row = 0; row < shown; ++row) {
        const String& line = *visible[total - shown + row];
        display.setCursor(4, top + 2 + static_cast<int>(row) * 15);
        display.print(line.substring(0, 39));
    }
}

void SshScreens::drawSession() {
    ScreenChrome::drawHeader(("SSH " + username_ + "@" + host_).c_str());
    drawSessionDynamic();
    ScreenChrome::drawFooter(service_.isConnected() ? "Esc: disconnect"
                                                    : "Disconnected   Esc: back");
}
