#include "ble_scanner.h"

#include <NimBLEAdvertisedDevice.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <WiFi.h>
#include <algorithm>

namespace {
// NimBLE 2.x wants a scan-callbacks object registered; an empty override is
// sufficient for a plain synchronous scan (no per-device or scan-end
// notifications needed), matching a known-working configuration rather than
// relying on it being optional.
class BleScanCallbacks : public NimBLEScanCallbacks {
public:
    void setOwner(BleScanner* owner) { owner_ = owner; }
    void onResult(const NimBLEAdvertisedDevice* advertised) override {
        if (owner_ != nullptr) owner_->onAdvertisement(advertised);
    }
private:
    BleScanner* owner_ = nullptr;
};
BleScanCallbacks bleScanCallbacks;

String bytesToHex(const std::string& bytes, size_t limit = 24) {
    String result;
    const size_t count = std::min(bytes.size(), limit);
    result.reserve(count * 2 + (bytes.size() > limit ? 3 : 0));
    static constexpr char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < count; ++i) {
        const uint8_t value = static_cast<uint8_t>(bytes[i]);
        result += hex[value >> 4];
        result += hex[value & 0x0F];
    }
    if (bytes.size() > limit) result += "...";
    return result;
}

String bytesToHex(const std::vector<uint8_t>& bytes, size_t limit = 24) {
    String result;
    const size_t count = std::min(bytes.size(), limit);
    result.reserve(count * 2 + (bytes.size() > limit ? 3 : 0));
    static constexpr char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < count; ++i) {
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 0x0F];
    }
    if (bytes.size() > limit) result += "...";
    return result;
}

const char* companyName(uint16_t companyId) {
    switch (companyId) {
        case 0x004C: return "Apple";
        case 0x0006: return "Microsoft";
        case 0x0075: return "Samsung";
        case 0x00E0: return "Google";
        default: return "Unknown";
    }
}
}  // namespace

void BleScanner::onAdvertisement(
    const NimBLEAdvertisedDevice* advertised) {
    if (!continuous_ || advertised == nullptr || queue_ == nullptr) return;
    RawAdvertisement raw{};
    snprintf(raw.address, sizeof(raw.address), "%s",
             advertised->getAddress().toString().c_str());
    snprintf(raw.name, sizeof(raw.name), "%s",
             advertised->haveName() ? advertised->getName().c_str()
                                    : "<unnamed>");
    String services;
    raw.serviceCount = advertised->getServiceUUIDCount();
    for (uint8_t index = 0; index < raw.serviceCount && index < 3; ++index) {
        if (!services.isEmpty()) services += " ";
        services += advertised->getServiceUUID(index).toString().c_str();
    }
    snprintf(raw.service, sizeof(raw.service), "%s", services.c_str());
    const std::string manufacturerData =
        advertised->haveManufacturerData()
            ? advertised->getManufacturerData()
            : std::string{};
    const String manufacturerHex = bytesToHex(manufacturerData);
    snprintf(raw.manufacturerData, sizeof(raw.manufacturerData), "%s",
             manufacturerHex.c_str());
    const String payloadHex = bytesToHex(advertised->getPayload(), 48);
    snprintf(raw.payloadData, sizeof(raw.payloadData), "%s",
             payloadHex.c_str());
    String manufacturer = "Not advertised";
    if (manufacturerData.size() >= 2) {
        const uint16_t companyId =
            static_cast<uint8_t>(manufacturerData[0]) |
            (static_cast<uint16_t>(
                 static_cast<uint8_t>(manufacturerData[1])) << 8);
        char text[28];
        snprintf(text, sizeof(text), "%s 0x%04X", companyName(companyId),
                 companyId);
        manufacturer = text;
    }
    snprintf(raw.manufacturer, sizeof(raw.manufacturer), "%s",
             manufacturer.c_str());
    raw.rssi = advertised->getRSSI();
    raw.addressType = advertised->getAddressType();
    raw.advertisementType = advertised->getAdvType();
    raw.connectable = raw.advertisementType == 0 ||
                      raw.advertisementType == 1;
    raw.payloadLength = advertised->getPayload().size();
    ++advertisementCount_;
    if (xQueueSend(queue_, &raw, 0) != pdTRUE) ++droppedCount_;
}

bool BleScanner::beginContinuous(String& status) {
    stop();
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);
    BLEDevice::init("");
    initialized_ = true;
    if (queue_ == nullptr) {
        queue_ = xQueueCreateStatic(kQueueCapacity, sizeof(RawAdvertisement),
                                    queueStorage_, &queueControl_);
    }
    xQueueReset(queue_);
    advertisementCount_ = 0;
    droppedCount_ = 0;
    BLEScan* scanner = BLEDevice::getScan();
    if (scanner == nullptr) {
        status = "BLE scanner unavailable";
        stop();
        return false;
    }
    bleScanCallbacks.setOwner(this);
    scanner->clearResults();
    scanner->setMaxResults(0);
    scanner->setScanCallbacks(&bleScanCallbacks, true);
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(80);
    continuous_ = true;
    if (!scanner->start(0, false, true)) {
        status = "Unable to start continuous scan";
        stop();
        return false;
    }
    status = "Continuous capture active";
    return true;
}

