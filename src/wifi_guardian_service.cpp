#include "wifi_guardian_service.h"

namespace {
constexpr uint32_t kWindowMs = 10000;
constexpr uint32_t kAlertCooldownMs = 30000;
}

void WifiGuardianService::begin(GuardianSensitivity sensitivity) {
    sensitivity_ = sensitivity;
    active_ = true;
    managementFrames_ = 0;
    beaconFrames_ = 0;
    probeFrames_ = 0;
    deauthFrames_ = 0;
    disassocFrames_ = 0;
    alertCount_ = 0;
    windowDisruptionFrames_ = 0;
    windowStartedMs_ = millis();
    lastAlertMs_ = 0;
    alertPending_ = false;
    alertMessage_ = "Watching management traffic";
}

void WifiGuardianService::stop() {
    active_ = false;
    alertPending_ = false;
}

uint8_t WifiGuardianService::subtype(const WifiRawFrameRecord& frame) {
    if (frame.length < 2) return 0xFF;
    return static_cast<uint8_t>((frame.data[0] >> 4U) & 0x0FU);
}

bool WifiGuardianService::isDisruptionFrame(
    const WifiRawFrameRecord& frame) {
    const uint8_t value = subtype(frame);
    return value == 10 || value == 12;  // Disassociation / deauthentication.
}

uint16_t WifiGuardianService::threshold() const {
    switch (sensitivity_) {
        case GuardianSensitivity::Relaxed: return 20;
        case GuardianSensitivity::Watchful: return 4;
        default: return 8;
    }
}

void WifiGuardianService::ingest(const WifiRawFrameRecord& frame) {
    if (!active_ || frame.length < 2) return;
    ++managementFrames_;
    switch (subtype(frame)) {
        case 4: ++probeFrames_; break;
        case 8: ++beaconFrames_; break;
        case 10:
            ++disassocFrames_;
            ++windowDisruptionFrames_;
            break;
        case 12:
            ++deauthFrames_;
            ++windowDisruptionFrames_;
            break;
        default: break;
    }

    const uint32_t now = millis();
    const bool cooldownPassed =
        lastAlertMs_ == 0 || now - lastAlertMs_ >= kAlertCooldownMs;
    if (!alertPending_ && cooldownPassed &&
        windowDisruptionFrames_ >= threshold()) {
        ++alertCount_;
        lastAlertMs_ = now;
        alertPending_ = true;
        alertMessage_ = "Deauth/disassoc burst: " +
                        String(windowDisruptionFrames_) + " frames/10s";
    }
}

void WifiGuardianService::update() {
    if (!active_) return;
    const uint32_t now = millis();
    if (now - windowStartedMs_ < kWindowMs) return;
    windowStartedMs_ = now;
    windowDisruptionFrames_ = 0;
}

void WifiGuardianService::cycleSensitivity() {
    sensitivity_ = static_cast<GuardianSensitivity>(
        (static_cast<uint8_t>(sensitivity_) + 1U) % 3U);
    windowDisruptionFrames_ = 0;
    windowStartedMs_ = millis();
}

const char* WifiGuardianService::sensitivityName() const {
    switch (sensitivity_) {
        case GuardianSensitivity::Relaxed: return "RELAXED";
        case GuardianSensitivity::Watchful: return "WATCHFUL";
        default: return "BALANCED";
    }
}

bool WifiGuardianService::takeAlert(String& message) {
    if (!alertPending_) return false;
    message = alertMessage_;
    alertPending_ = false;
    return true;
}
