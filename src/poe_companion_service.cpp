#include "poe_companion_service.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace {
constexpr char kService[] = "ghostwire";
constexpr char kProtocol[] = "tcp";
constexpr char kFallbackHost[] = "ghostwire-poe-p4";
constexpr char kExpectedProtocol[] = "ghostwire-companion";
constexpr uint16_t kFallbackPort = 8765;
constexpr uint8_t kProtocolVersion = 1;
constexpr int kMaxStatusBytes = 1536;
constexpr uint16_t kHttpTimeoutMs = 2500;
// discover() blocks the whole loop for up to ~4.5s (3s mDNS PTR query plus a
// 1.5s fallback host query). Retrying that on every 10s poll while the
// companion stays unfound was stalling the Grove UART link long enough to
// approach its own 5s timeout. Back off between attempts instead.
constexpr unsigned long kDiscoveryBackoffMs = 30000;

void hexEncode(char* dest, const uint8_t* src, size_t srcLen) {
    static const char kDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < srcLen; ++i) {
        dest[i * 2] = kDigits[src[i] >> 4];
        dest[i * 2 + 1] = kDigits[src[i] & 0x0F];
    }
    dest[srcLen * 2] = '\0';
}
}  // namespace

void PoeCompanionService::clearDeviceState() {
    deviceId_ = "";
    model_ = "";
    firmwareVersion_ = "";
    companionIp_ = "";
    reportedIp_ = "";
    gateway_ = "";
    dns_ = "";
    port_ = 0;
    ethernetStarted_ = false;
    ethernetLinkUp_ = false;
    internetReachable_ = false;
    ghostwireConnected_ = false;
    indicatorState_ = "";
    groveConnected_ = false;
    groveSequence_ = 0;
    linkSpeedMbps_ = 0;
    fullDuplex_ = false;
    relayUptimeMs_ = 0;
    resetReason_ = "";
    hasTemperature_ = false;
    temperatureC_ = 0.0f;
    freeHeapBytes_ = 0;
    minimumFreeHeapBytes_ = 0;
    payloadStateCode_ = 'I';
    payloadFindingCount_ = 0;
    lastSuccessMs_ = 0;
    consecutiveFailures_ = 0;
}

const char* PoeCompanionService::payloadRunState() const {
    switch (payloadStateCode_) {
        case 'R': return "running";
        case 'S': return "success";
        case 'E': return "error";
        case 'I':
        default: return "idle";
    }
}

bool PoeCompanionService::ensureMdns() {
    if (mdnsStarted_) return true;
    mdnsStarted_ = MDNS.begin("ghostwire-cardputer");
    return mdnsStarted_;
}

bool PoeCompanionService::discover(IPAddress& address, uint16_t& port) {
    if (ensureMdns()) {
        const int count = MDNS.queryService(kService, kProtocol);
        for (int index = 0; index < count; ++index) {
            const String advertisedProtocol = MDNS.txt(index, "proto");
            const String role = MDNS.txt(index, "role");
            const String path = MDNS.txt(index, "path");
            if (advertisedProtocol != "1" || role != "poe" ||
                path != "/v1/status") {
                continue;
            }
            address = MDNS.IP(index);
            port = MDNS.port(index);
            if (address != INADDR_NONE && port != 0) return true;
        }

        // The fixed hostname is a compatibility fallback for multicast
        // networks that resolve hosts but suppress DNS-SD browse responses.
        address = MDNS.queryHost(kFallbackHost, 1500);
        if (address != INADDR_NONE) {
            port = kFallbackPort;
            return true;
        }
    }

    return false;
}

