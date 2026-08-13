#include "socket_workbench_screens.h"

#include <M5Cardputer.h>
#include <algorithm>

#include "branding.h"
#include "screen_chrome.h"

void SocketWorkbenchScreens::drawSetup() {
    ScreenChrome::drawHeader("Socket Workbench");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 30);
    display.printf("Mode: %s", modeListen_ ? "Listen (TCP server)"
                                           : "Connect (TCP client)");
    ScreenChrome::drawTextEntryRow(
        50, modeListen_ ? "Port: " : "Host:port: ", targetInput_);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 68);
    display.print(modeListen_ ? "Accepts one connection at a time."
                              : "Format: host or host:port (default 4444)");
    ScreenChrome::drawFooter(
        status_.isEmpty() ? "Left/Right: mode  Enter: start  Esc: cancel"
                          : status_.c_str());
}

void SocketWorkbenchScreens::drawSessionDynamic() {
    auto& display = M5Cardputer.Display;
    const int top = 24;
    const int bottom = display.height() - 15;
    display.fillRect(0, top, display.width(), bottom - top, Branding::background);
    display.setTextColor(Branding::text, Branding::background);

    std::vector<const String*> visible;
    for (const auto& line : lines_) visible.push_back(&line);
    const String composeLine = "> " + composeInput_;
    visible.push_back(&composeLine);
    const size_t total = visible.size();
    const size_t shown = std::min(ScreenChrome::kVisibleRows, total);
    for (size_t row = 0; row < shown; ++row) {
        const String& line = *visible[total - shown + row];
        display.setCursor(4, top + 2 + static_cast<int>(row) * 15);
        display.print(line.substring(0, 39));
    }
}

void SocketWorkbenchScreens::drawSessionFooter() {
    const bool active = client_.connected() || listening_;
    String footer =
        !active ? "Disconnected   Esc: back"
        : client_.connected()
            ? String("Enter: send  ^H: hex  ^L: log  Esc: stop")
            : String("Waiting for a connection...  Esc: stop");
    if (loggingActive_) footer = "[LOG] " + footer;
    ScreenChrome::drawFooter(footer.c_str());
}

void SocketWorkbenchScreens::drawSession() {
    String title = modeListen_
                       ? "Listen :" + String(port_) +
                             (client_.connected() ? " (connected)" : "")
                       : "Connect " + host_ + ":" + String(port_);
    ScreenChrome::drawHeader(title.c_str());
    drawSessionDynamic();
    drawSessionFooter();
}
