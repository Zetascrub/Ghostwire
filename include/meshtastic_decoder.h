#pragma once

#include <Arduino.h>
#include <vector>

struct MeshtasticDecoded {
    bool valid = false;
    uint32_t to = 0;
    uint32_t from = 0;
    uint32_t id = 0;
    uint8_t channelHash = 0;
    uint32_t port = 0;
    std::vector<uint8_t> payload;
    String summary;
    String longName;
    String shortName;
    bool hasPosition = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int32_t altitude = 0;
    bool hasDeviceMetrics = false;
    uint32_t batteryLevel = 0;
    float voltage = 0.0F;
    float channelUtilization = 0.0F;
    float airUtilTx = 0.0F;
};

class MeshtasticDecoder {
public:
    bool decodePublic(const uint8_t* packet, size_t length,
                      MeshtasticDecoded& result) const;
    static const char* portName(uint32_t port);
    static bool readVarint(const uint8_t* data, size_t length, size_t& offset,
                           uint64_t& value);

private:
    static bool decodeData(const uint8_t* data, size_t length,
                           MeshtasticDecoded& result);
    static void decodeApplicationPayload(MeshtasticDecoded& result);
};
