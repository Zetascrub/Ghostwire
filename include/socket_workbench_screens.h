#pragma once

#include <WiFi.h>
#include <vector>

// Network Socket Workbench screens: setup form (Connect/Listen mode, target)
// and the live session view. Draw-only (see docs/screen-extraction.md);
// connecting, listening, byte-level send/receive, and SD logging stay in
// main.cpp. Deliberately not a terminal emulator: outgoing data is composed
// a line at a time rather than forwarded keystroke-by-keystroke, which is
// also what makes hex view and SD logging straightforward -- every line has
// a clean start/end rather than being a raw continuous byte stream.
class SocketWorkbenchScreens {
public:
    SocketWorkbenchScreens(String& targetInput, String& status, bool& modeListen,
                           WiFiClient& client, bool& listening, uint16_t& port,
                           String& host, std::vector<String>& lines,
                           String& composeInput, bool& hexView,
                           bool& loggingActive)
        : targetInput_(targetInput),
          status_(status),
          modeListen_(modeListen),
          client_(client),
          listening_(listening),
          port_(port),
          host_(host),
          lines_(lines),
          composeInput_(composeInput),
          hexView_(hexView),
          loggingActive_(loggingActive) {}

    void drawSetup();
    // Redraws just the scrolling history area plus the compose line --
    // called on every new byte and every keystroke, so it must not touch
    // drawHeader()'s screen clear or the screen flickers.
    void drawSessionDynamic();
    // Redraws just the footer -- connection/listening state can change
    // between history updates (e.g. a listener accepting a client), so this
    // is called on its own periodic tick too, still without touching the
    // header.
    void drawSessionFooter();
    void drawSession();

private:
    String& targetInput_;
    String& status_;
    bool& modeListen_;
    WiFiClient& client_;
    bool& listening_;
    uint16_t& port_;
    String& host_;
    std::vector<String>& lines_;
    String& composeInput_;
    bool& hexView_;
    bool& loggingActive_;
};
