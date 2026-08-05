#include "lora_service.h"

#include <SPI.h>
#include <algorithm>

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
            decoder_.decodePublic(packet.data(), packet.size(), lastDecoded_);
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
    if (lastDecoded_.hasPosition) {
        node->hasPosition = true;
        node->latitude = lastDecoded_.latitude;
        node->longitude = lastDecoded_.longitude;
        node->altitude = lastDecoded_.altitude;
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
                             lastDecoded_.id, millis(), lastDecoded_.summary});
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

bool LoRaService::restartReceive() {
    if (!ready_) return begin();
    packetReceived = false;
    status_ = radio_.startReceive();
    ready_ = status_ == RADIOLIB_ERR_NONE;
    return ready_;
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