bool PoeCompanionService::fetchStatus(const IPAddress& address, uint16_t port) {
    WiFiClient client;
    HTTPClient http;
    const String url = "http://" + address.toString() + ":" + String(port) +
                       "/v1/status";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(client, url)) {
        statusMessage_ = "Could not start status request";
        state_ = PoeCompanionState::Error;
        return false;
    }
    http.addHeader("Accept", "application/json");
    const int responseCode = http.GET();
    if (responseCode != HTTP_CODE_OK) {
        statusMessage_ = responseCode > 0
                             ? "Status HTTP " + String(responseCode)
                             : "Status request timed out";
        state_ = PoeCompanionState::Error;
        http.end();
        return false;
    }
    const int contentLength = http.getSize();
    if (contentLength > kMaxStatusBytes) {
        statusMessage_ = "Status response is too large";
        state_ = PoeCompanionState::Incompatible;
        http.end();
        return false;
    }

    JsonDocument document;
    const DeserializationError parseError =
        deserializeJson(document, http.getStream());
    http.end();
    if (parseError) {
        statusMessage_ = "Invalid companion JSON";
        state_ = PoeCompanionState::Incompatible;
        return false;
    }

    const String protocol = document["protocol"] | "";
    const int protocolVersion = document["protocol_version"] | 0;
    const String deviceId = document["device"]["id"] | "";
    const String model = document["device"]["model"] | "";
    const String firmware = document["device"]["firmware"] | "";
    bool supportsStatus = false;
    bool supportsEvents = false;
    for (JsonVariant capability : document["capabilities"].as<JsonArray>()) {
        const String value = capability.as<String>();
        supportsStatus |= value == "status";
        supportsEvents |= value == "events";
    }

    if (protocol != kExpectedProtocol || protocolVersion != kProtocolVersion ||
        deviceId.isEmpty() || model.isEmpty() || firmware.isEmpty() ||
        !supportsStatus || !supportsEvents) {
        statusMessage_ = "Unsupported companion protocol";
        state_ = PoeCompanionState::Incompatible;
        return false;
    }

    deviceId_ = deviceId;
    model_ = model;
    firmwareVersion_ = firmware;
    companionIp_ = address.toString();
    reportedIp_ = String(document["ethernet"]["ip"] | "");
    gateway_ = String(document["ethernet"]["gateway"] | "");
    dns_ = String(document["ethernet"]["dns"] | "");
    port_ = port;
    ethernetStarted_ = document["ethernet"]["started"] | false;
    ethernetLinkUp_ = document["ethernet"]["link"] | false;
    internetReachable_ = document["internet"]["reachable"] | false;
    ghostwireConnected_ = document["ghostwire"]["connected"] | false;
    indicatorState_ = String(document["indicator"]["state"] | "");
    groveConnected_ = document["grove"]["connected"] | false;
    groveSequence_ = document["grove"]["last_sequence"] | 0;
    linkSpeedMbps_ = document["ethernet"]["speed_mbps"] | 0;
    fullDuplex_ = document["ethernet"]["full_duplex"] | false;
    relayUptimeMs_ = document["system"]["uptime_ms"] | 0ULL;
    resetReason_ = String(document["system"]["reset_reason"] | "unknown");
    hasTemperature_ = document["system"]["temperature_c"].is<float>();
    temperatureC_ = document["system"]["temperature_c"] | 0.0f;
    freeHeapBytes_ = document["system"]["free_heap_bytes"] | 0;
    minimumFreeHeapBytes_ =
        document["system"]["minimum_free_heap_bytes"] | 0;
    {
        const String payloadState = document["payload"]["state"] | "idle";
        payloadStateCode_ = payloadState == "running" ? 'R'
                            : payloadState == "success" ? 'S'
                            : payloadState == "error"   ? 'E'
                                                        : 'I';
    }
    payloadFindingCount_ =
        static_cast<size_t>(document["payload"]["finding_count"] | 0);
    state_ = PoeCompanionState::Ready;
    statusMessage_ = ethernetLinkUp_ ? "Companion online" : "Companion link down";
    lastSuccessMs_ = millis();
    consecutiveFailures_ = 0;
    return true;
}

bool PoeCompanionService::refresh() {
    clearDeviceState();
    lastRefreshMs_ = millis();
    lastDiscoveryAttemptMs_ = lastRefreshMs_;
    if (WiFi.status() != WL_CONNECTED) {
        state_ = PoeCompanionState::WifiRequired;
        statusMessage_ = "Connect the Cardputer to Wi-Fi";
        return false;
    }

    state_ = PoeCompanionState::Discovering;
    statusMessage_ = "Looking for _ghostwire._tcp";
    IPAddress address;
    uint16_t port = 0;
    if (!discover(address, port)) {
        state_ = PoeCompanionState::NotFound;
        statusMessage_ = "No PoE companion found";
        return false;
    }
    return fetchStatus(address, port);
}

bool PoeCompanionService::poll() {
    if (WiFi.status() != WL_CONNECTED) {
        lastRefreshMs_ = millis();
        if (lastSuccessMs_ == 0) return refresh();
        if (consecutiveFailures_ < UINT8_MAX) ++consecutiveFailures_;
        state_ = consecutiveFailures_ >= 3 ? PoeCompanionState::Offline
                                           : PoeCompanionState::Stale;
        statusMessage_ = state_ == PoeCompanionState::Offline
                             ? "Relay offline: Wi-Fi unavailable"
                             : "Telemetry stale: Wi-Fi unavailable";
        return false;
    }
    if (companionIp_.isEmpty() || port_ == 0) {
        const unsigned long now = millis();
        if (lastDiscoveryAttemptMs_ != 0 &&
            now - lastDiscoveryAttemptMs_ < kDiscoveryBackoffMs) {
            // Still cooling down after a recent failed discovery; report the
            // existing failure without repeating the blocking mDNS scan.
            lastRefreshMs_ = now;
            if (consecutiveFailures_ < UINT8_MAX) ++consecutiveFailures_;
            state_ = consecutiveFailures_ >= 3 ? PoeCompanionState::Offline
                                               : PoeCompanionState::NotFound;
            statusMessage_ = "No PoE companion found (retrying periodically)";
            return false;
        }
        return refresh();
    }
    IPAddress address;
    if (!address.fromString(companionIp_)) return refresh();
    lastRefreshMs_ = millis();
    if (fetchStatus(address, port_)) return true;

    if (consecutiveFailures_ < UINT8_MAX) ++consecutiveFailures_;
    const String failure = statusMessage_;
    state_ = consecutiveFailures_ >= 3 ? PoeCompanionState::Offline
                                       : PoeCompanionState::Stale;
    statusMessage_ = state_ == PoeCompanionState::Offline
                         ? "Relay offline: " + failure
                         : "Telemetry stale: " + failure;
    return false;
}

