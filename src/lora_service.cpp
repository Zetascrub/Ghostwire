#include "lora_service.h"

#include <SPI.h>
#include <algorithm>
#include <esp_system.h>
#include <time.h>

namespace {
volatile bool packetReceived = false;

uint32_t meshKeyCrc32(const std::vector<uint8_t>& value) {
    uint32_t crc = 0xffffffffU;
    for (uint8_t byte : value) {
        crc ^= byte;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
        }
    }
    return ~crc;
}

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
    for (auto& message : messages_) {
        if (message.delivery == MeshMessage::Delivery::Pending &&
            static_cast<uint32_t>(millis() - message.receivedMs) > 180000U) {
            message.delivery = MeshMessage::Delivery::NoAck;
            message.archived = false;
            ++messageRevision_;
        }
    }
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
        if (!node->publicKey.empty() && node->publicKey != lastDecoded_.publicKey) {
            node->pendingPublicKey = lastDecoded_.publicKey;
            node->keyState = MeshNode::KeyState::Changed;
        } else {
            node->publicKey = lastDecoded_.publicKey;
            node->keyState = meshKeyCrc32(node->publicKey) == node->id
                                 ? MeshNode::KeyState::Bound
                                 : MeshNode::KeyState::Legacy;
        }
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
        MeshNode::TelemetrySample sample;
        const time_t now = time(nullptr);
        sample.timestamp = now > 1700000000 ? static_cast<uint32_t>(now) : 0;
        sample.batteryLevel = lastDecoded_.batteryLevel;
        sample.voltage = lastDecoded_.voltage;
        sample.rssi = lastRssi_;
        if (node->telemetryHistory.size() >= 6) {
            node->telemetryHistory.erase(node->telemetryHistory.begin());
        }
        node->telemetryHistory.push_back(sample);
    }
    if (lastDecoded_.hasRoutingResult && lastDecoded_.requestId != 0) {
        const auto sent = std::find_if(
            messages_.begin(), messages_.end(), [&](const MeshMessage& message) {
                return message.outgoing &&
                       message.packetId == lastDecoded_.requestId;
            });
        if (sent != messages_.end()) {
            sent->routingError = lastDecoded_.routingError;
            sent->delivery = lastDecoded_.routingError == 0
                                 ? MeshMessage::Delivery::Delivered
                                 : MeshMessage::Delivery::Failed;
            sent->archived = false;
            ++messageRevision_;
        }
        return;
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
        MeshMessage message;
        message.from = lastDecoded_.from;
        message.to = lastDecoded_.to;
        message.packetId = lastDecoded_.id;
        message.receivedMs = millis();
        const time_t now = time(nullptr);
        message.timestamp = now > 1700000000 ? static_cast<uint32_t>(now) : 0;
        message.text = lastDecoded_.summary;
        message.channel = lastDecoded_.channelName;
        message.unread = true;
        message.delivery = MeshMessage::Delivery::Received;
        messages_.push_back(message);
        ++receivedMessageCount_;
        ++messageRevision_;
    }
}

bool LoRaService::acceptChangedKey(uint32_t nodeId) {
    const auto node = std::find_if(nodes_.begin(), nodes_.end(),
                                   [nodeId](const MeshNode& value) {
        return value.id == nodeId;
    });
    if (node == nodes_.end() || node->pendingPublicKey.size() != 32) return false;
    node->publicKey = node->pendingPublicKey;
    node->pendingPublicKey.clear();
    node->keyState = meshKeyCrc32(node->publicKey) == node->id
                         ? MeshNode::KeyState::Bound
                         : MeshNode::KeyState::Legacy;
    return true;
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

std::vector<LoRaService::MeshConversation> LoRaService::conversations() const {
    std::vector<MeshConversation> result;
    result.reserve(decoder_.channels().size() + messages_.size());
    for (const auto& channel : decoder_.channels()) {
        MeshConversation conversation;
        conversation.channel = channel.name;
        conversation.preview = "No messages yet";
        result.push_back(conversation);
    }
    for (const auto& message : messages_) {
        const bool direct = message.to != 0xffffffffU;
        const uint32_t peer = direct
                                  ? (message.outgoing ? message.to : message.from)
                                  : 0;
        auto conversation = std::find_if(
            result.begin(), result.end(), [&](const MeshConversation& value) {
                return value.direct == direct && value.peer == peer &&
                       (direct || value.channel == message.channel);
            });
        if (conversation == result.end()) {
            MeshConversation value;
            value.direct = direct;
            value.peer = peer;
            value.channel = message.channel;
            result.push_back(value);
            conversation = result.end() - 1;
        }
        if (message.unread && conversation->unread < 99) ++conversation->unread;
        if (message.receivedMs >= conversation->lastMessageMs) {
            conversation->lastMessageMs = message.receivedMs;
            conversation->preview = message.text;
            conversation->lastOutgoing = message.outgoing;
            conversation->delivery = message.delivery;
            if (!message.channel.isEmpty()) conversation->channel = message.channel;
        }
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const MeshConversation& left,
                        const MeshConversation& right) {
        return left.lastMessageMs > right.lastMessageMs;
    });
    return result;
}

