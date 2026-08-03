#pragma once

#include <Arduino.h>

class NimBLEHIDDevice;
class NimBLEServer;
class NimBLECharacteristic;

class BleKeyboardService {
public:
    bool begin(uint8_t batteryPercent);
    void end();
    bool isActive() const { return active_; }
    bool isConnected() const;
    bool sendAscii(char value);
    uint32_t charactersSent() const { return charactersSent_; }
    const String& status() const { return status_; }

private:
    bool sendKey(uint8_t usage, uint8_t modifiers = 0);

    NimBLEServer* server_ = nullptr;
    NimBLEHIDDevice* hid_ = nullptr;
    NimBLECharacteristic* input_ = nullptr;
    bool active_ = false;
    uint32_t charactersSent_ = 0;
    String status_ = "Stopped";
};
