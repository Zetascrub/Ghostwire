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
};

class MeshtasticDecoder {
public:
    bool decodePublic(const uint8_t* packet, size_t length,
                      MeshtasticDecoded& result) const;
    static const char* portName(uint32_t port);

private:
    static bool readVarint(const uint8_t* data, size_t length, size_t& offset,
                           uint64_t& value);
    static bool decodeData(const uint8_t* data, size_t length,
                           MeshtasticDecoded& result);
};
