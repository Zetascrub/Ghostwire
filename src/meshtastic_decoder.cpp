#include "meshtastic_decoder.h"

#include <cstring>
#include <mbedtls/aes.h>

namespace {
constexpr size_t kHeaderLength = 16;
constexpr uint8_t kPublicKey[16] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01,
};

uint32_t readLe32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8) |
           (static_cast<uint32_t>(value[2]) << 16) |
           (static_cast<uint32_t>(value[3]) << 24);
}

String printableText(const std::vector<uint8_t>& payload) {
    String text;
    text.reserve(payload.size());
    for (uint8_t value : payload) {
        if (value >= 32 && value <= 126) {
            text += static_cast<char>(value);
        } else {
            text += '.';
        }
    }
    return text;
}
}  // namespace

bool MeshtasticDecoder::decodePublic(const uint8_t* packet, size_t length,
                                     MeshtasticDecoded& result) const {
    result = MeshtasticDecoded{};
    if (!packet || length <= kHeaderLength || length > 255) return false;

    result.to = readLe32(packet);
    result.from = readLe32(packet + 4);
    result.id = readLe32(packet + 8);
    result.channelHash = packet[13];
    if (result.from == 0) return false;

    std::vector<uint8_t> plain(packet + kHeaderLength, packet + length);
    uint8_t nonce[16] = {};
    memcpy(nonce, &result.id, sizeof(result.id));
    memcpy(nonce + 8, &result.from, sizeof(result.from));

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, kPublicKey, 128) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    size_t nonceOffset = 0;
    uint8_t streamBlock[16] = {};
    const int status =
        mbedtls_aes_crypt_ctr(&aes, plain.size(), &nonceOffset, nonce,
                              streamBlock, plain.data(), plain.data());
    mbedtls_aes_free(&aes);
    if (status != 0 || !decodeData(plain.data(), plain.size(), result)) {
        return false;
    }

    result.valid = true;
    if (result.port == 1 || result.port == 10 || result.port == 11 ||
        result.port == 32 || result.port == 66) {
        result.summary = printableText(result.payload);
    } else {
        result.summary = portName(result.port);
    }
    return true;
}

bool MeshtasticDecoder::readVarint(const uint8_t* data, size_t length,
                                   size_t& offset, uint64_t& value) {
    value = 0;
    for (uint8_t shift = 0; shift < 64 && offset < length; shift += 7) {
        const uint8_t byte = data[offset++];
        value |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) return true;
    }
    return false;
}

bool MeshtasticDecoder::decodeData(const uint8_t* data, size_t length,
                                   MeshtasticDecoded& result) {
    bool foundPort = false;
    size_t offset = 0;
    while (offset < length) {
        uint64_t key = 0;
        if (!readVarint(data, length, offset, key) || key == 0) return false;
        const uint32_t field = key >> 3;
        const uint8_t wire = key & 7;
        if (wire == 0) {
            uint64_t value = 0;
            if (!readVarint(data, length, offset, value)) return false;
            if (field == 1) {
                result.port = static_cast<uint32_t>(value);
                foundPort = result.port > 0 && result.port <= 511;
            }
        } else if (wire == 2) {
            uint64_t valueLength = 0;
            if (!readVarint(data, length, offset, valueLength) ||
                valueLength > length - offset) {
                return false;
            }
            if (field == 2) {
                result.payload.assign(data + offset,
                                      data + offset + valueLength);
            }
            offset += valueLength;
        } else if (wire == 1) {
            if (length - offset < 8) return false;
            offset += 8;
        } else if (wire == 5) {
            if (length - offset < 4) return false;
            offset += 4;
        } else {
            return false;
        }
    }
    return foundPort;
}

const char* MeshtasticDecoder::portName(uint32_t port) {
    switch (port) {
        case 1: return "TEXT";
        case 3: return "POSITION";
        case 4: return "NODEINFO";
        case 5: return "ROUTING";
        case 7: return "TEXT COMPRESSED";
        case 8: return "WAYPOINT";
        case 10: return "DETECTION";
        case 11: return "ALERT";
        case 32: return "REPLY";
        case 66: return "RANGE TEST";
        case 67: return "TELEMETRY";
        case 70: return "TRACEROUTE";
        case 71: return "NEIGHBOR";
        default: return "MESHTASTIC DATA";
    }
}