bool BleScanner::nextResult(BleDeviceInfo& device) {
    RawAdvertisement raw{};
    if (queue_ == nullptr || xQueueReceive(queue_, &raw, 0) != pdTRUE) {
        return false;
    }
    device.address = raw.address;
    device.name = raw.name;
    device.service = raw.service;
    device.manufacturerData = raw.manufacturerData;
    device.payloadData = raw.payloadData;
    device.manufacturer = raw.manufacturer;
    device.rssi = raw.rssi;
    device.connectable = raw.connectable;
    device.addressType = raw.addressType;
    device.advertisementType = raw.advertisementType;
    device.serviceCount = raw.serviceCount;
    device.payloadLength = raw.payloadLength;
    return true;
}

bool BleScanner::scan(std::vector<BleDeviceInfo>& devices, String& status,
                      uint32_t durationSeconds) {
    devices.clear();
    status = "BLE scan failed";

    // Both radios share the ESP32-S3 radio resources. Stop Wi-Fi before BLE.
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);

    if (!initialized_) {
        BLEDevice::init("");
        initialized_ = true;
    }

    BLEScan* scanner = BLEDevice::getScan();
    if (scanner == nullptr) {
        status = "BLE scanner unavailable";
        // Already initialized above; release it before bailing so a
        // subsequent Wi-Fi scan doesn't contend with a half-used BLE stack.
        BLEDevice::deinit(true);
        initialized_ = false;
        return false;
    }

    scanner->clearResults();
    scanner->setScanCallbacks(&bleScanCallbacks);
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(80);

    BLEScanResults results = scanner->getResults(durationSeconds * 1000, false);
    const int count = results.getCount();
    for (int index = 0; index < count; ++index) {
        const NimBLEAdvertisedDevice* advertised = results.getDevice(index);
        if (advertised == nullptr) continue;
        BleDeviceInfo device;
        device.address = advertised->getAddress().toString().c_str();
        device.name = advertised->haveName() ? advertised->getName().c_str()
                                             : "<unnamed>";
        device.serviceCount = advertised->getServiceUUIDCount();
        device.service = "";
        for (uint8_t service = 0;
             service < device.serviceCount && service < 3; ++service) {
            if (!device.service.isEmpty()) device.service += " ";
            device.service += advertised->getServiceUUID(service)
                                  .toString().c_str();
        }
        const std::string manufacturerData =
            advertised->haveManufacturerData()
                ? advertised->getManufacturerData()
                : std::string{};
        device.manufacturerData = bytesToHex(manufacturerData);
        device.payloadData = bytesToHex(advertised->getPayload(), 48);
        if (manufacturerData.size() >= 2) {
            const uint16_t companyId =
                static_cast<uint8_t>(manufacturerData[0]) |
                (static_cast<uint16_t>(
                     static_cast<uint8_t>(manufacturerData[1])) << 8);
            device.manufacturer = String(companyName(companyId)) + " 0x";
            char id[5];
            snprintf(id, sizeof(id), "%04X", companyId);
            device.manufacturer += id;
        } else {
            device.manufacturer = "Not advertised";
        }
        device.rssi = advertised->getRSSI();
        device.addressType = advertised->getAddressType();
        device.advertisementType = advertised->getAdvType();
        device.connectable = device.advertisementType == 0 ||
                             device.advertisementType == 1;
        device.payloadLength = static_cast<uint16_t>(
            advertised->getPayload().size());
        devices.push_back(device);
    }

    std::sort(devices.begin(), devices.end(),
              [](const BleDeviceInfo& left, const BleDeviceInfo& right) {
                  return left.rssi > right.rssi;
              });
    scanner->clearResults();
    status = devices.empty() ? "No advertisements found"
                             : String(devices.size()) + " devices";
    Serial.printf("[ble] scan result=%d\n", count);

    // Fully release NimBLE's controller/memory before returning. Unlike the
    // classic Bluedroid BLE stack this replaced, leaving NimBLE initialized
    // while a Wi-Fi scan starts crashes (observed on hardware: a genuine
    // panic/reboot, not a hang) -- this makes the radio handoff symmetric
    // with how entering a BLE scan already turns Wi-Fi off first.
    BLEDevice::deinit(true);
    initialized_ = false;
    return true;
}

void BleScanner::stop() {
    if (!initialized_) return;
    BLEScan* scanner = BLEDevice::getScan();
    if (scanner != nullptr) {
        scanner->stop();
        scanner->clearResults();
    }
    BLEDevice::deinit(true);
    initialized_ = false;
    continuous_ = false;
    bleScanCallbacks.setOwner(nullptr);
}
