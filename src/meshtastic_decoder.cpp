#include "meshtastic_decoder.h"

#include <cstring>
#include <Curve25519.h>
#include <XEdDSA.h>
#include <esp_system.h>
#include <mbedtls/aes.h>
#include <mbedtls/ccm.h>
#include <mbedtls/sha256.h>

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

int32_t readLeI32(const uint8_t* value) {
    return static_cast<int32_t>(readLe32(value));
}

float readLeFloat(const uint8_t* value) {
    const uint32_t raw = readLe32(value);
    float result = 0.0F;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

void decodeDeviceMetrics(const uint8_t* data, size_t length,
                         MeshtasticDecoded& result) {
    size_t offset = 0;
    while (offset < length) {
        uint64_t key = 0;
        if (!MeshtasticDecoder::readVarint(data, length, offset, key)) return;
        const uint32_t field = key >> 3;
        const uint8_t wire = key & 7;
        if (wire == 0) {
            uint64_t value = 0;
            if (!MeshtasticDecoder::readVarint(data, length, offset, value)) return;
            if (field == 1) result.batteryLevel = static_cast<uint32_t>(value);
        } else if (wire == 5) {
            if (length - offset < 4) return;
            const float value = readLeFloat(data + offset);
            if (field == 2) result.voltage = value;
            if (field == 3) result.channelUtilization = value;
            if (field == 4) result.airUtilTx = value;
            offset += 4;
        } else {
            return;
        }
    }
    result.hasDeviceMetrics = true;
}

bool deriveSharedKey(const std::vector<uint8_t>& privateKey,
                     const std::vector<uint8_t>& remotePublic,
                     uint8_t sharedKey[32]) {
    if (privateKey.size() != 32 || remotePublic.size() != 32) return false;
    uint8_t secret[32];
    if (!Curve25519::eval(secret, privateKey.data(), remotePublic.data())) {
        memset(secret, 0, sizeof(secret));
        return false;
    }
    const int status = mbedtls_sha256_ret(secret, sizeof(secret), sharedKey, 0);
    memset(secret, 0, sizeof(secret));
    return status == 0;
}
}  // namespace

bool MeshtasticDecoder::decodePublic(const uint8_t* packet, size_t length,
                                     MeshtasticDecoded& result,
                                     const std::vector<uint8_t>* senderPublicKey) const {
    result = MeshtasticDecoded{};
    if (!packet || length <= kHeaderLength || length > 255) return false;

    result.to = readLe32(packet);
    result.from = readLe32(packet + 4);
    result.id = readLe32(packet + 8);
    result.channelHash = packet[13];
    if (result.from == 0) return false;

    if (result.channelHash == 0 && senderPublicKey != nullptr &&
        senderPublicKey->size() == 32 && privateKey_.size() == 32 &&
        length > kHeaderLength + 12) {
        const size_t cipherLength = length - kHeaderLength - 12;
        const uint8_t* cipher = packet + kHeaderLength;
        const uint8_t* tag = cipher + cipherLength;
        uint32_t extraNonce = 0;
        memcpy(&extraNonce, tag + 8, sizeof(extraNonce));
        uint8_t nonce[16] = {};
        memcpy(nonce, &result.id, sizeof(result.id));
        memcpy(nonce + 4, &extraNonce, sizeof(extraNonce));
        memcpy(nonce + 8, &result.from, sizeof(result.from));
        uint8_t sharedKey[32];
        if (!deriveSharedKey(privateKey_, *senderPublicKey, sharedKey)) {
            return false;
        }
        std::vector<uint8_t> plain(cipherLength);
        mbedtls_ccm_context ccm;
        mbedtls_ccm_init(&ccm);
        int status = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES,
                                        sharedKey, 256);
        if (status == 0) {
            status = mbedtls_ccm_auth_decrypt(
                &ccm, cipherLength, nonce, 13, nullptr, 0, cipher,
                plain.data(), tag, 8);
        }
        mbedtls_ccm_free(&ccm);
        memset(sharedKey, 0, sizeof(sharedKey));
        if (status != 0 || !decodeData(plain.data(), plain.size(), result)) {
            return false;
        }
        result.channelName = "PKI";
        result.valid = true;
        decodeApplicationPayload(result);
        result.summary = (result.port == 1 || result.port == 7 ||
                          result.port == 32)
                             ? printableText(result.payload)
                             : portName(result.port);
        return true;
    }

    const MeshtasticChannel* channel = nullptr;
    for (const auto& candidate : channels_) {
        if (candidate.hash == result.channelHash) {
            channel = &candidate;
            break;
        }
    }
    std::vector<uint8_t> fallbackKey(kPublicKey, kPublicKey + sizeof(kPublicKey));
    MeshtasticChannel fallback{"LongFast", fallbackKey,
                               channelHash("LongFast", fallbackKey), true};
    if (channel == nullptr && result.channelHash == fallback.hash) {
        channel = &fallback;
    }
    if (channel == nullptr) return false;
    result.channelName = channel->name;

    std::vector<uint8_t> plain(packet + kHeaderLength, packet + length);
    uint8_t nonce[16] = {};
    memcpy(nonce, &result.id, sizeof(result.id));
    memcpy(nonce + 8, &result.from, sizeof(result.from));

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (!channel->key.empty() &&
        mbedtls_aes_setkey_enc(&aes, channel->key.data(),
                               channel->key.size() * 8) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    size_t nonceOffset = 0;
    uint8_t streamBlock[16] = {};
    const int status = channel->key.empty()
                           ? 0
                           : mbedtls_aes_crypt_ctr(
                                 &aes, plain.size(), &nonceOffset, nonce,
                                 streamBlock, plain.data(), plain.data());
    mbedtls_aes_free(&aes);
    if (status != 0 || !decodeData(plain.data(), plain.size(), result)) {
        return false;
    }

    result.valid = true;
    decodeApplicationPayload(result);
    if (result.port == 1 || result.port == 7 || result.port == 32) {
        result.summary = printableText(result.payload);
    } else if (result.port == 4 && !result.longName.isEmpty()) {
        result.summary = result.longName;
    } else if (result.port == 3 && result.hasPosition) {
        result.summary = String(result.latitude, 5) + ", " +
                         String(result.longitude, 5);
    } else {
        result.summary = portName(result.port);
    }
    return true;
}

