#pragma once

#include <Arduino.h>

class NimBLEClient;
class NimBLERemoteCharacteristic;

class ChameleonUltraClient {
public:
    bool connect(uint32_t scanTimeoutMs = 5000);
    void disconnect();
    bool isConnected() const { return connected_; }
    const String& lastStatus() const { return lastStatus_; }

    bool getAppVersion(uint8_t& major, uint8_t& minor);
    bool getBatteryInfo(uint16_t& millivolts, uint8_t& percentage);

    struct HfTag {
        uint8_t uid[10];
        uint8_t uidLen;
        uint16_t atqa;
        uint8_t sak;
    };
    // Returns false if no tag is present (or on a communication failure) --
    // the caller doesn't need to know which, same as the getters above.
    bool scanHf14a(HfTag& tag);
    bool scanEm410x(uint8_t id[5]);
    bool stageHfIdentity(const HfTag& tag, uint8_t slot = 7);
    bool stageEm410xIdentity(const uint8_t id[5], uint8_t slot = 7);
    bool setReaderMode();
    bool refreshState();
    uint8_t activeSlot() const { return activeSlot_; }
    bool isReaderMode() const { return readerMode_; }

private:
    bool sendCommand(uint16_t cmd, const uint8_t* data, uint16_t dataLen,
                      uint8_t* respData, size_t respDataCapacity,
                      uint16_t& respLen, uint16_t& respStatus,
                      uint32_t timeoutMs = 2000);
    bool simpleCommand(uint16_t cmd, const uint8_t* data, uint16_t dataLen);
    void onNotify(const uint8_t* data, size_t length);

    NimBLEClient* client_ = nullptr;
    NimBLERemoteCharacteristic* writeChar_ = nullptr;
    bool initialized_ = false;
    bool connected_ = false;
    uint8_t activeSlot_ = 0;
    bool readerMode_ = true;
    String lastStatus_ = "Not connected";

    static constexpr size_t kResponseBufSize = 256;
    uint8_t responseBuf_[kResponseBufSize];
    volatile size_t responseLen_ = 0;
    volatile bool responseReady_ = false;
};
