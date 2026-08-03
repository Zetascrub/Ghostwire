#pragma once

#include <Arduino.h>

enum class BleSpamMode { Apple, FastPair, SwiftPair, All };

class NimBLEAdvertising;

class BleSpamService {
public:
    bool begin(BleSpamMode mode);
    void end();
    void update();

    bool isActive() const { return active_; }
    uint32_t packetsSent() const { return packetsSent_; }
    const char* currentTypeName() const;
    const uint8_t* currentAddress() const { return currentAddress_; }

private:
    void broadcastNext();
    void randomizeAddress();
    BleSpamMode nextConcreteMode();

    NimBLEAdvertising* advertising_ = nullptr;
    bool initialized_ = false;
    bool active_ = false;
    BleSpamMode mode_ = BleSpamMode::All;
    BleSpamMode currentConcreteMode_ = BleSpamMode::Apple;
    size_t rotationIndex_ = 0;
    size_t varietyIndex_ = 0;
    uint32_t packetsSent_ = 0;
    unsigned long nextCycleMs_ = 0;
    uint8_t currentAddress_[6] = {0, 0, 0, 0, 0, 0};
};
