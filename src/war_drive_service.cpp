#include "war_drive_service.h"

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <cstring>

namespace {
// NimBLEScan::start()'s duration parameter is milliseconds, not seconds --
// confirmed by reading ble_scanner.cpp's blocking getResults() call, which
// multiplies its seconds parameter by 1000 before passing it down.
constexpr uint32_t kBleScanDurationMs = 5000;

// NimBLE 2.x wants a scan-callbacks object registered even for a plain
// scan -- same empty-override requirement already noted in
// ble_scanner.cpp.
class WarDriveScanCallbacks : public NimBLEScanCallbacks {};
WarDriveScanCallbacks warDriveScanCallbacks;
}  // namespace

void WarDriveService::start() {
    active_ = true;
    wifiUniqueCount_ = 0;
    bleUniqueCount_ = 0;
    pendingWifi_.clear();
    pendingBle_.clear();
    beginWifiPhase();
}

void WarDriveService::stop() {
    if (phase_ == Phase::WifiScanning) {
        WiFi.scanDelete();
        WiFi.mode(WIFI_OFF);
    } else if (phase_ == Phase::BleScanning) {
        NimBLEScan* scanner = NimBLEDevice::getScan();
        if (scanner != nullptr) scanner->stop();
        // Settle before deinit, same lesson as ble_spam_service.cpp's
        // reboot-on-stop fix.
        delay(50);
        if (bleInitialized_) {
            NimBLEDevice::deinit(true);
            bleInitialized_ = false;
        }
    }
    phase_ = Phase::Idle;
    active_ = false;
}

void WarDriveService::update() {
    if (!active_) return;

    if (phase_ == Phase::WifiScanning) {
        const int16_t result = WiFi.scanComplete();
        if (result >= 0) {
            finishWifiPhase(result);
        } else if (result == WIFI_SCAN_FAILED) {
            finishWifiPhase(0);
        }
        // WIFI_SCAN_RUNNING: keep waiting, no blocking.
    } else if (phase_ == Phase::BleScanning) {
        NimBLEScan* scanner = NimBLEDevice::getScan();
        if (scanner == nullptr || !scanner->isScanning()) {
            finishBlePhase();
        }
    }
}

const char* WarDriveService::currentPhaseName() const {
    switch (phase_) {
        case Phase::WifiScanning: return "Wi-Fi";
        case Phase::BleScanning: return "BLE";
        default: return "Idle";
    }
}

bool WarDriveService::nextWifiResult(WarDriveWifiResult& result) {
    if (pendingWifi_.empty()) return false;
    result = pendingWifi_.front();
    pendingWifi_.erase(pendingWifi_.begin());
    return true;
}

bool WarDriveService::nextBleResult(WarDriveBleResult& result) {
    if (pendingBle_.empty()) return false;
    result = pendingBle_.front();
    pendingBle_.erase(pendingBle_.begin());
    return true;
}

void WarDriveService::noteUniqueWifi(const uint8_t bssid[6]) {
    for (size_t i = 0; i < wifiUniqueCount_; ++i) {
        if (memcmp(wifiBssids_[i], bssid, 6) == 0) return;
    }
    if (wifiUniqueCount_ < kMaxUnique) {
        memcpy(wifiBssids_[wifiUniqueCount_], bssid, 6);
        ++wifiUniqueCount_;
    }
}

void WarDriveService::noteUniqueBle(const String& address) {
    for (size_t i = 0; i < bleUniqueCount_; ++i) {
        if (bleAddresses_[i] == address) return;
    }
    if (bleUniqueCount_ < kMaxUnique) {
        bleAddresses_[bleUniqueCount_] = address;
        ++bleUniqueCount_;
    }
}

void WarDriveService::beginWifiPhase() {
    if (bleInitialized_) {
        delay(50);
        NimBLEDevice::deinit(true);
        bleInitialized_ = false;
    }
    phase_ = Phase::WifiScanning;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(150);
    // WiFiScanClass::scanComplete() self-aborts as WIFI_SCAN_FAILED after
    // max_ms_per_chan*20 (6s at the default 300ms/channel) -- shorter than
    // the blocking scanWifiNetworks() path's own 10s wait for the same
    // scan, so a full-channel scan that legitimately takes 6-10s reported
    // FAILED here even though it would have succeeded synchronously.
    // Passing 500ms/channel raises that internal timeout to 10s, matching
    // scanWifiNetworks()'s wait.
    WiFi.scanNetworks(true, true, false, 500);
}

void WarDriveService::finishWifiPhase(int16_t count) {
    for (int16_t i = 0; i < count; ++i) {
        WarDriveWifiResult result;
        result.ssid = WiFi.SSID(i);
        const uint8_t* bssid = WiFi.BSSID(i);
        if (bssid != nullptr) {
            memcpy(result.bssid, bssid, sizeof(result.bssid));
        } else {
            memset(result.bssid, 0, sizeof(result.bssid));
        }
        result.channel = WiFi.channel(i);
        result.rssi = WiFi.RSSI(i);
        result.authmode = WiFi.encryptionType(i);
        noteUniqueWifi(result.bssid);
        pendingWifi_.push_back(result);
    }
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    beginBlePhase();
}

void WarDriveService::beginBlePhase() {
    phase_ = Phase::BleScanning;
    if (!bleInitialized_) {
        NimBLEDevice::init("");
        bleInitialized_ = true;
    }
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner == nullptr) {
        if (bleInitialized_) {
            NimBLEDevice::deinit(true);
            bleInitialized_ = false;
        }
        beginWifiPhase();
        return;
    }
    scanner->clearResults();
    scanner->setScanCallbacks(&warDriveScanCallbacks);
    scanner->setActiveScan(true);
    scanner->start(kBleScanDurationMs, false, true);
}

void WarDriveService::finishBlePhase() {
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner != nullptr) {
        NimBLEScanResults results = scanner->getResults();
        const int count = results.getCount();
        for (int i = 0; i < count; ++i) {
            const NimBLEAdvertisedDevice* device = results.getDevice(i);
            if (device == nullptr) continue;
            WarDriveBleResult result;
            result.address = device->getAddress().toString().c_str();
            result.name = device->haveName() ? device->getName().c_str()
                                             : "<unnamed>";
            result.service = device->haveServiceUUID()
                                  ? device->getServiceUUID().toString().c_str()
                                  : "";
            result.rssi = device->getRSSI();
            result.connectable = false;
            noteUniqueBle(result.address);
            pendingBle_.push_back(result);
        }
        scanner->clearResults();
    }
    // Settle before deinit, same lesson as ble_spam_service.cpp's
    // reboot-on-stop fix.
    delay(50);
    if (bleInitialized_) {
        NimBLEDevice::deinit(true);
        bleInitialized_ = false;
    }
    beginWifiPhase();
}
