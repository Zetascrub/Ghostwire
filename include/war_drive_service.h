#pragma once

#include <Arduino.h>
#include <esp_wifi_types.h>
#include <vector>

struct WarDriveWifiResult {
    String ssid;
    uint8_t bssid[6];
    int32_t channel;
    int32_t rssi;
    wifi_auth_mode_t authmode;
};

struct WarDriveBleResult {
    String name;
    String address;
    int rssi;
    bool connectable;
    String service;
};

class WarDriveService {
public:
    void start();
    void stop();
    void update();

    bool isActive() const { return active_; }
    const char* currentPhaseName() const;
    uint32_t wifiUniqueCount() const {
        return static_cast<uint32_t>(wifiUniqueCount_);
    }
    uint32_t bleUniqueCount() const {
        return static_cast<uint32_t>(bleUniqueCount_);
    }

    // Drain-style, same idiom as WifiSnifferService::nextRecord(): pops the
    // oldest pending result found since the last call.
    bool nextWifiResult(WarDriveWifiResult& result);
    bool nextBleResult(WarDriveBleResult& result);

private:
    enum class Phase { Idle, WifiScanning, BleScanning };

    void beginWifiPhase();
    void beginBlePhase();
    void finishWifiPhase(int16_t count);
    void finishBlePhase();
    void noteUniqueWifi(const uint8_t bssid[6]);
    void noteUniqueBle(const String& address);

    bool active_ = false;
    Phase phase_ = Phase::Idle;
    bool bleInitialized_ = false;

    static constexpr size_t kMaxUnique = 256;
    uint8_t wifiBssids_[kMaxUnique][6];
    size_t wifiUniqueCount_ = 0;
    String bleAddresses_[kMaxUnique];
    size_t bleUniqueCount_ = 0;

    std::vector<WarDriveWifiResult> pendingWifi_;
    std::vector<WarDriveBleResult> pendingBle_;
};
