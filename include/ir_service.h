#pragma once

#include <Arduino.h>

class IrService {
public:
    static constexpr uint8_t kTransmitPin = 44;
    static constexpr uint8_t kCarrierKhz = 38;

    void begin();
    void sendSelfTest();
    uint32_t transmissionCount() const;

private:
    bool initialized_ = false;
    uint32_t transmissionCount_ = 0;
};
