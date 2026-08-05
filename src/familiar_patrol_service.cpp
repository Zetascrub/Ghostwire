#include "familiar_patrol_service.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>
#include <algorithm>

namespace {
constexpr char kRootPath[] = "/ghostwire/assessments";
constexpr char kCheckpointPath[] = "/ghostwire/assessments/active.json";
constexpr char kCheckpointTempPath[] = "/ghostwire/assessments/active.tmp";
constexpr char kCheckpointBackupPath[] = "/ghostwire/assessments/active.bak";
}

constexpr uint16_t FamiliarPatrolService::kScoutPorts[];

uint32_t FamiliarPatrolService::ipToUint32(const IPAddress& ip) {
    return (static_cast<uint32_t>(ip[0]) << 24) |
           (static_cast<uint32_t>(ip[1]) << 16) |
           (static_cast<uint32_t>(ip[2]) << 8) | ip[3];
}

IPAddress FamiliarPatrolService::uint32ToIp(uint32_t value) {
    return IPAddress(static_cast<uint8_t>(value >> 24),
                     static_cast<uint8_t>(value >> 16),
                     static_cast<uint8_t>(value >> 8),
                     static_cast<uint8_t>(value));
}

uint32_t FamiliarPatrolService::usableAddressCount(
    const IPAddress& localIp, const IPAddress& subnetMask) {
    const uint32_t ip = ipToUint32(localIp);
    const uint32_t mask = ipToUint32(subnetMask);
    const uint32_t network = ip & mask;
    const uint32_t broadcast = network | ~mask;
    return broadcast > network + 1 ? broadcast - network - 1 : 0;
}

String FamiliarPatrolService::cidrText(const IPAddress& localIp,
                                       const IPAddress& subnetMask) {
    const uint32_t mask = ipToUint32(subnetMask);
    uint8_t prefix = 0;
    bool zeroSeen = false;
    bool contiguous = true;
    for (int bit = 31; bit >= 0; --bit) {
        const bool set = (mask & (1UL << bit)) != 0;
        if (set && zeroSeen) contiguous = false;
        if (set) ++prefix;
        else zeroSeen = true;
    }
    const IPAddress network = uint32ToIp(ipToUint32(localIp) & mask);
    return contiguous ? network.toString() + "/" + String(prefix)
                      : network.toString() + " mask " + subnetMask.toString();
}

void FamiliarPatrolService::begin() {
    resumeAvailable_ = SD.cardType() != CARD_NONE &&
                       (SD.exists(kCheckpointPath) ||
                        SD.exists(kCheckpointBackupPath));
    if (resumeAvailable_ && loadCheckpoint()) {
        resumeState_ = state_;
        state_ = FamiliarPatrolState::WaitingForNetwork;
        status_ = "Saved patrol waiting for network";
    } else {
        resumeAvailable_ = false;
    }
}

bool FamiliarPatrolService::prepareSession() {
    SD.mkdir("/ghostwire");
    SD.mkdir(kRootPath);
    for (uint16_t index = 1; index < 10000; ++index) {
        char candidate[64];
        snprintf(candidate, sizeof(candidate), "%s/patrol_%04u", kRootPath,
                 index);
        if (!SD.exists(candidate) && SD.mkdir(candidate)) {
            sessionPath_ = candidate;
            break;
        }
    }
    if (sessionPath_.isEmpty()) return false;

    File hosts = SD.open(sessionPath_ + "/hosts.csv", FILE_WRITE);
    File ports = SD.open(sessionPath_ + "/ports.csv", FILE_WRITE);
    File observations = SD.open(sessionPath_ + "/observations.jsonl", FILE_WRITE);
    if (!hosts || !ports || !observations) {
        if (hosts) hosts.close();
        if (ports) ports.close();
        if (observations) observations.close();
        return false;
    }
    hosts.println("ip_address");
    ports.println("ip_address,port,phase");
    hosts.close();
    ports.close();
    observations.close();
    if (!prepareCycleHosts()) return false;

    JsonDocument scope;
    scope["network"] = uint32ToIp(network_).toString();
    scope["subnet_mask"] = uint32ToIp(mask_).toString();
    scope["usable_addresses"] = addressCount_;
    scope["strategy"] = "icmp discovery, prioritized 100-port TCP scout scan";
    scope["tcp_ports"] = kScoutPortCount;
    File scopeFile = SD.open(sessionPath_ + "/scope.json", FILE_WRITE);
    if (!scopeFile) return false;
    serializeJsonPretty(scope, scopeFile);
    scopeFile.println();
    scopeFile.close();
    return true;
}

