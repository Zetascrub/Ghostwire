#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <vector>

struct NetworkPortResult {
    uint16_t port;
};

class NetworkPortScanService {
public:
    bool start(IPAddress target, uint16_t startPort, uint16_t endPort);
    void stop();
    void update();

    bool isActive() const { return active_; }
    uint32_t scannedCount() const { return scannedCount_; }
    uint32_t totalCount() const { return totalCount_; }

    // Drain-style, same idiom as WifiSnifferService::nextRecord().
    bool nextPortResult(NetworkPortResult& result);

private:
    static constexpr size_t kMaxConcurrent = 8;
    static constexpr uint32_t kPerPortTimeoutMs = 250;

    struct Slot {
        int fd = -1;
        uint16_t port = 0;
        unsigned long startMs = 0;
        bool inUse = false;
    };

    void fillSlots();
    void pollSlots();
    void closeSlot(size_t index);

    Slot slots_[kMaxConcurrent];
    IPAddress target_;
    uint32_t nextPort_ = 0;
    uint32_t endPort_ = 0;
    uint32_t scannedCount_ = 0;
    uint32_t totalCount_ = 0;
    bool active_ = false;

    std::vector<NetworkPortResult> pendingResults_;
};