void LoRaService::markConversationRead(bool direct, uint32_t peer,
                                       const String& channel) {
    for (auto& message : messages_) {
        const bool messageDirect = message.to != 0xffffffffU;
        const uint32_t messagePeer = message.outgoing ? message.to : message.from;
        if (messageDirect == direct &&
            (direct ? messagePeer == peer : message.channel == channel)) {
            if (message.unread) {
                message.unread = false;
                ++messageRevision_;
            }
        }
    }
}

void LoRaService::markMessageArchived(uint32_t packetId, uint32_t from,
                                      bool outgoing) {
    const auto message = std::find_if(messages_.begin(), messages_.end(),
                                      [packetId, from, outgoing](const MeshMessage& value) {
        return value.packetId == packetId && value.from == from &&
               value.outgoing == outgoing;
    });
    if (message != messages_.end()) message->archived = true;
}

void LoRaService::restoreNode(const MeshNode& node) {
    if (node.id == 0) return;
    const auto existing = std::find_if(nodes_.begin(), nodes_.end(),
                                       [&](const MeshNode& value) {
                                           return value.id == node.id;
                                       });
    MeshNode restored = node;
    if (restored.publicKey.size() == 32 &&
        restored.keyState == MeshNode::KeyState::Unknown) {
        restored.keyState = meshKeyCrc32(restored.publicKey) == restored.id
                                ? MeshNode::KeyState::Bound
                                : MeshNode::KeyState::Legacy;
    }
    if (existing != nodes_.end()) {
        *existing = restored;
    } else if (nodes_.size() < 24) {
        nodes_.push_back(restored);
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
        ++messageRevision_;
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
        if (recipient->keyState == MeshNode::KeyState::Changed) {
            transmitStatus_ = "Key changed; identity not trusted";
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
    MeshMessage message;
    message.from = nodeId;
    message.to = to;
    message.packetId = packetId;
    message.receivedMs = millis();
    const time_t now = time(nullptr);
    message.timestamp = now > 1700000000 ? static_cast<uint32_t>(now) : 0;
    message.text = text;
    message.channel = channel;
    message.outgoing = true;
    message.delivery = to == 0xffffffffU ? MeshMessage::Delivery::Sent
                                         : MeshMessage::Delivery::Pending;
    restoreMessage(message);
    return true;
}

bool LoRaService::sendRequest(uint32_t port, size_t channelIndex,
                              uint32_t nodeId, uint32_t to,
                              uint8_t hopLimit) {
    std::vector<uint8_t> packet;
    const std::vector<uint8_t>* recipientKey = nullptr;
    if (port == 67) {
        const auto recipient = std::find_if(
            nodes_.begin(), nodes_.end(),
            [to](const MeshNode& node) { return node.id == to; });
        if (recipient == nodes_.end() || recipient->publicKey.size() != 32 ||
            recipient->keyState == MeshNode::KeyState::Changed) {
            transmitStatus_ = "Telemetry needs a trusted peer key";
            return false;
        }
        recipientKey = &recipient->publicKey;
    }
    if (!decoder_.encodeRequest(port, channelIndex, nodeId, to, esp_random(),
                                hopLimit, recipientKey, packet)) {
        transmitStatus_ = "Unable to create request";
        return false;
    }
    const char* label = port == 3 ? "Position requested"
                        : port == 67 ? "Telemetry requested"
                                     : "Identity requested";
    return transmitPacket(packet, label);
}

bool LoRaService::sendPosition(double latitude, double longitude,
                               int32_t altitude, size_t channelIndex,
                               uint32_t nodeId, uint8_t hopLimit) {
    std::vector<uint8_t> packet;
    if (!decoder_.encodePosition(latitude, longitude, altitude, channelIndex,
                                 nodeId, esp_random(), hopLimit, packet)) {
        transmitStatus_ = "Unable to encode position";
        return false;
    }
    return transmitPacket(packet, "Position shared");
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
