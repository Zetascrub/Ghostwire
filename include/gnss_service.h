#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

class GnssService {
public:
    static constexpr int kReceivePin = 15;
    static constexpr int kTransmitPin = 13;
    static constexpr uint32_t kBaud = 115200;

    void begin();
    void update();
    void restart();

    bool hasData() const;
    bool hasFix() const;
    uint32_t charactersProcessed() const;
    uint32_t satellites();
    double latitude();
    double longitude();
    double altitudeMetres();
    double hdop();
    String utcTime();
    bool hasUtcDateTime() const;
    uint16_t utcYear();
    uint8_t utcMonth();
    uint8_t utcDay();
    uint8_t utcHour();
    uint8_t utcMinute();
    uint8_t utcSecond();

private:
    HardwareSerial serial_{1};
    TinyGPSPlus parser_;
    bool started_ = false;
};
