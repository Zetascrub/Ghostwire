#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <vector>

struct NetworkHostResult {
    IPAddress ip;
};

class NetworkHostScanService {
public:
    bool start();
    void stop();
    void update();

    bool isActive() const { return active_; }
    size_t scannedCount() const { return currentIndex_; }
    size_t totalCount() const { return hostCount_; }
    size_t foundCount() const { return foundCount_; }

    // Drain-style, same idiom as WifiSnifferService::nextRecord().
    bool nextHostResult(NetworkHostResult& result);

private:
    void beginNextPing();
    void finishCurrentPing();
    IPAddress candidateAt(uint32_t index) const;

    static void onPingSuccess(void* hdl, void* args);
    static void onPingEnd(void* hdl, void* args);

    bool active_ = false;
    uint32_t network_ = 0;
    uint32_t ownIp_ = 0;
    size_t hostCount_ = 0;
    size_t currentIndex_ = 0;
    size_t foundCount_ = 0;
    void* currentHandle_ = nullptr;
    unsigned long currentPingStartMs_ = 0;

    // Set by the static ping callbacks (internal ping task context), read
    // by update() (main loop context) -- same volatile-flag handoff
    // already used in wifi_sniffer_service.cpp/chameleon_ultra_client.cpp.
    volatile bool currentDone_ = false;
    volatile bool currentFound_ = false;

    std::vector<NetworkHostResult> pendingResults_;
};
