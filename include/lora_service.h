#pragma once

#include <Arduino.h>
#include <RadioLib.h>
#include <M5Unified.h>
#include <utility/PI4IOE5V6408_Class.hpp>
#include <vector>

#include "meshtastic_decoder.h"

class LoRaService {
public:
    struct MeshNode {
        uint32_t id = 0;
        String longName;
        String shortName;
        uint32_t lastSeenMs = 0;
        float lastRssi = 0.0F;
        float lastSnr = 0.0F;
        uint32_t packets = 0;
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

    struct MeshMessage {
        uint32_t from = 0;
        uint32_t to = 0;
        uint32_t packetId = 0;
        uint32_t receivedMs = 0;
        String text;
        String channel;
        bool outgoing = false;
    };

    enum class Profile {
        MeshtasticEuLongFast,
        M5StackGeneric,
    };

    bool begin();
    void update();
    bool restartReceive();
    void end();
    bool toggleProfile();

    bool isReady() const { return ready_; }
    int status() const { return status_; }
    uint32_t packetCount() const { return packetCount_; }
    uint32_t receivedMessageCount() const { return receivedMessageCount_; }
    float lastRssi() const { return lastRssi_; }
    float lastSnr() const { return lastSnr_; }
    const String& lastPacket() const { return lastPacket_; }
    const MeshtasticDecoded& lastDecoded() const { return lastDecoded_; }
    Profile profile() const { return profile_; }
    const char* profileName() const;
    float frequencyMhz() const;
    const std::vector<MeshNode>& nodes() const { return nodes_; }
    const std::vector<MeshMessage>& messages() const { return messages_; }
    String nodeDisplayName(uint32_t id) const;
    void restoreNode(const MeshNode& node);
    void restoreMessage(const MeshMessage& message);
    void setMeshChannels(const std::vector<MeshtasticChannel>& channels) {
        decoder_.setChannels(channels);
    }
    const std::vector<MeshtasticChannel>& meshChannels() const {
        return decoder_.channels();
    }
    bool sendText(const String& text, size_t channelIndex, uint32_t nodeId,
                  uint8_t hopLimit);
    bool sendNodeInfo(const String& longName, const String& shortName,
                      size_t channelIndex, uint32_t nodeId, uint8_t hopLimit);
    const String& transmitStatus() const { return transmitStatus_; }
    uint32_t nextTransmitMs() const { return nextTransmitMs_; }

private:
    Module module_{5, 4, 3, 6};
    SX1262 radio_{&module_};
    m5::PI4IOE5V6408_Class ioe_{0x43, 400000, &m5::In_I2C};
    bool ready_ = false;
    int status_ = RADIOLIB_ERR_UNKNOWN;
    uint32_t packetCount_ = 0;
    uint32_t receivedMessageCount_ = 0;
    float lastRssi_ = 0.0F;
    float lastSnr_ = 0.0F;
    String lastPacket_;
    std::vector<uint8_t> lastRawPacket_;
    MeshtasticDecoded lastDecoded_;
    MeshtasticDecoder decoder_;
    Profile profile_ = Profile::MeshtasticEuLongFast;
    std::vector<MeshNode> nodes_;
    std::vector<MeshMessage> messages_;

    void observeDecodedPacket();
    bool transmitPacket(const std::vector<uint8_t>& packet,
                        const String& successStatus);
    String transmitStatus_;
    uint32_t nextTransmitMs_ = 0;
};