bool FamiliarPatrolService::start(const IPAddress& localIp,
                                  const IPAddress& subnetMask,
                                  bool continuous, uint32_t intervalMs) {
    if (SD.cardType() == CARD_NONE || WiFi.status() != WL_CONNECTED) {
        status_ = SD.cardType() == CARD_NONE ? "SD card required"
                                             : "Wi-Fi connection required";
        return false;
    }
    stop();
    network_ = ipToUint32(localIp) & ipToUint32(subnetMask);
    mask_ = ipToUint32(subnetMask);
    addressCount_ = usableAddressCount(localIp, subnetMask);
    discoveryIndex_ = hostsFound_ = hostIndex_ = 0;
    openPortsFound_ = 0;
    lastOpenPort_ = 0;
    continuous_ = continuous;
    intervalMs_ = std::max<uint32_t>(60000, intervalMs);
    cycle_ = 1;
    cycleHostsFound_ = 0;
    knownHosts_.clear();
    knownHostsOverflow_ = false;
    sessionPath_ = "";
    if (addressCount_ == 0 || !prepareSession()) {
        fail(addressCount_ == 0 ? "Subnet has no usable addresses"
                                : "Unable to create patrol session");
        return false;
    }
    state_ = FamiliarPatrolState::Discovery;
    resumeState_ = state_;
    workerStarted_ = false;
    resumeAvailable_ = true;
    appendObservation("patrol_started", localIp);
    saveCheckpoint();
    status_ = "Beginning host discovery";
    return true;
}

bool FamiliarPatrolService::loadCheckpoint() {
    const char* path = SD.exists(kCheckpointPath) ? kCheckpointPath
                                                  : kCheckpointBackupPath;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;
    if ((doc["format_version"] | 0U) != 2U) return false;
    sessionPath_ = String(doc["path"] | "");
    const uint8_t savedState = doc["state"] | 0U;
    if (!sessionPath_.startsWith(String(kRootPath) + "/patrol_") ||
        !SD.exists(sessionPath_) ||
        savedState < static_cast<uint8_t>(FamiliarPatrolState::WaitingForNetwork) ||
        savedState > static_cast<uint8_t>(FamiliarPatrolState::WatchWait)) {
        return false;
    }
    state_ = static_cast<FamiliarPatrolState>(savedState);
    network_ = doc["network"] | 0U;
    mask_ = doc["mask"] | 0U;
    addressCount_ = doc["address_count"] | 0U;
    discoveryIndex_ = doc["discovery_index"] | 0U;
    hostsFound_ = doc["hosts_found"] | 0U;
    hostIndex_ = doc["host_index"] | 0U;
    openPortsFound_ = doc["open_ports"] | 0U;
    continuous_ = doc["continuous"] | false;
    intervalMs_ = std::max<uint32_t>(60000U,
                                     doc["interval_ms"] | 300000U);
    cycle_ = doc["cycle"] | 1U;
    cycleHostsFound_ = doc["cycle_hosts"] | 0U;
    loadKnownHosts();
    return true;
}

bool FamiliarPatrolService::saveCheckpoint() {
    if (sessionPath_.isEmpty()) return false;
    SD.remove(kCheckpointTempPath);
    File file = SD.open(kCheckpointTempPath, FILE_WRITE);
    if (!file) return false;
    JsonDocument doc;
    doc["format_version"] = 2;
    doc["path"] = sessionPath_;
    doc["state"] = static_cast<uint8_t>(state_);
    doc["network"] = network_;
    doc["mask"] = mask_;
    doc["address_count"] = addressCount_;
    doc["discovery_index"] = discoveryIndex_;
    doc["hosts_found"] = hostsFound_;
    doc["host_index"] = hostIndex_;
    doc["open_ports"] = openPortsFound_;
    doc["continuous"] = continuous_;
    doc["interval_ms"] = intervalMs_;
    doc["cycle"] = cycle_;
    doc["cycle_hosts"] = cycleHostsFound_;
    serializeJson(doc, file);
    file.println();
    file.flush();
    const bool ok = !file.getWriteError();
    file.close();
    if (!ok) return false;
    SD.remove(kCheckpointBackupPath);
    if (SD.exists(kCheckpointPath) &&
        !SD.rename(kCheckpointPath, kCheckpointBackupPath)) {
        return false;
    }
    if (!SD.rename(kCheckpointTempPath, kCheckpointPath)) {
        if (SD.exists(kCheckpointBackupPath)) {
            SD.rename(kCheckpointBackupPath, kCheckpointPath);
        }
        return false;
    }
    SD.remove(kCheckpointBackupPath);
    return true;
}

