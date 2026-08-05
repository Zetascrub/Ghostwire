#pragma once

#include <Arduino.h>

struct WifiProbeRecord {
    uint8_t mac[6];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
};

struct WifiRawFrameRecord {
    static constexpr size_t kMaxLength = 400;
    uint8_t data[kMaxLength];
    size_t length;
    size_t eapolOffset;  // Offset into data[] where the EAPOL header starts.
    bool isEapol;
    uint8_t channel;
    int8_t rssi;
};

enum class WifiCaptureMode : uint8_t { Probes, Management, Full };

class WifiSnifferService {
public:
    bool begin();
    void end();
    void update();

    bool isActive() const { return active_; }
    uint32_t probeCount() const { return probeCount_; }
    uint32_t droppedProbeCount() const;
    uint32_t droppedRawFrameCount() const;
    size_t uniqueDeviceCount() const { return uniqueDeviceCount_; }
    uint8_t currentChannel() const { return channel_; }
    WifiCaptureMode captureMode() const { return captureMode_; }
    const char* captureModeName() const;
    void setCaptureMode(WifiCaptureMode mode);
    void cycleCaptureMode();
    bool channelLocked() const { return channelLocked_; }
    void toggleChannelLock();

    // Pops the oldest pending probe-request record captured since the last
    // call. Returns false once nothing is left to drain.
    bool nextRecord(WifiProbeRecord& record);

    // Starts/stops matching EAPOL frames to/from a specific AP, in addition
    // to (not instead of) the probe-request sniffing above.
    void setHandshakeTarget(const uint8_t bssid[6]);
    void clearHandshakeTarget();
    bool hasHandshakeTarget() const { return handshakeTargetSet_; }

    // Pops the oldest pending captured EAPOL frame. Returns false once
    // nothing is left to drain.
    bool nextRawFrame(WifiRawFrameRecord& record);

private:
    void hopChannelIfDue();
    void noteUniqueMac(const uint8_t* mac);

    static constexpr size_t kMaxTrackedMacs = 64;

    bool active_ = false;
    uint8_t channel_ = 1;
    size_t hopIndex_ = 0;
    unsigned long lastHopMs_ = 0;
    uint32_t probeCount_ = 0;
    size_t uniqueDeviceCount_ = 0;
    uint8_t trackedMacs_[kMaxTrackedMacs][6]{};
    bool handshakeTargetSet_ = false;
    WifiCaptureMode captureMode_ = WifiCaptureMode::Probes;
    bool channelLocked_ = false;
};
