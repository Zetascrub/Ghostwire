#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <vector>

#include "auth.h"
#include "loot_entry.h"

enum class PoeCompanionState {
    NotChecked,
    WifiRequired,
    Discovering,
    NotFound,
    Incompatible,
    Ready,
    Stale,
    Offline,
    Error,
};

class PoeCompanionService {
public:
    // Performs one bounded DNS-SD lookup and HTTP status fetch. This mirrors
    // Ghostwire's other explicit refresh actions and does not run a hidden
    // background network operation.
    bool refresh();
    // Refreshes a previously validated endpoint without repeating DNS-SD.
    // Falls back to full discovery when no endpoint is cached.
    bool poll();

    PoeCompanionState state() const { return state_; }
    const String& statusMessage() const { return statusMessage_; }
    const String& deviceId() const { return deviceId_; }
    const String& model() const { return model_; }
    const String& firmwareVersion() const { return firmwareVersion_; }
    const String& companionIp() const { return companionIp_; }
    const String& reportedIp() const { return reportedIp_; }
    const String& gateway() const { return gateway_; }
    const String& dns() const { return dns_; }
    uint16_t port() const { return port_; }
    bool ethernetStarted() const { return ethernetStarted_; }
    bool ethernetLinkUp() const { return ethernetLinkUp_; }
    bool internetReachable() const { return internetReachable_; }
    bool ghostwireConnected() const { return ghostwireConnected_; }
    const String& indicatorState() const { return indicatorState_; }
    bool groveConnected() const { return groveConnected_; }
    uint32_t groveSequence() const { return groveSequence_; }
    unsigned long lastRefreshMs() const { return lastRefreshMs_; }
    unsigned long lastSuccessMs() const { return lastSuccessMs_; }
    unsigned long statusAgeMs() const {
        return lastSuccessMs_ == 0 ? 0 : millis() - lastSuccessMs_;
    }
    uint8_t consecutiveFailures() const { return consecutiveFailures_; }
    uint32_t linkSpeedMbps() const { return linkSpeedMbps_; }
    bool fullDuplex() const { return fullDuplex_; }
    uint64_t relayUptimeMs() const { return relayUptimeMs_; }
    const String& resetReason() const { return resetReason_; }
    bool hasTemperature() const { return hasTemperature_; }
    float temperatureC() const { return temperatureC_; }
    uint32_t freeHeapBytes() const { return freeHeapBytes_; }
    uint32_t minimumFreeHeapBytes() const { return minimumFreeHeapBytes_; }
    // The payload engine's traffic-light state and last scan's finding
    // count, mirrored from the status JSON's "payload" object -- the same
    // fields Grove's S frame carries, so a command's result is visible the
    // same way regardless of which transport triggered it. Defaults to
    // idle/0 against older firmware that doesn't send this object yet.
    const char* payloadRunState() const;
    size_t payloadFindingCount() const { return payloadFindingCount_; }

    // Wi-Fi command channel: POSTs a pre-authenticated command to the
    // cached companion endpoint. This class stays crypto-agnostic -- the
    // caller (main.cpp, via GroveCompanionLink::buildWifiCommandTag(), the
    // only holder of the session key) already built `tag`; this just knows
    // how to reach /v1/command over HTTP, the same way fetchStatus() knows
    // /v1/status. Blocking, like every other explicit action here -- HTTP
    // gets a synchronous accept/reject in the one response, so unlike
    // Grove's fire-then-poll design there's no pending state to track.
    // Requires a prior successful discovery (companionIp()/port() set).
    bool sendCommand(uint8_t slot, uint32_t nonce,
                     const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES]);

    // Wi-Fi script upload / loot extraction (POST /v1/payload, /v1/loot) --
    // same crypto-agnostic split as sendCommand() above: main.cpp builds
    // `tag` via GroveCompanionLink, this just knows how to reach the two
    // endpoints.
    bool sendScript(uint8_t slot, uint32_t nonce, const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES],
                    const String& script);
    bool fetchLoot(uint32_t nonce, const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES],
                   std::vector<LootEntry>& out);

private:
    bool ensureMdns();
    bool discover(IPAddress& address, uint16_t& port);
    bool fetchStatus(const IPAddress& address, uint16_t port);
    void clearDeviceState();

    PoeCompanionState state_ = PoeCompanionState::NotChecked;
    String statusMessage_ = "Press R to discover";
    String deviceId_;
    String model_;
    String firmwareVersion_;
    String companionIp_;
    String reportedIp_;
    String gateway_;
    String dns_;
    uint16_t port_ = 0;
    bool ethernetStarted_ = false;
    bool ethernetLinkUp_ = false;
    bool internetReachable_ = false;
    bool ghostwireConnected_ = false;
    String indicatorState_;
    bool groveConnected_ = false;
    uint32_t groveSequence_ = 0;
    bool mdnsStarted_ = false;
    unsigned long lastRefreshMs_ = 0;
    unsigned long lastSuccessMs_ = 0;
    unsigned long lastDiscoveryAttemptMs_ = 0;
    uint8_t consecutiveFailures_ = 0;
    uint32_t linkSpeedMbps_ = 0;
    bool fullDuplex_ = false;
    uint64_t relayUptimeMs_ = 0;
    String resetReason_;
    bool hasTemperature_ = false;
    float temperatureC_ = 0.0f;
    uint32_t freeHeapBytes_ = 0;
    uint32_t minimumFreeHeapBytes_ = 0;
    char payloadStateCode_ = 'I';
    size_t payloadFindingCount_ = 0;
};
