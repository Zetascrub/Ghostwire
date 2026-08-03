#include "network_host_scan_service.h"

#include <WiFi.h>
#include <ping/ping_sock.h>

namespace {
constexpr uint32_t kPingTimeoutMs = 300;
constexpr size_t kMaxHosts = 254;

uint32_t ipToUint32(const IPAddress& ip) {
    return (static_cast<uint32_t>(ip[0]) << 24) |
           (static_cast<uint32_t>(ip[1]) << 16) |
           (static_cast<uint32_t>(ip[2]) << 8) | static_cast<uint32_t>(ip[3]);
}

IPAddress uint32ToIp(uint32_t value) {
    return IPAddress(static_cast<uint8_t>(value >> 24),
                     static_cast<uint8_t>(value >> 16),
                     static_cast<uint8_t>(value >> 8),
                     static_cast<uint8_t>(value));
}
}  // namespace

IPAddress NetworkHostScanService::candidateAt(uint32_t index) const {
    // Host addresses start at network_+1 (network_+0 is the network
    // address itself, already excluded from hostCount_'s range).
    return uint32ToIp(network_ + 1 + index);
}

bool NetworkHostScanService::start() {
    if (WiFi.status() != WL_CONNECTED) return false;

    const IPAddress localIp = WiFi.localIP();
    const IPAddress mask = WiFi.subnetMask();
    const uint32_t ipValue = ipToUint32(localIp);
    const uint32_t maskValue = ipToUint32(mask);
    const uint32_t network = ipValue & maskValue;
    const uint32_t broadcast = network | (~maskValue);

    // Range is network+1 .. broadcast-1 (excludes network/broadcast
    // addresses), capped at kMaxHosts -- almost all real networks are /24
    // anyway, and scanning a much larger range sequentially at
    // kPingTimeoutMs/host would take far too long on this hardware.
    uint32_t available = 0;
    if (broadcast > network + 1) available = broadcast - network - 1;
    hostCount_ = available > kMaxHosts ? kMaxHosts : available;

    network_ = network;
    ownIp_ = ipValue;
    currentIndex_ = 0;
    foundCount_ = 0;
    pendingResults_.clear();
    active_ = true;
    beginNextPing();
    return true;
}

void NetworkHostScanService::stop() {
    if (currentHandle_ != nullptr) {
        esp_ping_stop(currentHandle_);
        esp_ping_delete_session(currentHandle_);
        currentHandle_ = nullptr;
    }
    active_ = false;
}

void NetworkHostScanService::beginNextPing() {
    while (currentIndex_ < hostCount_) {
        if (ipToUint32(candidateAt(currentIndex_)) == ownIp_) {
            ++currentIndex_;
            continue;
        }

        currentDone_ = false;
        currentFound_ = false;
        const IPAddress target = candidateAt(currentIndex_);
        esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
        config.count = 1;
        config.timeout_ms = kPingTimeoutMs;
        config.interval_ms = kPingTimeoutMs;
        config.data_size = 32;
        IP_ADDR4(&config.target_addr, target[0], target[1], target[2],
                 target[3]);

        esp_ping_callbacks_t callbacks{};
        callbacks.cb_args = this;
        callbacks.on_ping_success = onPingSuccess;
        callbacks.on_ping_timeout = nullptr;
        callbacks.on_ping_end = onPingEnd;

        esp_ping_handle_t handle = nullptr;
        if (esp_ping_new_session(&config, &callbacks, &handle) != ESP_OK) {
            ++currentIndex_;
            continue;
        }

        currentHandle_ = handle;
        currentPingStartMs_ = millis();
        esp_ping_start(handle);
        return;
    }
    active_ = false;
}

void NetworkHostScanService::finishCurrentPing() {
    if (currentFound_) {
        pendingResults_.push_back({candidateAt(currentIndex_)});
        ++foundCount_;
    }
    if (currentHandle_ != nullptr) {
        esp_ping_stop(currentHandle_);
        esp_ping_delete_session(currentHandle_);
        currentHandle_ = nullptr;
    }
    ++currentIndex_;
    beginNextPing();
}

void NetworkHostScanService::update() {
    if (!active_) return;
    // Watchdog: force-advance if a single host's callbacks never fire --
    // defensive against a stuck ping session hanging the whole scan.
    const bool watchdogExpired =
        millis() - currentPingStartMs_ > (kPingTimeoutMs + 1000);
    if (currentDone_ || watchdogExpired) {
        finishCurrentPing();
    }
}

bool NetworkHostScanService::nextHostResult(NetworkHostResult& result) {
    if (pendingResults_.empty()) return false;
    result = pendingResults_.front();
    pendingResults_.erase(pendingResults_.begin());
    return true;
}

void NetworkHostScanService::onPingSuccess(void* hdl, void* args) {
    (void)hdl;
    auto* self = static_cast<NetworkHostScanService*>(args);
    self->currentFound_ = true;
}

void NetworkHostScanService::onPingEnd(void* hdl, void* args) {
    (void)hdl;
    auto* self = static_cast<NetworkHostScanService*>(args);
    self->currentDone_ = true;
}
