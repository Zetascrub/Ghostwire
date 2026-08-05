#include "ble_keyboard_service.h"

#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <WiFi.h>
#include <vector>

namespace {
constexpr uint8_t kReportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x06,
    0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07,
    0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
};

bool asciiToHid(char value, uint8_t& usage, uint8_t& modifiers) {
    modifiers = 0;
    if (value >= 'a' && value <= 'z') {
        usage = 0x04 + value - 'a';
        return true;
    }
    if (value >= 'A' && value <= 'Z') {
        usage = 0x04 + value - 'A';
        modifiers = 0x02;
        return true;
    }
    if (value >= '1' && value <= '9') {
        usage = 0x1E + value - '1';
        return true;
    }
    if (value == '0') { usage = 0x27; return true; }
    switch (value) {
        case '\n': case '\r': usage = 0x28; return true;
        case '\b': case 0x7F: usage = 0x2A; return true;
        case '\t': usage = 0x2B; return true;
        case ' ': usage = 0x2C; return true;
        case '-': usage = 0x2D; return true;
        case '_': usage = 0x2D; modifiers = 0x02; return true;
        case '=': usage = 0x2E; return true;
        case '+': usage = 0x2E; modifiers = 0x02; return true;
        case '[': usage = 0x2F; return true;
        case '{': usage = 0x2F; modifiers = 0x02; return true;
        case ']': usage = 0x30; return true;
        case '}': usage = 0x30; modifiers = 0x02; return true;
        case '\\': usage = 0x31; return true;
        case '|': usage = 0x31; modifiers = 0x02; return true;
        case ';': usage = 0x33; return true;
        case ':': usage = 0x33; modifiers = 0x02; return true;
        case '\'': usage = 0x34; return true;
        case '"': usage = 0x34; modifiers = 0x02; return true;
        case '`': usage = 0x35; return true;
        case '~': usage = 0x35; modifiers = 0x02; return true;
        case ',': usage = 0x36; return true;
        case '<': usage = 0x36; modifiers = 0x02; return true;
        case '.': usage = 0x37; return true;
        case '>': usage = 0x37; modifiers = 0x02; return true;
        case '/': usage = 0x38; return true;
        case '?': usage = 0x38; modifiers = 0x02; return true;
        case '!': usage = 0x1E; modifiers = 0x02; return true;
        case '@': usage = 0x1F; modifiers = 0x02; return true;
        case '#': usage = 0x20; modifiers = 0x02; return true;
        case '$': usage = 0x21; modifiers = 0x02; return true;
        case '%': usage = 0x22; modifiers = 0x02; return true;
        case '^': usage = 0x23; modifiers = 0x02; return true;
        case '&': usage = 0x24; modifiers = 0x02; return true;
        case '*': usage = 0x25; modifiers = 0x02; return true;
        case '(': usage = 0x26; modifiers = 0x02; return true;
        case ')': usage = 0x27; modifiers = 0x02; return true;
        default: return false;
    }
}
}  // namespace

bool BleKeyboardService::begin(uint8_t batteryPercent) {
    if (active_) return true;
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);

    NimBLEDevice::init("Ghostwire Keyboard");
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    server_ = NimBLEDevice::createServer();
    server_->advertiseOnDisconnect(true);
    hid_ = new NimBLEHIDDevice(server_);
    input_ = hid_->getInputReport(1);
    hid_->setManufacturer("Zetascrub");
    hid_->setPnp(0x02, 0x303A, 0x1001, 0x0100);
    hid_->setHidInfo(0x00, 0x01);
    hid_->setReportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));
    hid_->setBatteryLevel(batteryPercent);
    if (!server_->start()) {
        status_ = "BLE HID service failed";
        end();
        return false;
    }
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->setName("Ghostwire Keyboard");
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid_->getHidService()->getUUID());
    advertising->enableScanResponse(true);
    if (!advertising->start()) {
        status_ = "Advertising failed";
        end();
        return false;
    }
    charactersSent_ = 0;
    active_ = true;
    status_ = "Advertising - pair from host";
    return true;
}

void BleKeyboardService::end() {
    if (!active_ && server_ == nullptr && hid_ == nullptr) return;
    active_ = false;
    if (NimBLEDevice::isInitialized()) {
        // A connected HID peripheral cannot be torn down like a passive scan.
        // Disable advertise-on-disconnect first, explicitly terminate every
        // peer, and give the NimBLE host task a bounded window to process the
        // disconnect event before deinit(true) deletes the server and its
        // characteristics. Deinitializing immediately here used to race the
        // host task and panic when Esc stopped an established keyboard link.
        if (server_ != nullptr) {
            server_->advertiseOnDisconnect(false);
            const std::vector<uint16_t> peers = server_->getPeerDevices();
            for (const uint16_t handle : peers) server_->disconnect(handle);
            const unsigned long deadline = millis() + 500;
            while (server_->getConnectedCount() > 0 &&
                   static_cast<long>(deadline - millis()) > 0) {
                delay(10);
            }
        }
        NimBLEDevice::stopAdvertising();
        delay(50);
        delete hid_;
        hid_ = nullptr;
        NimBLEDevice::deinit(true);
    }
    server_ = nullptr;
    input_ = nullptr;
    // If begin() failed before NimBLE initialization completed, the HID
    // wrapper can still exist even though the stack does not.
    delete hid_;
    hid_ = nullptr;
    status_ = "Stopped";
}

bool BleKeyboardService::isConnected() const {
    return active_ && server_ != nullptr && server_->getConnectedCount() > 0;
}

bool BleKeyboardService::sendKey(uint8_t usage, uint8_t modifiers) {
    if (!isConnected() || input_ == nullptr) return false;
    uint8_t report[8] = {modifiers, 0, usage, 0, 0, 0, 0, 0};
    if (!input_->notify(report, sizeof(report))) return false;
    delay(8);
    memset(report, 0, sizeof(report));
    input_->notify(report, sizeof(report));
    ++charactersSent_;
    return true;
}

bool BleKeyboardService::sendAscii(char value) {
    uint8_t usage = 0;
    uint8_t modifiers = 0;
    return asciiToHid(value, usage, modifiers) && sendKey(usage, modifiers);
}
