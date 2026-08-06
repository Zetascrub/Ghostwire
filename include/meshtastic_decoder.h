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
    uint32_t requestId = 0;
    uint32_t routingError = 0;
    bool hasRoutingResult = false;
    std::vector<uint8_t> payload;
    String summary;
    String longName;
    String shortName;
    std::vector<uint8_t> publicKey;
    bool hasPosition = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int32_t altitude = 0;
    bool hasDeviceMetrics = false;
    uint32_t batteryLevel = 0;
    float voltage = 0.0F;
    float channelUtilization = 0.0F;
    float airUtilTx = 0.0F;
    String channelName;
};

struct MeshtasticChannel {
    String name;
    std::vector<uint8_t> key;
    uint8_t hash = 0;
    bool isPublic = false;
};

class MeshtasticDecoder {
public:
    bool decodePublic(const uint8_t* packet, size_t length,
                      MeshtasticDecoded& result,
                      const std::vector<uint8_t>* senderPublicKey = nullptr) const;
    static const char* portName(uint32_t port);
    void setChannels(const std::vector<MeshtasticChannel>& channels);
    const std::vector<MeshtasticChannel>& channels() const { return channels_; }
    static uint8_t channelHash(const String& name,
                               const std::vector<uint8_t>& key);
    bool encodeText(const String& text, size_t channelIndex, uint32_t from,
                    uint32_t to, uint32_t packetId, uint8_t hopLimit,
                    const std::vector<uint8_t>* recipientPublicKey,
                    std::vector<uint8_t>& packet) const;
    bool encodeNodeInfo(const String& longName, const String& shortName,
                        size_t channelIndex, uint32_t from, uint32_t packetId,
                        uint8_t hopLimit, std::vector<uint8_t>& packet) const;
    bool encodeRequest(uint32_t port, size_t channelIndex, uint32_t from,
                       uint32_t to, uint32_t packetId, uint8_t hopLimit,
                       const std::vector<uint8_t>* recipientPublicKey,
                       std::vector<uint8_t>& packet) const;
    bool encodePosition(double latitude, double longitude, int32_t altitude,
                        size_t channelIndex, uint32_t from, uint32_t packetId,
                        uint8_t hopLimit, std::vector<uint8_t>& packet) const;
    void setLocalKeyPair(const std::vector<uint8_t>& privateKey,
                         const std::vector<uint8_t>& publicKey);
    static bool readVarint(const uint8_t* data, size_t length, size_t& offset,
                           uint64_t& value);

private:
    bool encodeApplication(const std::vector<uint8_t>& payload, uint32_t port,
                           size_t channelIndex, uint32_t from,
                           uint32_t to, uint32_t packetId, uint8_t hopLimit,
                           bool wantAck,
                           bool wantResponse,
                           const std::vector<uint8_t>* recipientPublicKey,
                           std::vector<uint8_t>& packet) const;
    static bool decodeData(const uint8_t* data, size_t length,
                           MeshtasticDecoded& result);
    static void decodeApplicationPayload(MeshtasticDecoded& result);
    std::vector<MeshtasticChannel> channels_;
    std::vector<uint8_t> privateKey_;
    std::vector<uint8_t> publicKey_;
    std::vector<uint8_t> signingPrivateKey_;
    std::vector<uint8_t> signingPublicKey_;
};