uint8_t MeshtasticDecoder::channelHash(
    const String& name, const std::vector<uint8_t>& key) {
    uint8_t result = 0;
    const String effectiveName = name.isEmpty() ? "X" : name;
    for (size_t index = 0; index < effectiveName.length(); ++index) {
        result ^= static_cast<uint8_t>(effectiveName[index]);
    }
    for (uint8_t value : key) result ^= value;
    return result;
}

void MeshtasticDecoder::setChannels(
    const std::vector<MeshtasticChannel>& channels) {
    channels_.clear();
    for (const auto& channel : channels) {
        if (channels_.size() >= 4) break;
        if (channel.name.isEmpty() ||
            (!channel.key.empty() && channel.key.size() != 16 &&
             channel.key.size() != 32)) continue;
        MeshtasticChannel copy = channel;
        copy.hash = channelHash(copy.name, copy.key);
        channels_.push_back(copy);
    }
}

void MeshtasticDecoder::setLocalKeyPair(
    const std::vector<uint8_t>& privateKey,
    const std::vector<uint8_t>& publicKey) {
    privateKey_ = privateKey.size() == 32 ? privateKey : std::vector<uint8_t>{};
    publicKey_ = publicKey.size() == 32 ? publicKey : std::vector<uint8_t>{};
    signingPrivateKey_.clear();
    signingPublicKey_.clear();
    if (privateKey_.size() == 32 && publicKey_.size() == 32) {
        signingPrivateKey_.resize(32);
        signingPublicKey_.resize(32);
        std::vector<uint8_t> curvePrivate = privateKey_;
        XEdDSA::priv_curve_to_ed_keys(curvePrivate.data(),
                                     signingPrivateKey_.data(),
                                     signingPublicKey_.data());
    }
}

bool MeshtasticDecoder::encodeText(const String& text, size_t channelIndex,
                                   uint32_t from, uint32_t to,
                                   uint32_t packetId,
                                   uint8_t hopLimit,
                                   const std::vector<uint8_t>* recipientPublicKey,
                                   std::vector<uint8_t>& packet) const {
    if (channelIndex >= channels_.size() || from == 0 || text.isEmpty() ||
        text.length() > 180 || hopLimit < 1 || hopLimit > 7) return false;
    std::vector<uint8_t> payload(text.c_str(), text.c_str() + text.length());
    return encodeApplication(payload, 1, channelIndex, from, to, packetId,
                             hopLimit, to != 0xffffffffU, false,
                             recipientPublicKey, packet);
}

