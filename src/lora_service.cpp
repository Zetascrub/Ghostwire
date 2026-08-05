#include "lora_service.h"

#include <SPI.h>
#include <algorithm>
#include <esp_system.h>

namespace {
volatile bool packetReceived = false;

void IRAM_ATTR onPacketReceived() {
    packetReceived = true;
}
}  // namespace

bool LoRaService::begin() {
    if (ready_) return true;

    if (!m5::In_I2C.begin(I2C_NUM_0, 8, 9)) {
        status_ = RADIOLIB_ERR_CHIP_NOT_FOUND;
        return false;
    }
    if (!ioe_.begin()) {
        status_ = RADIOLIB_ERR_CHIP_NOT_FOUND;
        return false;
    }
    ioe_.setDirection(0, true);
    ioe_.setHighImpedance(0, false);
    ioe_.digitalWrite(0, true);

    // The SX1262 shares the Cardputer's SPI pins with microSD, using its own
    // chip select on G5. Both chip selects remain high when inactive.
    SPI.begin(40, 39, 14, 5);
    digitalWrite(12, HIGH);
    const bool meshtastic = profile_ == Profile::MeshtasticEuLongFast;
    status_ = radio_.begin(meshtastic ? 869.525F : 868.0F,
                           meshtastic ? 250.0F : 125.0F,
                           meshtastic ? 11 : 12, 5,
                           meshtastic ? 0x2B : 0x34, 10,
                           meshtastic ? 16 : 20, 3.0F, true);
    if (status_ != RADIOLIB_ERR_NONE) return false;

    radio_.setCurrentLimit(140);
    radio_.setPacketReceivedAction(onPacketReceived);
    status_ = radio_.startReceive();
    ready_ = status_ == RADIOLIB_ERR_NONE;
    return ready_;
}

void LoRaService::update() {
    if (!ready_ || !packetReceived) return;
    packetReceived = false;

    const size_t length = radio_.getPacketLength();
    std::vector<uint8_t> packet(length);
    status_ = radio_.readData(packet.data(), packet.size());
    if (status_ == RADIOLIB_ERR_NONE ||
        status_ == RADIOLIB_ERR_CRC_MISMATCH) {
        lastRawPacket_ = packet;
        lastPacket_ = "";
        const size_t previewLength = std::min(packet.size(), size_t{18});
        for (size_t index = 0; index < previewLength; ++index) {
            char byte[4];
            snprintf(byte, sizeof(byte), "%02X", packet[index]);
            lastPacket_ += byte;
            if (index + 1 < previewLength) lastPacket_ += ' ';
        }
        lastDecoded_ = MeshtasticDecoded{};
        if (profile_ == Profile::MeshtasticEuLongFast) {
            const std::vector<uint8_t>* senderKey = nullptr;
            if (packet.size() >= 16) {
                const uint32_t sender = static_cast<uint32_t>(packet[4]) |
                    (static_cast<uint32_t>(packet[5]) << 8) |
                    (static_cast<uint32_t>(packet[6]) << 16) |
                    (static_cast<uint32_t>(packet[7]) << 24);
                const auto known = std::find_if(
                    nodes_.begin(), nodes_.end(),
                    [sender](const MeshNode& node) { return node.id == sender; });
                if (known != nodes_.end() && known->publicKey.size() == 32) {
                    senderKey = &known->publicKey;
                }
            }
            decoder_.decodePublic(packet.data(), packet.size(), lastDecoded_,
                                  senderKey);
        }
        lastRssi_ = radio_.getRSSI();
        lastSnr_ = radio_.getSNR();
        ++packetCount_;
        observeDecodedPacket();
    }
    restartReceive();
}

