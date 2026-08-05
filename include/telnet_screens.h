#pragma once

#include <WiFi.h>
#include <vector>

// Telnet Client screens: connect form and the live session view. Draw-only
// (see docs/screen-extraction.md); connecting, disconnecting, and byte-level
// terminal handling stay in main.cpp.
class TelnetScreens {
public:
    TelnetScreens(String& hostInput, String& status, WiFiClient& client,
                 String& host, uint16_t& port, std::vector<String>& lines,
                 String& pendingLine)
        : hostInput_(hostInput),
          status_(status),
          client_(client),
          host_(host),
          port_(port),
          lines_(lines),
          pendingLine_(pendingLine) {}

    void drawConnect();
    // Redraws just the scrolling text area (between header and footer) --
    // called on every new byte from the remote host, so it must not touch
    // drawHeader()'s fillScreen()/drawFooter() or the screen flickers on
    // every incoming character.
    void drawSessionDynamic();
    void drawSession();

private:
    String& hostInput_;
    String& status_;
    WiFiClient& client_;
    String& host_;
    uint16_t& port_;
    std::vector<String>& lines_;
    String& pendingLine_;
};