bool MeshtasticDecoder::encodeNodeInfo(const String& longName,
                                       const String& shortName,
                                       size_t channelIndex, uint32_t from,
                                       uint32_t packetId, uint8_t hopLimit,
                                       std::vector<uint8_t>& packet) const {
    if (longName.isEmpty() || longName.length() > 24 || shortName.isEmpty() ||
        shortName.length() > 4) return false;
    std::vector<uint8_t> user;
    const auto appendString = [&](uint8_t tag, const String& value) {
        user.push_back(tag);
        user.push_back(static_cast<uint8_t>(value.length()));
        user.insert(user.end(), value.c_str(), value.c_str() + value.length());
    };
    char nodeId[10];
    snprintf(nodeId, sizeof(nodeId), "!%08lx",
             static_cast<unsigned long>(from));
    appendString(0x0a, nodeId);    // User.id
    appendString(0x12, longName);  // User.long_name
    appendString(0x1a, shortName); // User.short_name
    if (publicKey_.size() == 32) {
        user.push_back(0x42);      // User.public_key
        user.push_back(32);
        user.insert(user.end(), publicKey_.begin(), publicKey_.end());
    }
    user.push_back(0x38);          // User.role
    user.push_back(0x01);          // CLIENT_MUTE
    return encodeApplication(user, 4, channelIndex, from, 0xffffffffU,
                             packetId, hopLimit, false, true, nullptr, packet);
}

bool MeshtasticDecoder::encodeApplication(
    const std::vector<uint8_t>& payload, uint32_t port, size_t channelIndex,
    uint32_t from, uint32_t to, uint32_t packetId, uint8_t hopLimit,
    bool wantAck,
    bool wantResponse,
    const std::vector<uint8_t>* recipientPublicKey,
    std::vector<uint8_t>& packet) const {
    if (channelIndex >= channels_.size() || from == 0 || payload.empty() ||
        payload.size() > 220 || port > 127 || hopLimit < 1 || hopLimit > 7) {
        return false;
    }
    const auto& channel = channels_[channelIndex];
    std::vector<uint8_t> plain;
    plain.reserve(payload.size() + 5);
    plain.push_back(0x08);  // Data.portnum, varint
    plain.push_back(static_cast<uint8_t>(port));
    plain.push_back(0x12);  // Data.payload, length-delimited
    if (payload.size() < 128) {
        plain.push_back(static_cast<uint8_t>(payload.size()));
    } else {
        plain.push_back(static_cast<uint8_t>(payload.size()) | 0x80);
        plain.push_back(static_cast<uint8_t>(payload.size() >> 7));
    }
    plain.insert(plain.end(), payload.begin(), payload.end());
    if (wantResponse) {
        plain.push_back(0x18);  // Data.want_response
        plain.push_back(0x01);
    }
    if (to != 0xffffffffU) {
        plain.push_back(0x48);  // Data.bitfield (present, value zero)
        plain.push_back(0x00);
    } else if (wantResponse) {
        plain.push_back(0x48);  // Data.bitfield
        plain.push_back(0x02);  // BITFIELD_WANT_RESPONSE_MASK
    }
    if (to == 0xffffffffU && signingPrivateKey_.size() == 32 &&
               signingPublicKey_.size() == 32) {
        uint8_t signature[64];
        esp_fill_random(signature, 32);
        std::vector<uint8_t> signedMessage(12 + payload.size());
        memcpy(signedMessage.data(), &from, sizeof(from));
        memcpy(signedMessage.data() + 4, &packetId, sizeof(packetId));
        memcpy(signedMessage.data() + 8, &port, sizeof(port));
        memcpy(signedMessage.data() + 12, payload.data(), payload.size());
        XEdDSA::sign(signature, signingPrivateKey_.data(),
                     signingPublicKey_.data(), signedMessage.data(),
                     signedMessage.size());
        plain.push_back(0x52);  // Data.xeddsa_signature
        plain.push_back(64);
        plain.insert(plain.end(), signature, signature + sizeof(signature));
        memset(signature, 0, sizeof(signature));
    }

    if (plain.size() + kHeaderLength > 255) return false;

    packet.assign(kHeaderLength + plain.size(), 0);
    const auto writeLe32 = [&](size_t offset, uint32_t value) {
        packet[offset] = value & 0xff;
        packet[offset + 1] = (value >> 8) & 0xff;
        packet[offset + 2] = (value >> 16) & 0xff;
        packet[offset + 3] = (value >> 24) & 0xff;
    };
    writeLe32(0, to);
    writeLe32(4, from);
    writeLe32(8, packetId);
    packet[12] = static_cast<uint8_t>(hopLimit | (hopLimit << 5) |
                                      (wantAck ? 0x08 : 0x00));
    const bool pki = to != 0xffffffffU && recipientPublicKey != nullptr &&
                     recipientPublicKey->size() == 32 && privateKey_.size() == 32;
    if (to != 0xffffffffU && !pki) return false;
    packet[13] = pki ? 0 : channel.hash;
    packet[14] = 0;
    packet[15] = static_cast<uint8_t>(from & 0xff);

    if (pki) {
        uint8_t sharedKey[32];
        if (!deriveSharedKey(privateKey_, *recipientPublicKey, sharedKey)) {
            return false;
        }
        const uint32_t extraNonce = esp_random();
        uint8_t nonce[16] = {};
        memcpy(nonce, &packetId, sizeof(packetId));
        memcpy(nonce + 4, &extraNonce, sizeof(extraNonce));
        memcpy(nonce + 8, &from, sizeof(from));
        packet.resize(kHeaderLength + plain.size() + 12);
        uint8_t* cipher = packet.data() + kHeaderLength;
        uint8_t* tag = cipher + plain.size();
        mbedtls_ccm_context ccm;
        mbedtls_ccm_init(&ccm);
        int status = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES,
                                        sharedKey, 256);
        if (status == 0) {
            status = mbedtls_ccm_encrypt_and_tag(
                &ccm, plain.size(), nonce, 13, nullptr, 0, plain.data(),
                cipher, tag, 8);
        }
        mbedtls_ccm_free(&ccm);
        memset(sharedKey, 0, sizeof(sharedKey));
        if (status != 0) return false;
        memcpy(tag + 8, &extraNonce, sizeof(extraNonce));
    } else if (!channel.key.empty()) {
        uint8_t nonce[16] = {};
        memcpy(nonce, &packetId, sizeof(packetId));
        memcpy(nonce + 8, &from, sizeof(from));
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        if (mbedtls_aes_setkey_enc(&aes, channel.key.data(),
                                   channel.key.size() * 8) != 0) {
            mbedtls_aes_free(&aes);
            return false;
        }
        size_t nonceOffset = 0;
        uint8_t streamBlock[16] = {};
        const int status = mbedtls_aes_crypt_ctr(
            &aes, plain.size(), &nonceOffset, nonce, streamBlock, plain.data(),
            packet.data() + kHeaderLength);
        mbedtls_aes_free(&aes);
        if (status != 0) return false;
    } else {
        memcpy(packet.data() + kHeaderLength, plain.data(), plain.size());
    }
    return true;
}