void LoRaService::observeDecodedPacket() {
    if (!lastDecoded_.valid) return;
    auto node = std::find_if(nodes_.begin(), nodes_.end(), [&](const MeshNode& n) {
        return n.id == lastDecoded_.from;
    });
    if (node == nodes_.end()) {
        if (nodes_.size() >= 24) {
            node = std::min_element(nodes_.begin(), nodes_.end(),
                                    [](const MeshNode& a, const MeshNode& b) {
                                        return a.lastSeenMs < b.lastSeenMs;
                                    });
            *node = MeshNode{};
        } else {
            nodes_.push_back(MeshNode{});
            node = nodes_.end() - 1;
        }
        node->id = lastDecoded_.from;
    }
    node->lastSeenMs = millis();
    node->lastRssi = lastRssi_;
    node->lastSnr = lastSnr_;
    ++node->packets;
    if (!lastDecoded_.longName.isEmpty()) node->longName = lastDecoded_.longName;
    if (!lastDecoded_.shortName.isEmpty()) node->shortName = lastDecoded_.shortName;
    if (lastDecoded_.publicKey.size() == 32) {
        node->publicKey = lastDecoded_.publicKey;
    }
    if (lastDecoded_.hasPosition) {
        node->hasPosition = true;
        node->latitude = lastDecoded_.latitude;
        node->longitude = lastDecoded_.longitude;
        node->altitude = lastDecoded_.altitude;
    }
    if (lastDecoded_.hasDeviceMetrics) {
        node->hasDeviceMetrics = true;
        node->batteryLevel = lastDecoded_.batteryLevel;
        node->voltage = lastDecoded_.voltage;
        node->channelUtilization = lastDecoded_.channelUtilization;
        node->airUtilTx = lastDecoded_.airUtilTx;
    }
    if ((lastDecoded_.port == 1 || lastDecoded_.port == 7 ||
         lastDecoded_.port == 32) && !lastDecoded_.summary.isEmpty()) {
        const auto duplicate = std::find_if(
            messages_.begin(), messages_.end(), [&](const MeshMessage& message) {
                return message.from == lastDecoded_.from &&
                       message.packetId == lastDecoded_.id;
            });
        if (duplicate != messages_.end()) return;
        if (messages_.size() >= 32) messages_.erase(messages_.begin());
        messages_.push_back({lastDecoded_.from, lastDecoded_.to,
                             lastDecoded_.id, millis(), lastDecoded_.summary,
                             lastDecoded_.channelName, false});
        ++receivedMessageCount_;
    }
}

String LoRaService::nodeDisplayName(uint32_t id) const {
    const auto node = std::find_if(nodes_.begin(), nodes_.end(),
                                   [id](const MeshNode& n) { return n.id == id; });
    if (node != nodes_.end()) {
        if (!node->longName.isEmpty()) return node->longName;
        if (!node->shortName.isEmpty()) return node->shortName;
    }
    char fallback[10];
    snprintf(fallback, sizeof(fallback), "!%08lX", static_cast<unsigned long>(id));
    return String(fallback);
}

void LoRaService::restoreNode(const MeshNode& node) {
    if (node.id == 0) return;
    const auto existing = std::find_if(nodes_.begin(), nodes_.end(),
                                       [&](const MeshNode& value) {
                                           return value.id == node.id;
                                       });
    if (existing != nodes_.end()) {
        *existing = node;
    } else if (nodes_.size() < 24) {
        nodes_.push_back(node);
    }
}

void LoRaService::restoreMessage(const MeshMessage& message) {
    if (message.from == 0 || message.text.isEmpty()) return;
    const auto duplicate = std::find_if(messages_.begin(), messages_.end(),
                                        [&](const MeshMessage& value) {
        return value.from == message.from &&
               value.packetId == message.packetId;
    });
    if (duplicate == messages_.end()) {
        if (messages_.size() >= 32) messages_.erase(messages_.begin());
        messages_.push_back(message);
    }
}

