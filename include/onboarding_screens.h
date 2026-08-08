#pragma once

#include <Arduino.h>

class OnboardingScreens {
public:
    OnboardingScreens(uint8_t& page, bool& sdAvailable,
                      bool& cardNavigationEnabled,
                      bool& saveWifiCredentials)
        : page_(page),
          sdAvailable_(sdAvailable),
          cardNavigationEnabled_(cardNavigationEnabled),
          saveWifiCredentials_(saveWifiCredentials) {}

    void draw();
    static constexpr uint8_t kPageCount = 7;

private:
    uint8_t& page_;
    bool& sdAvailable_;
    bool& cardNavigationEnabled_;
    bool& saveWifiCredentials_;
};
