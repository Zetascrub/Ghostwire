#include "chameleon_ultra_client.h"

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <algorithm>
#include <cstring>

namespace {
constexpr char kDeviceName[] = "ChameleonUltra";

// Standard Nordic UART Service UUIDs -- a de facto industry-wide UUID
// scheme, not Chameleon-specific, safe to reuse directly (same category as
// the public company/service IDs used in ble_spam_service.cpp).
const NimBLEUUID kServiceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
const NimBLEUUID kWriteCharUuid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
const NimBLEUUID kNotifyCharUuid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

// Command IDs and frame format per the official, independently-read
// RfidResearchGroup/ChameleonUltraDocs protocol.md (see the plan file /
// CHANGELOG for the full citation) -- not copied from any AGPL source.
constexpr uint16_t kCmdChangeDeviceMode = 1001;
constexpr uint16_t kCmdGetDeviceMode = 1002;
constexpr uint16_t kCmdGetAppVersion = 1000;
constexpr uint16_t kCmdGetBatteryInfo = 1025;
constexpr uint16_t kCmdHf14aScan = 2000;
constexpr uint16_t kCmdEm410xScan = 3000;
constexpr uint16_t kCmdSetActiveSlot = 1003;
constexpr uint16_t kCmdSetSlotTagType = 1004;
constexpr uint16_t kCmdSetSlotDataDefault = 1005;
constexpr uint16_t kCmdSetSlotEnable = 1006;
constexpr uint16_t kCmdSlotDataConfigSave = 1009;
constexpr uint16_t kCmdGetActiveSlot = 1018;
constexpr uint16_t kCmdHfSetAntiCollData = 4001;
constexpr uint16_t kCmdEm410xSetEmuId = 5000;

constexpr uint8_t kDeviceModeEmulator = 0x00;
constexpr uint8_t kDeviceModeReader = 0x01;
constexpr uint8_t kTagSenseLf = 0x01;
constexpr uint8_t kTagSenseHf = 0x02;
constexpr uint16_t kTagTypeEm410x = 100;
constexpr uint16_t kTagTypeMifareMini = 1000;
constexpr uint16_t kTagTypeMifare1k = 1001;
constexpr uint16_t kTagTypeMifare4k = 1003;
constexpr uint16_t kStatusSuccess = 0x68;

uint8_t lrc(const uint8_t* data, size_t length) {
    uint16_t sum = 0;
    for (size_t i = 0; i < length; ++i) sum += data[i];
    return static_cast<uint8_t>((0x100 - (sum & 0xFF)) & 0xFF);
}

// Frame: SOF | LRC1 | CMD(2) | STATUS(2) | LEN(2) | LRC2 | DATA | LRC3.
// LRC = 8-bit two's-complement of the sum of the bytes it covers.
size_t buildFrame(uint16_t cmd, const uint8_t* data, uint16_t dataLen,
                   uint8_t* out) {
    out[0] = 0x11;
    out[1] = lrc(out, 1);
    out[2] = static_cast<uint8_t>(cmd >> 8);
    out[3] = static_cast<uint8_t>(cmd & 0xFF);
    out[4] = 0;
    out[5] = 0;
    out[6] = static_cast<uint8_t>(dataLen >> 8);
    out[7] = static_cast<uint8_t>(dataLen & 0xFF);
    out[8] = lrc(&out[2], 6);
    if (dataLen > 0) memcpy(&out[9], data, dataLen);
    out[9 + dataLen] = lrc(&out[9], dataLen);
    return 10 + dataLen;
}
}  // namespace

void ChameleonUltraClient::onNotify(const uint8_t* data, size_t length) {
    // Single-slot handoff: exactly one command is ever in flight, so no
    // ring buffer is needed here (contrast with wifi_sniffer_service.cpp's
    // ring, which handles a continuous stream). The notify callback runs
    // on the NimBLE host task; sendCommand() below polls the volatile
    // flag from the caller's task, same producer/consumer discipline used
    // throughout this codebase.
    //
    // A response frame can span multiple BLE notifications if it doesn't
    // fit in one ATT MTU packet (relevant for HF14A_SCAN responses with a
    // variable-length ATS field) -- append rather than overwrite, and only
    // signal ready once the full frame (per its own LEN field) has arrived.
    for (size_t i = 0; i < length && responseLen_ < kResponseBufSize; ++i) {
        responseBuf_[responseLen_++] = data[i];
    }
    if (responseLen_ >= 8) {
        const uint16_t len = (static_cast<uint16_t>(responseBuf_[6]) << 8) |
                              responseBuf_[7];
        if (responseLen_ >= static_cast<size_t>(10 + len)) {
            responseReady_ = true;
        }
    }
}

