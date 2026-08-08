#pragma once

#include <Arduino.h>

struct WifiProfile {
    String name;
    String ssid;
    String password;
};

constexpr size_t kMaxWifiProfiles = 5;