void FamiliarPatrolService::removeCheckpoint() {
    SD.remove(kCheckpointTempPath);
    SD.remove(kCheckpointPath);
    SD.remove(kCheckpointBackupPath);
    resumeAvailable_ = false;
}

bool FamiliarPatrolService::appendObservation(const char* type,
                                              const IPAddress& ip,
                                              int32_t port) {
    File file = SD.open(sessionPath_ + "/observations.jsonl", FILE_APPEND);
    if (!file) return false;
    JsonDocument event;
    event["elapsed_ms"] = millis();
    event["type"] = type;
    event["ip"] = ip.toString();
    if (port >= 0) event["port"] = port;
    serializeJson(event, file);
    file.println();
    file.close();
    return true;
}

bool FamiliarPatrolService::appendHost(const IPAddress& ip) {
    File file = SD.open(sessionPath_ + "/hosts.csv", FILE_APPEND);
    if (!file) return false;
    file.println(ip.toString());
    file.close();
    File cycleFile = SD.open(sessionPath_ + "/cycle_hosts.csv", FILE_APPEND);
    if (!cycleFile) return false;
    cycleFile.println(ip.toString());
    cycleFile.close();
    if (knownHosts_.size() < kKnownHostCacheLimit) {
        knownHosts_.push_back(ipToUint32(ip));
    } else {
        knownHostsOverflow_ = true;
    }
    return appendObservation("host_seen", ip);
}

bool FamiliarPatrolService::isKnownHost(const IPAddress& ip) const {
    const uint32_t value = ipToUint32(ip);
    if (std::find(knownHosts_.begin(), knownHosts_.end(), value) !=
        knownHosts_.end()) {
        return true;
    }
    if (!knownHostsOverflow_) return false;

    // Large scopes remain exact without allowing the RAM cache to grow with
    // subnet size. The uncommon overflow path trades an SD scan for bounded
    // memory; ordinary /24 patrols stay entirely in the fast cache.
    File file = SD.open(sessionPath_ + "/hosts.csv", FILE_READ);
    if (!file) return false;
    file.readStringUntil('\n');
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        IPAddress candidate;
        if (candidate.fromString(line) && ipToUint32(candidate) == value) {
            file.close();
            return true;
        }
    }
    file.close();
    return false;
}

void FamiliarPatrolService::loadKnownHosts() {
    knownHosts_.clear();
    knownHostsOverflow_ = false;
    File file = SD.open(sessionPath_ + "/hosts.csv", FILE_READ);
    if (!file) return;
    file.readStringUntil('\n');
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        IPAddress ip;
        if (!ip.fromString(line)) continue;
        if (knownHosts_.size() < kKnownHostCacheLimit) {
            knownHosts_.push_back(ipToUint32(ip));
        } else {
            knownHostsOverflow_ = true;
        }
    }
    file.close();
}

bool FamiliarPatrolService::prepareCycleHosts() {
    const String path = sessionPath_ + "/cycle_hosts.csv";
    SD.remove(path);
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    file.println("ip_address");
    file.close();
    return true;
}

bool FamiliarPatrolService::appendPort(const IPAddress& ip, uint16_t port,
                                      const char* phase) {
    File file = SD.open(sessionPath_ + "/ports.csv", FILE_APPEND);
    if (!file) return false;
    file.printf("%s,%u,%s\n", ip.toString().c_str(), port, phase);
    file.close();
    return appendObservation("port_open", ip, port);
}

bool FamiliarPatrolService::writeReport() {
    File file = SD.open(sessionPath_ + "/report.md", FILE_WRITE);
    if (!file) return false;
    file.println("# Ghostwire Familiar Patrol report");
    file.println();
    file.printf("- Scope: `%s`\n",
                cidrText(uint32ToIp(network_ + 1), uint32ToIp(mask_)).c_str());
    file.printf("- Usable addresses considered: %lu\n",
                static_cast<unsigned long>(addressCount_));
    file.printf("- ICMP-responsive hosts: %lu\n",
                static_cast<unsigned long>(hostsFound_));
    file.printf("- Open-port observations: %lu\n",
                static_cast<unsigned long>(openPortsFound_));
    file.println("- Strategy: ICMP discovery and prioritized 100-port TCP scout scan");
    file.println();
    file.println("Evidence is stored in `hosts.csv`, `ports.csv`, and");
    file.println("`observations.jsonl`. An open port is an exposure observation,");
    file.println("not proof of a vulnerability; validate services manually.");
    file.flush();
    const bool ok = !file.getWriteError();
    file.close();
    return ok;
}

