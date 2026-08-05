#pragma once

#include <Arduino.h>

#include "wifi_sniffer_service.h"

enum class GuardianSensitivity : uint8_t { Relaxed, Balanced, Watchful };

class WifiGuardianService {
public:
    void begin(GuardianSensitivity sensitivity = GuardianSensitivity::Balanced);
    void stop();
    void ingest(const WifiRawFrameRecord& frame);
    void update();

    bool isActive() const { return active_; }
    GuardianSensitivity sensitivity() const { return sensitivity_; }
    void cycleSensitivity();
    const char* sensitivityName() const;
    uint32_t managementFrames() const { return managementFrames_; }
    uint32_t beaconFrames() const { return beaconFrames_; }
    uint32_t probeFrames() const { return probeFrames_; }
    uint32_t deauthFrames() const { return deauthFrames_; }
    uint32_t disassocFrames() const { return disassocFrames_; }
    uint32_t alertCount() const { return alertCount_; }
    uint16_t recentDisruptionFrames() const { return windowDisruptionFrames_; }
    bool takeAlert(String& message);

    static uint8_t subtype(const WifiRawFrameRecord& frame);
    static bool isDisruptionFrame(const WifiRawFrameRecord& frame);

private:
    uint16_t threshold() const;

    bool active_ = false;
    GuardianSensitivity sensitivity_ = GuardianSensitivity::Balanced;
    uint32_t managementFrames_ = 0;
    uint32_t beaconFrames_ = 0;
    uint32_t probeFrames_ = 0;
    uint32_t deauthFrames_ = 0;
    uint32_t disassocFrames_ = 0;
    uint32_t alertCount_ = 0;
    uint16_t windowDisruptionFrames_ = 0;
    uint32_t windowStartedMs_ = 0;
    uint32_t lastAlertMs_ = 0;
    bool alertPending_ = false;
    String alertMessage_;
};
