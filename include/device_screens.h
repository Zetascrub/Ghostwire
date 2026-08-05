#pragma once

#include <vector>

#include "biscuit_pro_client.h"
#include "chameleon_ultra_client.h"
#include "sd_logger.h"

// Connected-device screens (Devices menu): Biscuit Pro's status/tools/
// result/wardrive screens, and the Chameleon Ultra status/emulate-confirm
// screens. Two unrelated subsystems grouped in one file pair purely to keep
// file count down -- see docs/screen-extraction.md. Each takes references
// to only its own subsystem's state; there's no cross-talk between them.
class BiscuitScreens {
public:
    BiscuitScreens(BiscuitProClient& client, size_t& listSelection,
                   size_t& listOffset, bool& wardriveActive,
                   uint32_t& wardriveApCount, uint32_t& wardriveBleCount,
                   String& resultTitle, std::vector<String>& resultLines,
                   size_t& resultOffset)
        : client_(client),
          listSelection_(listSelection),
          listOffset_(listOffset),
          wardriveActive_(wardriveActive),
          wardriveApCount_(wardriveApCount),
          wardriveBleCount_(wardriveBleCount),
          resultTitle_(resultTitle),
          resultLines_(resultLines),
          resultOffset_(resultOffset) {}

    void drawMain();
    void drawTools();
    void drawWardrive(bool fullDraw = true);
    void drawResult();

private:
    BiscuitProClient& client_;
    size_t& listSelection_;
    size_t& listOffset_;
    bool& wardriveActive_;
    uint32_t& wardriveApCount_;
    uint32_t& wardriveBleCount_;
    String& resultTitle_;
    std::vector<String>& resultLines_;
    size_t& resultOffset_;
    uint32_t lastWardriveSignature_ = UINT32_MAX;
};

class ChameleonScreen {
public:
    ChameleonScreen(ChameleonUltraClient& client, bool& hasReadings,
                    uint8_t& appMajor, uint8_t& appMinor, uint16_t& batteryMv,
                    uint8_t& batteryPct, bool& scanAttempted, bool& hfFound,
                    ChameleonUltraClient::HfTag& hfTag, bool& lfFound,
                    uint8_t (&lfId)[5], String& workflowStatus,
                    bool& continuousScan, SdLogger& logger)
        : client_(client),
          hasReadings_(hasReadings),
          appMajor_(appMajor),
          appMinor_(appMinor),
          batteryMv_(batteryMv),
          batteryPct_(batteryPct),
          scanAttempted_(scanAttempted),
          hfFound_(hfFound),
          hfTag_(hfTag),
          lfFound_(lfFound),
          lfId_(lfId),
          workflowStatus_(workflowStatus),
          continuousScan_(continuousScan),
          logger_(logger) {}

    void draw(bool fullDraw = true);
    void drawEmulateConfirm();

    // Single source of truth for the colon-separated hex ID rendering, also
    // used directly by main.cpp's save/scan/log code.
    static String hexId(const uint8_t* data, size_t len);

private:
    ChameleonUltraClient& client_;
    bool& hasReadings_;
    uint8_t& appMajor_;
    uint8_t& appMinor_;
    uint16_t& batteryMv_;
    uint8_t& batteryPct_;
    bool& scanAttempted_;
    bool& hfFound_;
    ChameleonUltraClient::HfTag& hfTag_;
    bool& lfFound_;
    uint8_t (&lfId_)[5];
    String& workflowStatus_;
    bool& continuousScan_;
    SdLogger& logger_;
};
