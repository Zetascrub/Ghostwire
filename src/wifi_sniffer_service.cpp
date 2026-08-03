#include "wifi_sniffer_service.h"

#include <WiFi.h>
#include <algorithm>
#include <cstring>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
constexpr size_t kRingCapacity = 24;
constexpr uint8_t kHopChannels[] = {1, 6, 11};
constexpr size_t kHopChannelCount = sizeof(kHopChannels) / sizeof(kHopChannels[0]);
constexpr unsigned long kHopIntervalMs = 400;
constexpr size_t kRawRingCapacity = 8;

StaticQueue_t probeQueueControl;
uint8_t probeQueueStorage[kRingCapacity * sizeof(WifiProbeRecord)];
QueueHandle_t probeQueue = nullptr;
StaticQueue_t rawQueueControl;
uint8_t rawQueueStorage[kRawRingCapacity * sizeof(WifiRawFrameRecord)];
QueueHandle_t rawQueue = nullptr;
portMUX_TYPE targetMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t droppedProbes = 0;
volatile uint32_t droppedRawFrames = 0;

// Written by setHandshakeTarget()/clearHandshakeTarget() (main thread)
// before handshakeCaptureActive is published; read by the promiscuous
// callback. The bool gates whether the callback ever looks at the BSSID,
// so the BSSID write always happens-before the callback can observe it.
uint8_t handshakeTargetBssid[6] = {};
volatile bool handshakeCaptureActive = false;
volatile WifiCaptureMode callbackCaptureMode = WifiCaptureMode::Probes;

void queueRawFrame(const wifi_promiscuous_pkt_t* packet, bool isEapol,
                   size_t eapolOffset = 0) {
    WifiRawFrameRecord record{};
    const size_t length = packet->rx_ctrl.sig_len;
    record.length = std::min<size_t>(length, WifiRawFrameRecord::kMaxLength);
    memcpy(record.data, packet->payload, record.length);
    record.eapolOffset = eapolOffset;
    record.isEapol = isEapol;
    record.channel = packet->rx_ctrl.channel;
    record.rssi = static_cast<int8_t>(packet->rx_ctrl.rssi);
    if (rawQueue == nullptr || xQueueSend(rawQueue, &record, 0) != pdTRUE) {
        ++droppedRawFrames;
    }
}

void handleProbeRequest(const wifi_promiscuous_pkt_t* packet) {
    const uint8_t* payload = packet->payload;
    const uint32_t length = packet->rx_ctrl.sig_len;
    // Probe request: frame control byte masks to type=management, subtype=4.
    if (length < 24 || (payload[0] & 0xFC) != 0x40) return;

    WifiProbeRecord record{};
    memcpy(record.mac, payload + 10, 6);
    record.rssi = static_cast<int8_t>(packet->rx_ctrl.rssi);
    record.channel = packet->rx_ctrl.channel;

    size_t ssidLength = 0;
    if (length >= 26) {
        const uint8_t tag = payload[24];
        const uint8_t tagLength = payload[25];
        if (tag == 0 && static_cast<uint32_t>(26 + tagLength) <= length) {
            ssidLength = std::min<size_t>(tagLength, sizeof(record.ssid) - 1);
            memcpy(record.ssid, payload + 26, ssidLength);
        }
    }
    record.ssid[ssidLength] = '\0';

    if (probeQueue == nullptr || xQueueSend(probeQueue, &record, 0) != pdTRUE) {
        ++droppedProbes;
    }
}

void handleManagementFrame(const wifi_promiscuous_pkt_t* packet) {
    handleProbeRequest(packet);
    if (callbackCaptureMode == WifiCaptureMode::Management ||
        callbackCaptureMode == WifiCaptureMode::Full) {
        queueRawFrame(packet, false);
    }
}

void handleDataFrame(const wifi_promiscuous_pkt_t* packet) {
    uint8_t targetBssid[6];
    portENTER_CRITICAL(&targetMux);
    const bool captureActive = handshakeCaptureActive;
    memcpy(targetBssid, handshakeTargetBssid, sizeof(targetBssid));
    portEXIT_CRITICAL(&targetMux);
    const uint8_t* payload = packet->payload;
    const uint32_t length = packet->rx_ctrl.sig_len;
    if (length < 24) return;
    const bool fullCapture = callbackCaptureMode == WifiCaptureMode::Full;

    const uint8_t frameControl0 = payload[0];
    const uint8_t frameControl1 = payload[1];
    if ((frameControl1 & 0x03) == 0x03) {
        if (fullCapture) queueRawFrame(packet, false);
        return;  // 4-address/WDS: not parsed for EAPOL.
    }
    const bool isQosData = (frameControl0 & 0x80) != 0;

    // Addr1 @ +4, Addr2 @ +10, Addr3 @ +16 in the standard 24-byte header.
    const bool targetMatches =
        memcmp(payload + 4, targetBssid, 6) == 0 ||
        memcmp(payload + 10, targetBssid, 6) == 0 ||
        memcmp(payload + 16, targetBssid, 6) == 0;
    if (!captureActive || !targetMatches) {
        if (fullCapture) queueRawFrame(packet, false);
        return;
    }

    size_t offset = 24 + (isQosData ? 2 : 0);
    if (offset + 8 > length) {
        if (fullCapture) queueRawFrame(packet, false);
        return;
    }
    // LLC/SNAP: AA AA 03 00 00 00 <2-byte EtherType>. EAPOL == 0x888E.
    if (payload[offset] != 0xAA || payload[offset + 1] != 0xAA ||
        payload[offset + 2] != 0x03 || payload[offset + 3] != 0x00 ||
        payload[offset + 4] != 0x00 || payload[offset + 5] != 0x00 ||
        payload[offset + 6] != 0x88 || payload[offset + 7] != 0x8E) {
        if (fullCapture) queueRawFrame(packet, false);
        return;
    }

    queueRawFrame(packet, true, offset + 8);
}

