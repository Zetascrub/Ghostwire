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
    }
    restartReceive();
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