bool ChameleonUltraClient::connect(uint32_t scanTimeoutMs) {
    disconnect();

    // Both radios share the ESP32-S3 radio resources, same discipline as
    // ble_scanner.cpp/ble_spam_service.cpp.
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);

    if (!initialized_) {
        NimBLEDevice::init("");
        initialized_ = true;
    }

    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner == nullptr) {
        lastStatus_ = "BLE scanner unavailable";
        NimBLEDevice::deinit(true);
        initialized_ = false;
        return false;
    }
    scanner->clearResults();
    scanner->setActiveScan(true);
    NimBLEScanResults results = scanner->getResults(scanTimeoutMs, false);

    const NimBLEAdvertisedDevice* target = nullptr;
    const int count = results.getCount();
    for (int i = 0; i < count; ++i) {
        const NimBLEAdvertisedDevice* device = results.getDevice(i);
        if (device != nullptr && device->haveName() &&
            device->getName() == kDeviceName) {
            target = device;
            break;
        }
    }
    if (target == nullptr) {
        scanner->clearResults();
        lastStatus_ = "Chameleon Ultra not found";
        NimBLEDevice::deinit(true);
        initialized_ = false;
        return false;
    }

    client_ = NimBLEDevice::createClient();
    // Plain connect, no bonding/pairing-key exchange -- matches the
    // upstream ESP-ChameleonUltra library's behavior, but unverified
    // against real hardware until this increment's flash test.
    // Keep the scan result alive until connect() has consumed it. Clearing
    // results first invalidates `target` and caused intermittent connections.
    if (client_ == nullptr || !client_->connect(target)) {
        scanner->clearResults();
        lastStatus_ = "Connect failed";
        client_ = nullptr;
        NimBLEDevice::deinit(true);
        initialized_ = false;
        return false;
    }
    scanner->clearResults();

    NimBLERemoteService* service = client_->getService(kServiceUuid);
    NimBLERemoteCharacteristic* notifyChar =
        service != nullptr ? service->getCharacteristic(kNotifyCharUuid)
                            : nullptr;
    writeChar_ = service != nullptr
                     ? service->getCharacteristic(kWriteCharUuid)
                     : nullptr;

    if (service == nullptr || notifyChar == nullptr || writeChar_ == nullptr) {
        lastStatus_ = "UART service not found";
        client_->disconnect();
        writeChar_ = nullptr;
        client_ = nullptr;
        NimBLEDevice::deinit(true);
        initialized_ = false;
        return false;
    }

    notifyChar->subscribe(true, [this](NimBLERemoteCharacteristic*,
                                        uint8_t* data, size_t length, bool) {
        onNotify(data, length);
    });

    connected_ = true;
    // Give the CCCD subscription and Chameleon UART task time to settle before
    // sending the first command. Without this, its response can beat notification
    // delivery setup even though the BLE link itself is healthy.
    delay(150);

    // The device can be in emulator mode (last used for tag emulation) or
    // reader mode -- HF14A_SCAN/EM410X_SCAN only work in reader mode, so
    // force it every connect rather than assuming whatever state it was
    // left in.
    uint8_t modeByte = kDeviceModeReader;
    uint8_t modeRespData[4];
    uint16_t modeRespLen = 0, modeRespStatus = 0;
    sendCommand(kCmdChangeDeviceMode, &modeByte, 1, modeRespData,
                sizeof(modeRespData), modeRespLen, modeRespStatus);

    bool stateReady = false;
    for (uint8_t attempt = 0; attempt < 3 && !stateReady; ++attempt) {
        stateReady = refreshState();
        if (!stateReady) delay(100);
    }
    lastStatus_ = stateReady
        ? "Connected - slot " + String(activeSlot_ + 1) +
              (readerMode_ ? " READER" : " EMULATOR")
        : "Connected (state readback pending)";
    return true;
}

void ChameleonUltraClient::disconnect() {
    connected_ = false;
    writeChar_ = nullptr;
    if (client_ != nullptr) {
        if (client_->isConnected()) client_->disconnect();
        client_ = nullptr;
    }
    if (initialized_) {
        // Give the NimBLE host task a moment to fully process the
        // just-issued disconnect before deinit() deletes objects out from
        // under it -- same lesson as ble_spam_service.cpp's reboot-on-stop
        // fix (NimBLEDevice::deinit() has no graceful pre-teardown hook
        // for every role the way it does for scanning).
        delay(50);
        NimBLEDevice::deinit(true);
        initialized_ = false;
    }
}