void IRAM_ATTR promiscuousCallback(void* buffer, wifi_promiscuous_pkt_type_t type) {
    const auto* packet = static_cast<const wifi_promiscuous_pkt_t*>(buffer);
    if (type == WIFI_PKT_DATA) {
        handleDataFrame(packet);
    } else if (type == WIFI_PKT_MGMT) {
        handleManagementFrame(packet);
    }
}
}  // namespace

bool WifiSnifferService::begin() {
    if (active_) return true;
    if (probeQueue == nullptr) {
        probeQueue = xQueueCreateStatic(kRingCapacity, sizeof(WifiProbeRecord),
                                        probeQueueStorage, &probeQueueControl);
    }
    if (rawQueue == nullptr) {
        rawQueue = xQueueCreateStatic(kRawRingCapacity,
                                      sizeof(WifiRawFrameRecord),
                                      rawQueueStorage, &rawQueueControl);
    }
    xQueueReset(probeQueue);
    xQueueReset(rawQueue);
    portENTER_CRITICAL(&targetMux);
    handshakeCaptureActive = false;
    portEXIT_CRITICAL(&targetMux);
    droppedProbes = 0;
    droppedRawFrames = 0;
    handshakeTargetSet_ = false;
    callbackCaptureMode = captureMode_;
    probeCount_ = 0;
    uniqueDeviceCount_ = 0;
    hopIndex_ = 0;
    channel_ = kHopChannels[0];
    lastHopMs_ = millis();

    WiFi.mode(WIFI_STA);
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                         (captureMode_ == WifiCaptureMode::Full
                              ? WIFI_PROMIS_FILTER_MASK_DATA
                              : 0);
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&promiscuousCallback);
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    active_ = esp_wifi_set_promiscuous(true) == ESP_OK;
    return active_;
}

void WifiSnifferService::end() {
    if (!active_) return;
    esp_wifi_set_promiscuous(false);
    active_ = false;
    portENTER_CRITICAL(&targetMux);
    handshakeCaptureActive = false;
    portEXIT_CRITICAL(&targetMux);
    handshakeTargetSet_ = false;
}

void WifiSnifferService::setHandshakeTarget(const uint8_t bssid[6]) {
    portENTER_CRITICAL(&targetMux);
    memcpy(handshakeTargetBssid, bssid, 6);
    handshakeCaptureActive = true;
    portEXIT_CRITICAL(&targetMux);
    if (rawQueue != nullptr) xQueueReset(rawQueue);
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filter);
    handshakeTargetSet_ = true;
}

void WifiSnifferService::clearHandshakeTarget() {
    portENTER_CRITICAL(&targetMux);
    handshakeCaptureActive = false;
    portEXIT_CRITICAL(&targetMux);
    handshakeTargetSet_ = false;
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                         (captureMode_ == WifiCaptureMode::Full
                              ? WIFI_PROMIS_FILTER_MASK_DATA
                              : 0);
    esp_wifi_set_promiscuous_filter(&filter);
}

bool WifiSnifferService::nextRawFrame(WifiRawFrameRecord& record) {
    return rawQueue != nullptr && xQueueReceive(rawQueue, &record, 0) == pdTRUE;
}

uint32_t WifiSnifferService::droppedProbeCount() const {
    return droppedProbes;
}

uint32_t WifiSnifferService::droppedRawFrameCount() const {
    return droppedRawFrames;
}

void WifiSnifferService::hopChannelIfDue() {
    if (channelLocked_) return;
    const unsigned long now = millis();
    if (now - lastHopMs_ < kHopIntervalMs) return;
    lastHopMs_ = now;
    hopIndex_ = (hopIndex_ + 1) % kHopChannelCount;
    channel_ = kHopChannels[hopIndex_];
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
}

const char* WifiSnifferService::captureModeName() const {
    switch (captureMode_) {
        case WifiCaptureMode::Management: return "MGMT PCAP";
        case WifiCaptureMode::Full: return "FULL PCAP";
        default: return "PROBES";
    }
}

void WifiSnifferService::cycleCaptureMode() {
    captureMode_ = static_cast<WifiCaptureMode>(
        (static_cast<uint8_t>(captureMode_) + 1U) % 3U);
    callbackCaptureMode = captureMode_;
    if (active_) {
        wifi_promiscuous_filter_t filter{};
        filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                             ((captureMode_ == WifiCaptureMode::Full ||
                               handshakeTargetSet_)
                                  ? WIFI_PROMIS_FILTER_MASK_DATA
                                  : 0);
        esp_wifi_set_promiscuous_filter(&filter);
        if (rawQueue != nullptr) xQueueReset(rawQueue);
    }
}

void WifiSnifferService::toggleChannelLock() {
    channelLocked_ = !channelLocked_;
    lastHopMs_ = millis();
}

void WifiSnifferService::update() {
    if (!active_) return;
    hopChannelIfDue();
}

void WifiSnifferService::noteUniqueMac(const uint8_t* mac) {
    for (size_t index = 0; index < uniqueDeviceCount_; ++index) {
        if (memcmp(trackedMacs_[index], mac, 6) == 0) return;
    }
    if (uniqueDeviceCount_ < kMaxTrackedMacs) {
        memcpy(trackedMacs_[uniqueDeviceCount_], mac, 6);
        ++uniqueDeviceCount_;
    }
}

bool WifiSnifferService::nextRecord(WifiProbeRecord& record) {
    if (probeQueue == nullptr || xQueueReceive(probeQueue, &record, 0) != pdTRUE) {
        return false;
    }
    ++probeCount_;
    noteUniqueMac(record.mac);
    return true;
}