bool FamiliarPatrolService::networkMatches() const {
    if (WiFi.status() != WL_CONNECTED) return false;
    return (ipToUint32(WiFi.localIP()) & ipToUint32(WiFi.subnetMask())) ==
               network_ &&
           ipToUint32(WiFi.subnetMask()) == mask_;
}

void FamiliarPatrolService::enterDiscovery() {
    state_ = FamiliarPatrolState::Discovery;
    workerStarted_ = hostScanner_.start(discoveryIndex_, UINT32_MAX);
    status_ = workerStarted_ ? "Discovering authorized subnet"
                             : "Unable to start discovery";
}

bool FamiliarPatrolService::ensureHostReader() {
    if (hostsReader_) return true;
    hostsReader_ = SD.open(sessionPath_ + "/cycle_hosts.csv", FILE_READ);
    if (!hostsReader_) return false;
    hostsReader_.readStringUntil('\n');
    for (uint32_t index = 0; index < hostIndex_ && hostsReader_.available();
         ++index) {
        hostsReader_.readStringUntil('\n');
    }
    return true;
}

bool FamiliarPatrolService::loadNextHost() {
    if (!ensureHostReader()) return false;
    while (hostsReader_.available()) {
        String line = hostsReader_.readStringUntil('\n');
        line.trim();
        if (!line.isEmpty() && currentHost_.fromString(line)) {
            hostLoaded_ = true;
            return true;
        }
    }
    hostLoaded_ = false;
    return false;
}

void FamiliarPatrolService::enterCommonPorts() {
    hostScanner_.stop();
    discoveryIndex_ = addressCount_;
    state_ = FamiliarPatrolState::CommonPorts;
    hostIndex_ = 0;
    hostLoaded_ = false;
    closeFiles();
    workerStarted_ = false;
    status_ = "Starting prioritized scout scan";
    saveCheckpoint();
}

void FamiliarPatrolService::startCommonProbe() {
    if (!hostLoaded_ && !loadNextHost()) {
        finishCycle();
        return;
    }
    workerStarted_ =
        portScanner_.start(currentHost_, kScoutPorts, kScoutPortCount);
    status_ = "Scouting " + currentHost_.toString();
}

void FamiliarPatrolService::update() {
    if (!isActive()) return;
    if (WiFi.status() != WL_CONNECTED) {
        hostScanner_.stop();
        portScanner_.stop();
        workerStarted_ = false;
        if (state_ != FamiliarPatrolState::WaitingForNetwork) {
            resumeState_ = state_;
            state_ = FamiliarPatrolState::WaitingForNetwork;
            status_ = "Paused - Wi-Fi disconnected";
        }
        return;
    }
    if (state_ == FamiliarPatrolState::WaitingForNetwork) {
        if (!networkMatches()) {
            status_ = "Connect to saved patrol subnet";
            return;
        }
        state_ = resumeState_;
        workerStarted_ = false;
        status_ = "Resuming saved patrol";
    }
    if (!networkMatches()) {
        fail("Connected subnet differs from confirmed scope");
        return;
    }

    if (state_ == FamiliarPatrolState::WatchWait) {
        if (static_cast<int32_t>(millis() - nextScanMs_) >= 0) {
            beginNextCycle();
        }
        return;
    }

    if (state_ == FamiliarPatrolState::Discovery) {
        if (!workerStarted_) enterDiscovery();
        if (!workerStarted_) { fail(status_); return; }
        hostScanner_.update();
        NetworkHostResult result;
        bool foundHost = false;
        while (hostScanner_.nextHostResult(result)) {
            if (isKnownHost(result.ip)) continue;
            if (!appendHost(result.ip)) { fail("SD write failed"); return; }
            lastSeenHost_ = result.ip;
            ++hostsFound_;
            ++cycleHostsFound_;
            foundHost = true;
        }
        const uint32_t previousDiscoveryIndex = discoveryIndex_;
        discoveryIndex_ = hostScanner_.scannedCount();
        const bool checkpointBoundary =
            discoveryIndex_ != previousDiscoveryIndex &&
            (discoveryIndex_ & 31U) == 0;
        if (foundHost || checkpointBoundary) saveCheckpoint();
        if (!hostScanner_.isActive()) enterCommonPorts();
        return;
    }

    if (state_ == FamiliarPatrolState::CommonPorts) {
        if (!workerStarted_) startCommonProbe();
        if (state_ != FamiliarPatrolState::CommonPorts) return;
        portScanner_.update();
        NetworkPortResult result;
        while (portScanner_.nextPortResult(result)) {
            if (!appendPort(currentHost_, result.port, "scout")) {
                fail("SD write failed"); return;
            }
            lastOpenHost_ = currentHost_;
            lastOpenPort_ = result.port;
            ++openPortsFound_;
        }
        if (!portScanner_.isActive()) {
            workerStarted_ = false;
            ++hostIndex_;
            hostLoaded_ = false;
            saveCheckpoint();
        }
        if (hostIndex_ >= cycleHostsFound_ && !hostLoaded_ && !workerStarted_) {
            finishCycle();
        }
        return;
    }
}

