#pragma once

#include <Arduino.h>
#include <vector>

#include "ssh_service.h"

// SSH Client screens: connect form, password entry, and the live session
// view. Draw-only (see docs/screen-extraction.md); connecting, host-key
// trust, and byte-level terminal handling stay in main.cpp.
class SshScreens {
public:
    SshScreens(String& hostInput, String& status, String& username,
              String& host, uint16_t& port, String& passwordInput,
              SshService& service, std::vector<String>& lines,
              String& pendingLine)
        : hostInput_(hostInput),
          status_(status),
          username_(username),
          host_(host),
          port_(port),
          passwordInput_(passwordInput),
          service_(service),
          lines_(lines),
          pendingLine_(pendingLine) {}

    void drawConnect();
    void drawPassword();
    // Redraws just the scrolling text area, same split/reasoning as
    // TelnetScreens::drawSessionDynamic().
    void drawSessionDynamic();
    void drawSession();

private:
    String& hostInput_;
    String& status_;
    String& username_;
    String& host_;
    uint16_t& port_;
    String& passwordInput_;
    SshService& service_;
    std::vector<String>& lines_;
    String& pendingLine_;
};