bool LoRaService::sendText(const String& text, size_t channelIndex,
                           uint32_t nodeId, uint32_t to, uint8_t hopLimit) {
    const uint32_t packetId = esp_random();
    std::vector<uint8_t> packet;
    const std::vector<uint8_t>* recipientKey = nullptr;
    if (to != 0xffffffffU) {
        const auto recipient = std::find_if(
            nodes_.begin(), nodes_.end(),
            [to](const MeshNode& node) { return node.id == to; });
        if (recipient == nodes_.end() || recipient->publicKey.size() != 32) {
            transmitStatus_ = "No public key; await NodeInfo";
            return false;
        }
        recipientKey = &recipient->publicKey;
    }
    if (!decoder_.encodeText(text, channelIndex, nodeId, to, packetId, hopLimit,
                             recipientKey, packet)) {
        transmitStatus_ = "Invalid message or channel";
        return false;
    }
    if (!transmitPacket(packet, to == 0xffffffffU ? "Broadcast sent"
                                                  : "Direct message sent")) {
        return false;
    }
    const String channel = channelIndex < decoder_.channels().size()
                               ? decoder_.channels()[channelIndex].name : "";
    restoreMessage({nodeId, to, packetId, millis(), text, channel,
                    true});
    return true;
}

bool LoRaService::sendNodeInfo(const String& longName, const String& shortName,
                               size_t channelIndex, uint32_t nodeId,
                               uint8_t hopLimit) {
    std::vector<uint8_t> packet;
    if (!decoder_.encodeNodeInfo(longName, shortName, channelIndex, nodeId,
                                 esp_random(), hopLimit, packet)) {
        transmitStatus_ = "Invalid identity or channel";
        return false;
    }
    return transmitPacket(packet, "Identity sent; requesting peer keys");
}

bool LoRaService::transmitPacket(const std::vector<uint8_t>& packet,
                                 const String& successStatus) {
    if (!ready_ || profile_ != Profile::MeshtasticEuLongFast) {
        transmitStatus_ = "Meshtastic radio unavailable";
        return false;
    }
    if (static_cast<int32_t>(millis() - nextTransmitMs_) < 0) {
        transmitStatus_ = "Airtime guard active";
        return false;
    }
    packetReceived = false;
    radio_.standby();
    int16_t cad = radio_.scanChannel();
    for (uint8_t attempt = 0;
         cad == RADIOLIB_LORA_DETECTED && attempt < 3; ++attempt) {
        delay(40 + (esp_random() % 120));
        cad = radio_.scanChannel();
    }
    if (cad == RADIOLIB_LORA_DETECTED) {
        transmitStatus_ = "Channel busy; try again";
        restartReceive();
        return false;
    }
    if (cad != RADIOLIB_CHANNEL_FREE) {
        transmitStatus_ = "Channel check failed: " + String(cad);
        restartReceive();
        return false;
    }
    const uint32_t airtimeUs = radio_.getTimeOnAir(packet.size());
    const int16_t transmitResult =
        radio_.transmit(packet.data(), packet.size());
    restartReceive();
    if (transmitResult != RADIOLIB_ERR_NONE) {
        transmitStatus_ = "Transmit failed: " + String(transmitResult);
        return false;
    }
    nextTransmitMs_ = millis() + std::max<uint32_t>(5000, airtimeUs / 100);
    transmitStatus_ = successStatus;
    return true;
}

bool LoRaService::restartReceive() {
    if (!ready_) return begin();
    packetReceived = false;
    status_ = radio_.startReceive();
    ready_ = status_ == RADIOLIB_ERR_NONE;
    return ready_;
}

void LoRaService::end() {
    if (ready_) radio_.standby();
    ready_ = false;
    packetReceived = false;
    transmitStatus_ = "Meshtastic radio stopped";
}

bool LoRaService::toggleProfile() {
    if (ready_) radio_.standby();
    ready_ = false;
    profile_ = profile_ == Profile::MeshtasticEuLongFast
                   ? Profile::M5StackGeneric
                   : Profile::MeshtasticEuLongFast;
    return begin();
}

const char* LoRaService::profileName() const {
    return profile_ == Profile::MeshtasticEuLongFast ? "Meshtastic LongFast"
                                                      : "M5Stack generic";
}

float LoRaService::frequencyMhz() const {
    return profile_ == Profile::MeshtasticEuLongFast ? 869.525F : 868.0F;
}
