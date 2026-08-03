#include "biscuit_pro_client.h"

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <cstring>

namespace {
constexpr char kDeviceName[] = "Biscuit Pro";
const NimBLEUUID kServiceUuid("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
const NimBLEUUID kCommandUuid("beb5483e-36e1-4688-b7f5-ea07361b26a8");
const NimBLEUUID kResponseUuid("beb5483e-36e1-4688-b7f5-ea07361b26a9");
const NimBLEUUID kStatusUuid("beb5483e-36e1-4688-b7f5-ea07361b26aa");
const NimBLEUUID kDeviceInfoUuid(static_cast<uint16_t>(0x180A));
const NimBLEUUID kManufacturerUuid(static_cast<uint16_t>(0x2A29));
const NimBLEUUID kModelUuid(static_cast<uint16_t>(0x2A24));
const NimBLEUUID kFirmwareUuid(static_cast<uint16_t>(0x2A26));
const NimBLEUUID kC5FirmwareUuid(static_cast<uint16_t>(0x2A28));
}  // namespace

void BiscuitProClient::onNotify(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length && responseLen_ < kResponseCapacity - 1;
         ++i) {
        response_[responseLen_++] = static_cast<char>(data[i]);
    }
    response_[responseLen_] = '\0';
    lastNotifyMs_ = millis();
}

String BiscuitProClient::readText(
    NimBLERemoteCharacteristic* characteristic) {
    if (characteristic == nullptr || !characteristic->canRead()) return "";
    const NimBLEAttValue value = characteristic->readValue();
    if (value.size() == 0) return "";
    String result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const char c = static_cast<char>(value[i]);
        if (c != '\0' && static_cast<uint8_t>(c) >= 0x20) result += c;
    }
    result.trim();
    return result;
}

bool BiscuitProClient::connect(uint32_t scanTimeoutMs) {
    disconnect();
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);

    NimBLEDevice::init("");
    NimBLEDevice::setMTU(512);
    initialized_ = true;
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner == nullptr) {
        lastStatus_ = "BLE scanner unavailable";
        disconnect();
        return false;
    }
    scanner->clearResults();
    scanner->setActiveScan(true);
    NimBLEScanResults results = scanner->getResults(scanTimeoutMs, false);
    const NimBLEAdvertisedDevice* target = nullptr;
    for (int i = 0; i < results.getCount(); ++i) {
        const NimBLEAdvertisedDevice* device = results.getDevice(i);
        if (device == nullptr) continue;
        if ((device->haveName() && device->getName() == kDeviceName) ||
            device->isAdvertisingService(kServiceUuid)) {
            target = device;
            break;
        }
    }
    if (target == nullptr) {
        scanner->clearResults();
        lastStatus_ = "Biscuit Pro not found";
        disconnect();
        return false;
    }

    client_ = NimBLEDevice::createClient();
    if (client_ == nullptr || !client_->connect(target)) {
        scanner->clearResults();
        lastStatus_ = "Connection failed";
        disconnect();
        return false;
    }
    scanner->clearResults();
    NimBLERemoteService* service = client_->getService(kServiceUuid);
    commandChar_ = service != nullptr
                       ? service->getCharacteristic(kCommandUuid)
                       : nullptr;
    NimBLERemoteCharacteristic* responseChar =
        service != nullptr ? service->getCharacteristic(kResponseUuid)
                           : nullptr;
    NimBLERemoteCharacteristic* statusChar =
        service != nullptr ? service->getCharacteristic(kStatusUuid) : nullptr;
    if (commandChar_ == nullptr || responseChar == nullptr ||
        !responseChar->subscribe(true,
            [this](NimBLERemoteCharacteristic*, uint8_t* data,
                   size_t length, bool) { onNotify(data, length); })) {
        lastStatus_ = "Biscuit GATT service incomplete";
        disconnect();
        return false;
    }

    deviceStatus_ = readText(statusChar);
    NimBLERemoteService* info = client_->getService(kDeviceInfoUuid);
    if (info != nullptr) {
        manufacturer_ = readText(info->getCharacteristic(kManufacturerUuid));
        model_ = readText(info->getCharacteristic(kModelUuid));
        firmware_ = readText(info->getCharacteristic(kFirmwareUuid));
        c5Firmware_ = readText(info->getCharacteristic(kC5FirmwareUuid));
    }
    lastStatus_ = "Connected";
    return true;
}

bool BiscuitProClient::isConnected() const {
    return client_ != nullptr && client_->isConnected();
}

bool BiscuitProClient::sendReadOnlyCommand(const String& command,
                                           String& response,
                                           uint32_t timeoutMs) {
    response = "";
    if (!isConnected() || commandChar_ == nullptr) {
        lastStatus_ = "Not connected";
        return false;
    }
    responseLen_ = 0;
    lastNotifyMs_ = 0;
    response_[0] = '\0';
    if (!commandChar_->writeValue(
            reinterpret_cast<const uint8_t*>(command.c_str()),
            command.length(), true)) {
        lastStatus_ = "Command write failed";
        return false;
    }

    const uint32_t started = millis();
    while (millis() - started < timeoutMs) {
        if (responseLen_ > 0 && millis() - lastNotifyMs_ >= 250) break;
        if (!isConnected()) {
            lastStatus_ = "Biscuit disconnected";
            return false;
        }
        delay(10);
    }
    if (responseLen_ == 0) {
        lastStatus_ = "Response timed out";
        return false;
    }
    response = String(response_);
    response.trim();
    lastStatus_ = "Response received";
    return true;
}

bool BiscuitProClient::sendCommandNoWait(const String& command) {
    if (!isConnected() || commandChar_ == nullptr) {
        lastStatus_ = "Not connected";
        return false;
    }
    responseLen_ = 0;
    lastNotifyMs_ = 0;
    response_[0] = '\0';
    const bool written = commandChar_->writeValue(
        reinterpret_cast<const uint8_t*>(command.c_str()), command.length(),
        true);
    lastStatus_ = written ? "Command sent" : "Command write failed";
    return written;
}

String BiscuitProClient::takeNotifications() {
    if (responseLen_ == 0) return "";
    const size_t length = responseLen_;
    response_[length] = '\0';
    String result(response_);
    responseLen_ = 0;
    response_[0] = '\0';
    return result;
}

void BiscuitProClient::disconnect() {
    commandChar_ = nullptr;
    if (client_ != nullptr) {
        if (client_->isConnected()) client_->disconnect();
        client_ = nullptr;
    }
    if (initialized_) {
        delay(50);
        NimBLEDevice::deinit(true);
        initialized_ = false;
    }
}
