#pragma once

#include <Arduino.h>
#include <RadioLib.h>
#include <M5Unified.h>
#include <utility/PI4IOE5V6408_Class.hpp>
#include <vector>

#include "meshtastic_decoder.h"

class LoRaService {
public:
    enum class Profile {
        MeshtasticEuLongFast,
        M5StackGeneric,
    };

    bool begin();
    void update();
    bool restartReceive();
    bool toggleProfile();

    bool isReady() const { return ready_; }
    int status() const { return status_; }
    uint32_t packetCount() const { return packetCount_; }
    float lastRssi() const { return lastRssi_; }
    float lastSnr() const { return lastSnr_; }
    const String& lastPacket() const { return lastPacket_; }
    const MeshtasticDecoded& lastDecoded() const { return lastDecoded_; }
    Profile profile() const { return profile_; }
    const char* profileName() const;
    float frequencyMhz() const;

private:
    Module module_{5, 4, 3, 6};
    SX1262 radio_{&module_};
    m5::PI4IOE5V6408_Class ioe_{0x43, 400000, &m5::In_I2C};
    bool ready_ = false;
    int status_ = RADIOLIB_ERR_UNKNOWN;
    uint32_t packetCount_ = 0;
    float lastRssi_ = 0.0F;
    float lastSnr_ = 0.0F;
    String lastPacket_;
    std::vector<uint8_t> lastRawPacket_;
    MeshtasticDecoded lastDecoded_;
    MeshtasticDecoder decoder_;
    Profile profile_ = Profile::MeshtasticEuLongFast;
};