bool PoeCompanionService::sendCommand(uint8_t slot, uint32_t nonce,
                                      const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES]) {
    if (WiFi.status() != WL_CONNECTED || companionIp_.isEmpty() || port_ == 0) {
        return false;
    }
    IPAddress address;
    if (!address.fromString(companionIp_)) return false;

    char tagHex[GHOSTWIRE_AUTH_TAG_BYTES * 2 + 1];
    hexEncode(tagHex, tag, GHOSTWIRE_AUTH_TAG_BYTES);
    char body[96];
    const int bodyLength =
        snprintf(body, sizeof(body), "{\"slot\":%u,\"nonce\":%lu,\"tag\":\"%s\"}",
                static_cast<unsigned>(slot), static_cast<unsigned long>(nonce), tagHex);
    if (bodyLength < 0 || static_cast<size_t>(bodyLength) >= sizeof(body)) return false;

    WiFiClient client;
    HTTPClient http;
    const String url =
        "http://" + address.toString() + ":" + String(port_) + "/v1/command";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    const int responseCode = http.POST(reinterpret_cast<uint8_t*>(body), bodyLength);
    if (responseCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    JsonDocument document;
    const DeserializationError parseError = deserializeJson(document, http.getStream());
    http.end();
    if (parseError) return false;
    return document["accepted"] | false;
}

bool PoeCompanionService::sendScript(uint8_t slot, uint32_t nonce,
                                     const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES],
                                     const String& script) {
    if (WiFi.status() != WL_CONNECTED || companionIp_.isEmpty() || port_ == 0) {
        return false;
    }
    IPAddress address;
    if (!address.fromString(companionIp_)) return false;

    char tagHex[GHOSTWIRE_AUTH_TAG_BYTES * 2 + 1];
    hexEncode(tagHex, tag, GHOSTWIRE_AUTH_TAG_BYTES);
    // Built via ArduinoJson (not snprintf, unlike sendCommand()'s small
    // fixed-shape body) so the script's own newlines/quotes get proper JSON
    // string escaping rather than needing that done by hand.
    JsonDocument requestDocument;
    requestDocument["slot"] = slot;
    requestDocument["nonce"] = nonce;
    requestDocument["tag"] = tagHex;
    requestDocument["script"] = script;
    String body;
    serializeJson(requestDocument, body);

    WiFiClient client;
    HTTPClient http;
    const String url =
        "http://" + address.toString() + ":" + String(port_) + "/v1/payload";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    const int responseCode = http.POST(body);
    if (responseCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    JsonDocument responseDocument;
    const DeserializationError parseError = deserializeJson(responseDocument, http.getStream());
    http.end();
    if (parseError) return false;
    return responseDocument["accepted"] | false;
}

bool PoeCompanionService::fetchLoot(uint32_t nonce, const uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES],
                                    std::vector<LootEntry>& out) {
    out.clear();
    if (WiFi.status() != WL_CONNECTED || companionIp_.isEmpty() || port_ == 0) {
        return false;
    }
    IPAddress address;
    if (!address.fromString(companionIp_)) return false;

    char tagHex[GHOSTWIRE_AUTH_TAG_BYTES * 2 + 1];
    hexEncode(tagHex, tag, GHOSTWIRE_AUTH_TAG_BYTES);
    char body[96];
    const int bodyLength = snprintf(body, sizeof(body), "{\"nonce\":%lu,\"tag\":\"%s\"}",
                                    static_cast<unsigned long>(nonce), tagHex);
    if (bodyLength < 0 || static_cast<size_t>(bodyLength) >= sizeof(body)) return false;

    WiFiClient client;
    HTTPClient http;
    const String url =
        "http://" + address.toString() + ":" + String(port_) + "/v1/loot";
    http.setTimeout(kHttpTimeoutMs);
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    const int responseCode = http.POST(reinterpret_cast<uint8_t*>(body), bodyLength);
    if (responseCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    JsonDocument document;
    const DeserializationError parseError = deserializeJson(document, http.getStream());
    http.end();
    if (parseError) return false;
    for (JsonVariant entry : document["entries"].as<JsonArray>()) {
        LootEntry loot;
        loot.ip = entry["ip"] | "";
        loot.port = entry["port"] | 0;
        if (!loot.ip.isEmpty()) out.push_back(loot);
    }
    return true;
}