void FamiliarPatrolService::finishCycle() {
    state_ = continuous_ ? FamiliarPatrolState::WatchWait
                         : FamiliarPatrolState::Complete;
    status_ = continuous_ ? "Watching for new hosts"
                          : "Scout patrol complete";
    appendObservation("patrol_complete", WiFi.localIP());
    writeReport();
    if (continuous_) {
        nextScanMs_ = millis() + intervalMs_;
        saveCheckpoint();
    } else {
        removeCheckpoint();
    }
    closeFiles();
}

void FamiliarPatrolService::closeFiles() {
    if (hostsReader_) hostsReader_.close();
}

void FamiliarPatrolService::fail(const String& message) {
    hostScanner_.stop();
    portScanner_.stop();
    workerStarted_ = false;
    state_ = FamiliarPatrolState::Error;
    status_ = message;
    saveCheckpoint();
    closeFiles();
}

void FamiliarPatrolService::stop() {
    hostScanner_.stop();
    portScanner_.stop();
    workerStarted_ = false;
    closeFiles();
    if (state_ != FamiliarPatrolState::Idle &&
        state_ != FamiliarPatrolState::Complete) {
        state_ = FamiliarPatrolState::Stopped;
        status_ = "Patrol stopped";
    }
    removeCheckpoint();
}

bool FamiliarPatrolService::isActive() const {
    return state_ == FamiliarPatrolState::WaitingForNetwork ||
           state_ == FamiliarPatrolState::Discovery ||
           state_ == FamiliarPatrolState::CommonPorts ||
           state_ == FamiliarPatrolState::WatchWait;
}

const char* FamiliarPatrolService::stateName() const {
    switch (state_) {
        case FamiliarPatrolState::WaitingForNetwork: return "WAITING";
        case FamiliarPatrolState::Discovery: return "DISCOVERY";
        case FamiliarPatrolState::CommonPorts: return "SCOUTING";
        case FamiliarPatrolState::WatchWait: return "WATCHING";
        case FamiliarPatrolState::Complete: return "COMPLETE";
        case FamiliarPatrolState::Stopped: return "STOPPED";
        case FamiliarPatrolState::Error: return "ERROR";
        default: return "IDLE";
    }
}

uint32_t FamiliarPatrolService::nextScanSeconds() const {
    if (state_ != FamiliarPatrolState::WatchWait ||
        static_cast<int32_t>(nextScanMs_ - millis()) <= 0) return 0;
    return (nextScanMs_ - millis() + 999) / 1000;
}

void FamiliarPatrolService::beginNextCycle() {
    ++cycle_;
    cycleHostsFound_ = 0;
    hostIndex_ = 0;
    discoveryIndex_ = 0;
    hostLoaded_ = false;
    workerStarted_ = false;
    closeFiles();
    if (!prepareCycleHosts()) { fail("Unable to create watch cycle"); return; }
    state_ = FamiliarPatrolState::Discovery;
    resumeState_ = state_;
    status_ = "Looking for new hosts";
    saveCheckpoint();
}

uint32_t FamiliarPatrolService::discoveryScanned() const {
    return state_ == FamiliarPatrolState::Discovery
               ? hostScanner_.scannedCount() : discoveryIndex_;
}

uint32_t FamiliarPatrolService::currentPort() const {
    if (state_ == FamiliarPatrolState::CommonPorts &&
        portScanner_.isActive()) {
        const size_t index = std::min<size_t>(
            portScanner_.scannedCount(), kScoutPortCount - 1);
        return kScoutPorts[index];
    }
    return 0;
}
