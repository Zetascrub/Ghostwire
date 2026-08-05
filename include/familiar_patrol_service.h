#pragma once

#include <Arduino.h>
#include <FS.h>
#include <IPAddress.h>
#include <vector>

#include "network_host_scan_service.h"
#include "network_port_scan_service.h"

enum class FamiliarPatrolState : uint8_t {
    Idle,
    WaitingForNetwork,
    Discovery,
    CommonPorts,
    WatchWait,
    Complete,
    Stopped,
    Error,
};

class FamiliarPatrolService {
public:
    void begin();
    bool start(const IPAddress& localIp, const IPAddress& subnetMask,
               bool continuous = false, uint32_t intervalMs = 300000);
    void update();
    void stop();

    bool isActive() const;
    bool canResume() const { return resumeAvailable_; }
    FamiliarPatrolState state() const { return state_; }
    const char* stateName() const;
    const String& status() const { return status_; }
    const String& sessionPath() const { return sessionPath_; }

    uint32_t networkAddress() const { return network_; }
    uint32_t subnetMask() const { return mask_; }
    uint32_t addressCount() const { return addressCount_; }
    uint32_t discoveryScanned() const;
    uint32_t hostsFound() const { return hostsFound_; }
    uint32_t hostIndex() const { return hostIndex_; }
    uint32_t openPortsFound() const { return openPortsFound_; }
    uint32_t currentPort() const;
    IPAddress currentHost() const { return currentHost_; }
    IPAddress lastSeenHost() const { return lastSeenHost_; }
    uint16_t lastOpenPort() const { return lastOpenPort_; }
    IPAddress lastOpenHost() const { return lastOpenHost_; }
    bool continuous() const { return continuous_; }
    uint32_t nextScanSeconds() const;
    uint32_t cycle() const { return cycle_; }
    uint32_t cycleHostsFound() const { return cycleHostsFound_; }

    static uint32_t ipToUint32(const IPAddress& ip);
    static IPAddress uint32ToIp(uint32_t value);
    static uint32_t usableAddressCount(const IPAddress& localIp,
                                       const IPAddress& subnetMask);
    static String cidrText(const IPAddress& localIp,
                           const IPAddress& subnetMask);

private:
    static constexpr uint16_t kScoutPorts[] = {
        20, 21, 22, 23, 25, 26, 53, 80, 81, 88, 110, 111, 113, 119, 135,
        139, 143, 144, 179, 199, 389, 427,
        443, 444, 445, 465, 513, 514, 515, 543, 544, 548, 554, 587, 631,
        646, 873, 990, 993, 995, 1025, 1026, 1027, 1028, 1029, 1110,
        1433, 1720, 1723, 1755, 1883, 1900, 2000, 2001, 2049, 2121,
        2375, 2376, 2717, 3000, 3128, 3268, 3306, 3389, 4899, 5000,
        5001, 5060, 5101, 5190, 5357, 5432, 5631, 5666, 5800, 5900,
        6000, 6001, 6379, 6443, 7070,
        8000, 8008, 8009, 8080, 8081, 8088, 8090, 8181, 8443, 8888,
        9000, 9090, 9100, 9200, 9999, 10000, 27017, 32768, 49152,
    };
    static constexpr size_t kScoutPortCount =
        sizeof(kScoutPorts) / sizeof(kScoutPorts[0]);

    bool prepareSession();
    bool loadCheckpoint();
    bool saveCheckpoint();
    void removeCheckpoint();
    bool appendObservation(const char* type, const IPAddress& ip,
                           int32_t port = -1);
    bool appendHost(const IPAddress& ip);
    bool isKnownHost(const IPAddress& ip) const;
    void loadKnownHosts();
    bool prepareCycleHosts();
    void beginNextCycle();
    bool appendPort(const IPAddress& ip, uint16_t port, const char* phase);
    bool writeReport();
    bool ensureHostReader();
    bool loadNextHost();
    void closeFiles();
    void fail(const String& message);
    void enterDiscovery();
    void enterCommonPorts();
    void startCommonProbe();
    void finishCycle();
    bool networkMatches() const;

    FamiliarPatrolState state_ = FamiliarPatrolState::Idle;
    FamiliarPatrolState resumeState_ = FamiliarPatrolState::Idle;
    String status_ = "Ready";
    String sessionPath_;
    uint32_t network_ = 0;
    uint32_t mask_ = 0;
    uint32_t addressCount_ = 0;
    uint32_t discoveryIndex_ = 0;
    uint32_t hostsFound_ = 0;
    uint32_t hostIndex_ = 0;
    uint32_t openPortsFound_ = 0;
    IPAddress currentHost_;
    IPAddress lastSeenHost_;
    IPAddress lastOpenHost_;
    uint16_t lastOpenPort_ = 0;
    bool continuous_ = false;
    uint32_t intervalMs_ = 300000;
    uint32_t nextScanMs_ = 0;
    uint32_t cycle_ = 1;
    uint32_t cycleHostsFound_ = 0;
    static constexpr size_t kKnownHostCacheLimit = 512;
    std::vector<uint32_t> knownHosts_;
    bool knownHostsOverflow_ = false;
    bool resumeAvailable_ = false;
    bool workerStarted_ = false;
    bool hostLoaded_ = false;
    File hostsReader_;
    NetworkHostScanService hostScanner_;
    NetworkPortScanService portScanner_;
};