void MeshtasticDecoder::decodeApplicationPayload(MeshtasticDecoded& result) {
    size_t offset = 0;
    while (offset < result.payload.size()) {
        uint64_t key = 0;
        if (!readVarint(result.payload.data(), result.payload.size(), offset,
                        key) || key == 0) return;
        const uint32_t field = key >> 3;
        const uint8_t wire = key & 7;
        if (wire == 2) {
            uint64_t length = 0;
            if (!readVarint(result.payload.data(), result.payload.size(),
                            offset, length) ||
                length > result.payload.size() - offset) return;
            if (result.port == 4 && (field == 2 || field == 3)) {
                String value;
                value.reserve(length);
                for (size_t i = 0; i < length; ++i) {
                    const uint8_t c = result.payload[offset + i];
                    if (c >= 32 && c <= 126) value += static_cast<char>(c);
                }
                if (field == 2) result.longName = value;
                if (field == 3) result.shortName = value;
            }
            if (result.port == 4 && field == 8 && length == 32) {
                result.publicKey.assign(result.payload.begin() + offset,
                                        result.payload.begin() + offset + 32);
            }
            if (result.port == 67 && field == 2) {
                decodeDeviceMetrics(result.payload.data() + offset, length,
                                    result);
            }
            offset += length;
        } else if (wire == 5) {
            if (result.payload.size() - offset < 4) return;
            if (result.port == 3) {
                if (field == 1) {
                    result.latitude = readLeI32(result.payload.data() + offset) *
                                      1e-7;
                    result.hasPosition = true;
                } else if (field == 2) {
                    result.longitude = readLeI32(result.payload.data() + offset) *
                                       1e-7;
                }
            }
            offset += 4;
        } else if (wire == 0) {
            uint64_t value = 0;
            if (!readVarint(result.payload.data(), result.payload.size(),
                            offset, value)) return;
            if (result.port == 3 && field == 3) {
                result.altitude = static_cast<int32_t>(value);
            }
        } else if (wire == 1) {
            if (result.payload.size() - offset < 8) return;
            offset += 8;
        } else {
            return;
        }
    }
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
