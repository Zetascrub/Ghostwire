#pragma once

#include <Arduino.h>

class NimBLEClient;
class NimBLERemoteCharacteristic;

class BiscuitProClient {
public:
    bool connect(uint32_t scanTimeoutMs = 5000);
    void disconnect();
    bool isConnected() const;
    const String& lastStatus() const { return lastStatus_; }

    const String& manufacturer() const { return manufacturer_; }
    const String& model() const { return model_; }
    const String& firmware() const { return firmware_; }
    const String& c5Firmware() const { return c5Firmware_; }
    const String& deviceStatus() const { return deviceStatus_; }

    // Commands are deliberately supplied by the UI from a fixed, read-only
    // allow-list. This client does not expose attack or configuration helpers.
    bool sendReadOnlyCommand(const String& command, String& response,
                             uint32_t timeoutMs = 5000);
    bool sendCommandNoWait(const String& command);
    String takeNotifications();

private:
    void onNotify(const uint8_t* data, size_t length);
    String readText(NimBLERemoteCharacteristic* characteristic);

    NimBLEClient* client_ = nullptr;
    NimBLERemoteCharacteristic* commandChar_ = nullptr;
    bool initialized_ = false;
    String lastStatus_ = "Not connected";
    String manufacturer_;
    String model_;
    String firmware_;
    String c5Firmware_;
    String deviceStatus_;

    static constexpr size_t kResponseCapacity = 2048;
    char response_[kResponseCapacity];
    volatile size_t responseLen_ = 0;
    volatile uint32_t lastNotifyMs_ = 0;
};
