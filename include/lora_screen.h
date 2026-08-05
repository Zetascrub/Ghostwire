#pragma once

#include "lora_service.h"
#include "gnss_service.h"
#include "sd_logger.h"

// LoRa Receive screen: radio status, packet/RSSI/SNR readout, and decoded
// Meshtastic summary when available. Draw-only extraction (see
// docs/screen-extraction.md and include/gnss_screen.h for the pattern this
// follows) -- input handling (log toggle, profile toggle, restart) and the
// packet-driven CSV log rows stay in main.cpp.
class LoRaScreen {
public:
    LoRaScreen(LoRaService& service, SdLogger& logger, GnssService& gnss)
        : service_(service), logger_(logger), gnss_(gnss) {}

    void draw(bool fullDraw = true);
    void drawNodes(size_t selection, size_t offset);
    void drawNodeDetail(size_t selection);
    void drawMessages(size_t selection, size_t offset);
    void drawMessageDetail(size_t selection);
    void drawRadar();
    void drawChannels(size_t selection, size_t offset,
                      const String& configurationStatus, uint8_t hopLimit);
    void drawCompose(const String& draft, size_t channelIndex,
                     uint8_t hopLimit, const String& nodeName,
                     const String& status);
    void drawSettings(size_t selection, size_t offset, const String& longName,
                      const String& shortName, size_t channelIndex,
                      uint8_t hopLimit, bool editing, const String& status);

private:
    LoRaService& service_;
    SdLogger& logger_;
    GnssService& gnss_;
    // Skips redundant partial redraws when nothing shown on screen has
    // changed since the last tick (was a function-static in main.cpp;
    // equivalent here since exactly one LoRaScreen instance exists).
    uint32_t lastSignature_ = UINT32_MAX;
};
