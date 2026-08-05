#include "telnet_screens.h"

#include <M5Cardputer.h>
#include <algorithm>

#include "branding.h"
#include "screen_chrome.h"

void TelnetScreens::drawConnect() {
    ScreenChrome::drawHeader("Telnet Client");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    ScreenChrome::drawTextEntryRow(36, "Host: ", hostInput_);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 54);
    display.print("Format: host or host:port (default 23)");
    ScreenChrome::drawFooter(status_.isEmpty() ? "Enter: connect   Esc: cancel"
                                               : status_.c_str());
}

void TelnetScreens::drawSessionDynamic() {
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

void TelnetScreens::drawSession() {
    ScreenChrome::drawHeader(("Telnet " + host_ + ":" + String(port_)).c_str());
    drawSessionDynamic();
    ScreenChrome::drawFooter(client_.connected() ? "Esc: disconnect"
                                                 : "Disconnected   Esc: back");
}