bool ChameleonUltraClient::sendCommand(uint16_t cmd, const uint8_t* data,
                                       uint16_t dataLen, uint8_t* respData,
                                       size_t respDataCapacity,
                                       uint16_t& respLen, uint16_t& respStatus,
                                       uint32_t timeoutMs) {
    if (!connected_ || writeChar_ == nullptr) return false;

    uint8_t frame[64];
    if (dataLen + 10 > sizeof(frame)) return false;
    const size_t frameLen = buildFrame(cmd, data, dataLen, frame);

    responseLen_ = 0;
    responseReady_ = false;
    if (!writeChar_->writeValue(frame, frameLen, true)) return false;

    const unsigned long start = millis();
    while (!responseReady_ && millis() - start < timeoutMs) {
        delay(10);
    }
    if (!responseReady_ || responseLen_ < 10) return false;

    const uint8_t* resp = responseBuf_;
    respStatus = (static_cast<uint16_t>(resp[4]) << 8) | resp[5];
    const uint16_t len = (static_cast<uint16_t>(resp[6]) << 8) | resp[7];
    if (responseLen_ < static_cast<size_t>(10 + len)) return false;
    // Guard against a response larger than the caller's buffer -- relevant
    // now that HF14A_SCAN's response length is genuinely variable (ATS),
    // unlike the fixed-size responses this was originally written for.
    if (len > 0 && (respData == nullptr || len > respDataCapacity)) return false;

    respLen = len;
    if (len > 0) memcpy(respData, &resp[9], len);
    return true;
}

bool ChameleonUltraClient::simpleCommand(uint16_t cmd, const uint8_t* data,
                                         uint16_t dataLen) {
    uint8_t response[8];
    uint16_t responseLen = 0;
    uint16_t responseStatus = 0;
    if (!sendCommand(cmd, data, dataLen, response, sizeof(response),
                     responseLen, responseStatus) ||
        responseStatus != kStatusSuccess) {
        lastStatus_ = "Command " + String(cmd) + " failed (" +
                      String(responseStatus) + ")";
        return false;
    }
    return true;
}

bool ChameleonUltraClient::getAppVersion(uint8_t& major, uint8_t& minor) {
    uint8_t respData[8];
    uint16_t respLen = 0, respStatus = 0;
    if (!sendCommand(kCmdGetAppVersion, nullptr, 0, respData, sizeof(respData),
                      respLen, respStatus) ||
        respLen < 2) {
        lastStatus_ = "GET_APP_VERSION failed";
        return false;
    }
    major = respData[0];
    minor = respData[1];
    return true;
}

bool ChameleonUltraClient::getBatteryInfo(uint16_t& millivolts,
                                          uint8_t& percentage) {
    uint8_t respData[8];
    uint16_t respLen = 0, respStatus = 0;
    if (!sendCommand(kCmdGetBatteryInfo, nullptr, 0, respData,
                      sizeof(respData), respLen, respStatus) ||
        respLen < 3) {
        lastStatus_ = "GET_BATTERY_INFO failed";
        return false;
    }
    millivolts = (static_cast<uint16_t>(respData[0]) << 8) | respData[1];
    percentage = respData[2];
    return true;
}

bool ChameleonUltraClient::scanHf14a(HfTag& tag) {
    uint8_t respData[64];
    uint16_t respLen = 0, respStatus = 0;
    if (!sendCommand(kCmdHf14aScan, nullptr, 0, respData, sizeof(respData),
                      respLen, respStatus) ||
        respLen == 0) {
        return false;
    }
    const uint8_t uidLen = respData[0];
    if (uidLen == 0 || uidLen > sizeof(tag.uid) ||
        respLen < static_cast<uint16_t>(1 + uidLen + 4)) {
        return false;
    }
    tag.uidLen = uidLen;
    memcpy(tag.uid, &respData[1], uidLen);
    const size_t offset = 1 + uidLen;
    tag.atqa = (static_cast<uint16_t>(respData[offset]) << 8) |
               respData[offset + 1];
    tag.sak = respData[offset + 2];
    return true;
}

bool ChameleonUltraClient::scanEm410x(uint8_t id[5]) {
    uint8_t respData[8];
    uint16_t respLen = 0, respStatus = 0;
    if (!sendCommand(kCmdEm410xScan, nullptr, 0, respData, sizeof(respData),
                      respLen, respStatus) ||
        respLen < 5) {
        return false;
    }
    memcpy(id, respData, 5);
    return true;
}

bool ChameleonUltraClient::setReaderMode() {
    const uint8_t mode = kDeviceModeReader;
    if (!simpleCommand(kCmdChangeDeviceMode, &mode, 1)) return false;
    if (!refreshState() || !readerMode_) {
        lastStatus_ = "Reader mode verification failed";
        return false;
    }
    lastStatus_ = "Reader mode";
    return true;
}

