#include "network_port_scan_service.h"

#include <lwip/sockets.h>

namespace {
// IPAddress's uint32_t cast returns its internal byte-order storage
// directly, which already matches what sockaddr_in.sin_addr.s_addr
// expects on this platform (same convention WiFiClient itself relies on
// internally) -- no additional htonl() needed.
sockaddr_in makeSockAddr(const IPAddress& ip, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = static_cast<uint32_t>(ip);
    return addr;
}
}  // namespace

bool NetworkPortScanService::start(IPAddress target, uint16_t startPort,
                                   uint16_t endPort) {
    if (active_) stop();

    target_ = target;
    nextPort_ = startPort;
    endPort_ = endPort;
    scannedCount_ = 0;
    totalCount_ = static_cast<uint32_t>(endPort) -
                  static_cast<uint32_t>(startPort) + 1;
    pendingResults_.clear();
    for (auto& slot : slots_) slot = Slot{};
    active_ = true;
    fillSlots();
    return true;
}

void NetworkPortScanService::stop() {
    for (size_t i = 0; i < kMaxConcurrent; ++i) {
        if (slots_[i].inUse) closeSlot(i);
    }
    active_ = false;
}

void NetworkPortScanService::closeSlot(size_t index) {
    Slot& slot = slots_[index];
    if (slot.fd >= 0) {
        close(slot.fd);
    }
    slot = Slot{};
}

void NetworkPortScanService::fillSlots() {
    for (size_t i = 0; i < kMaxConcurrent; ++i) {
        if (slots_[i].inUse) continue;
        if (nextPort_ > endPort_) continue;

        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;
        const int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        const sockaddr_in addr =
            makeSockAddr(target_, static_cast<uint16_t>(nextPort_));
        const int rc =
            connect(fd, reinterpret_cast<const sockaddr*>(&addr),
                   sizeof(addr));
        if (rc != 0 && errno != EINPROGRESS) {
            // Immediate failure (e.g. no route) -- count as scanned and
            // move on rather than leaving the slot stuck.
            close(fd);
            ++scannedCount_;
            ++nextPort_;
            continue;
        }

        slots_[i].fd = fd;
        slots_[i].port = static_cast<uint16_t>(nextPort_);
        slots_[i].startMs = millis();
        slots_[i].inUse = true;
        ++nextPort_;
    }
}

void NetworkPortScanService::pollSlots() {
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    int maxFd = -1;
    bool anyInUse = false;

    for (const auto& slot : slots_) {
        if (!slot.inUse) continue;
        anyInUse = true;
        FD_SET(slot.fd, &writeSet);
        FD_SET(slot.fd, &errorSet);
        if (slot.fd > maxFd) maxFd = slot.fd;
    }

    if (anyInUse) {
        timeval timeout{0, 0};
        select(maxFd + 1, nullptr, &writeSet, &errorSet, &timeout);
    }

    const unsigned long now = millis();
    for (size_t i = 0; i < kMaxConcurrent; ++i) {
        Slot& slot = slots_[i];
        if (!slot.inUse) continue;

        const bool ready =
            FD_ISSET(slot.fd, &writeSet) || FD_ISSET(slot.fd, &errorSet);
        const bool timedOut = now - slot.startMs > kPerPortTimeoutMs;
        if (!ready && !timedOut) continue;

        if (ready) {
            int socketError = 0;
            socklen_t len = sizeof(socketError);
            if (getsockopt(slot.fd, SOL_SOCKET, SO_ERROR, &socketError,
                           &len) == 0 &&
                socketError == 0) {
                pendingResults_.push_back({slot.port});
            }
        }
        // timedOut with no ready state: filtered/no response, not open.

        closeSlot(i);
        ++scannedCount_;
    }
}

void NetworkPortScanService::update() {
    if (!active_) return;
    pollSlots();
    fillSlots();

    bool anyInUse = false;
    for (const auto& slot : slots_) {
        if (slot.inUse) {
            anyInUse = true;
            break;
        }
    }
    if (nextPort_ > endPort_ && !anyInUse) {
        active_ = false;
    }
}

bool NetworkPortScanService::nextPortResult(NetworkPortResult& result) {
    if (pendingResults_.empty()) return false;
    result = pendingResults_.front();
    pendingResults_.erase(pendingResults_.begin());
    return true;
}