bool ChameleonUltraClient::refreshState() {
    uint8_t data[4];
    uint16_t length = 0, status = 0;
    if (!sendCommand(kCmdGetActiveSlot, nullptr, 0, data, sizeof(data),
                     length, status) || status != kStatusSuccess ||
        length != 1) {
        lastStatus_ = "Active slot readback failed";
        return false;
    }
    activeSlot_ = data[0];
    if (!sendCommand(kCmdGetDeviceMode, nullptr, 0, data, sizeof(data),
                     length, status) || status != kStatusSuccess ||
        length != 1) {
        lastStatus_ = "Device mode readback failed";
        return false;
    }
    // The protocol returns 1 for reader and 0 for emulator mode.
    readerMode_ = data[0] != 0;
    return true;
}

bool ChameleonUltraClient::stageEm410xIdentity(const uint8_t id[5],
                                                uint8_t slot) {
    if (slot > 7) return false;
    const uint8_t slotData[] = {slot};
    const uint8_t typeData[] = {
        slot, static_cast<uint8_t>(kTagTypeEm410x >> 8),
        static_cast<uint8_t>(kTagTypeEm410x & 0xFF)};
    const uint8_t enableData[] = {slot, kTagSenseLf, 1};
    const uint8_t emulatorMode = kDeviceModeEmulator;
    if (!simpleCommand(kCmdSetSlotTagType, typeData, sizeof(typeData)) ||
        !simpleCommand(kCmdSetSlotDataDefault, typeData, sizeof(typeData)) ||
        !simpleCommand(kCmdSetActiveSlot, slotData, sizeof(slotData)) ||
        !simpleCommand(kCmdEm410xSetEmuId, id, 5) ||
        !simpleCommand(kCmdSetSlotEnable, enableData, sizeof(enableData)) ||
        !simpleCommand(kCmdSlotDataConfigSave, nullptr, 0) ||
        !simpleCommand(kCmdChangeDeviceMode, &emulatorMode, 1)) {
        return false;
    }
    if (!refreshState() || activeSlot_ != slot || readerMode_) {
        lastStatus_ = "Slot/mode verification failed";
        return false;
    }
    lastStatus_ = "Slot " + String(slot + 1) + " EM410x EMULATING";
    return true;
}

bool ChameleonUltraClient::stageHfIdentity(const HfTag& tag, uint8_t slot) {
    if (slot > 7 || tag.uidLen == 0 || tag.uidLen > sizeof(tag.uid)) {
        return false;
    }
    uint16_t tagType = 0;
    if (tag.sak == 0x09) tagType = kTagTypeMifareMini;
    else if (tag.sak == 0x08) tagType = kTagTypeMifare1k;
    else if (tag.sak == 0x18) tagType = kTagTypeMifare4k;
    else {
        lastStatus_ = "Unsupported HF identity (SAK)";
        return false;
    }
    const uint8_t slotData[] = {slot};
    const uint8_t typeData[] = {
        slot, static_cast<uint8_t>(tagType >> 8),
        static_cast<uint8_t>(tagType & 0xFF)};
    const uint8_t enableData[] = {slot, kTagSenseHf, 1};
    uint8_t antiCollision[16];
    size_t length = 0;
    antiCollision[length++] = tag.uidLen;
    memcpy(&antiCollision[length], tag.uid, tag.uidLen);
    length += tag.uidLen;
    antiCollision[length++] = static_cast<uint8_t>(tag.atqa >> 8);
    antiCollision[length++] = static_cast<uint8_t>(tag.atqa & 0xFF);
    antiCollision[length++] = tag.sak;
    antiCollision[length++] = 0;  // No ATS was returned by the current scan.
    const uint8_t emulatorMode = kDeviceModeEmulator;
    if (!simpleCommand(kCmdSetSlotTagType, typeData, sizeof(typeData)) ||
        !simpleCommand(kCmdSetSlotDataDefault, typeData, sizeof(typeData)) ||
        !simpleCommand(kCmdSetActiveSlot, slotData, sizeof(slotData)) ||
        !simpleCommand(kCmdHfSetAntiCollData, antiCollision, length) ||
        !simpleCommand(kCmdSetSlotEnable, enableData, sizeof(enableData)) ||
        !simpleCommand(kCmdSlotDataConfigSave, nullptr, 0) ||
        !simpleCommand(kCmdChangeDeviceMode, &emulatorMode, 1)) {
        return false;
    }
    if (!refreshState() || activeSlot_ != slot || readerMode_) {
        lastStatus_ = "Slot/mode verification failed";
        return false;
    }
    lastStatus_ = "Slot " + String(slot + 1) + " HF EMULATING";
    return true;
}
