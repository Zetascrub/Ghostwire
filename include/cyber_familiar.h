#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

enum class FamiliarMood : uint8_t {
    Content,
    Curious,
    Excited,
    Sleepy,
    Proud,
    Worried,
    Dizzy,
};

struct FamiliarActivity {
    uint32_t wifiSeen = 0;
    uint32_t bleSeen = 0;
    uint32_t loraPackets = 0;
    bool gnssFix = false;
    bool wifiConnected = false;
    uint8_t batteryPercent = 100;
};

// One entry per distinct XP-granting event, so every reward the class
// hands out -- whether triggered internally (interact(), observeTool(),
// the telemetry reactions in update()) or externally via notePatrol() --
// draws from the single xpForEvent() table in cyber_familiar.cpp instead
// of a numeric literal at each call site. Values there are a
// reconciliation of what was already being awarded, not a rebalance,
// except "nothing new found" patrol results (previously 0/3/5 XP
// depending on which of three call sites hit it) normalized to one value.
enum class FamiliarXpEvent : uint8_t {
    Interact,
    CycleName,
    ToolUnlocked,
    IdentitySeen,
    LoraPacketHeard,
    GnssFixFound,
    WifiUplinkConnected,
    GuardianWatchStarted,
    GuardianWatchEnded,
    GuardianDisconnectAnomaly,
    EvilPortalLive,
    EvilPortalStoppedQuiet,
    EvilPortalStoppedWithCaptures,
    EvilPortalLoginCaptured,
    PatrolStarted,
    PatrolStoppedByOperator,
    PatrolQuiet,
    WatchPassFound,
    PatrolCompleteFound,
    PatrolNeedsAttention,
    HandshakeMissionQuiet,
    HandshakeMissionFound,
    BattleWon,
    BattleLost,
    BattleFled,
};

class CyberFamiliar {
public:
    void begin(Preferences& preferences);
    void update(const FamiliarActivity& activity);
    void observeTool(uint8_t toolId, const char* label);
    bool observeWifiIdentity(const uint8_t mac[6]);
    bool observeBleIdentity(const String& address);
    void interact();
    void cycleName();
    void toggleIdleMode();
    void noteRecovery();
    void notePatrol(const String& message, FamiliarXpEvent event,
                    FamiliarMood mood = FamiliarMood::Curious);
    void resetProgress();

    const String& name() const { return name_; }
    const String& lastMessage() const { return lastMessage_; }
    const std::vector<String>& journal() const { return journal_; }
    FamiliarMood mood() const { return mood_; }
    const char* moodName() const;
    // Strictly XP-gated evolution tier (Script Sprite -> ... -> Hex
    // Familiar) -- not to be confused with level() below, which is a much
    // finer-grained (1-99) trickle stat that keeps ticking up within a
    // stage. A stage change is the rare, big-deal moment; a level-up is
    // the frequent, small one -- both show up in the journal, distinctly.
    const char* stageName() const;
    uint8_t stageIndex() const;
    bool isMaxStage() const;
    // 0-100, progress within the *current* stage toward the next one (100
    // once isMaxStage()). Mirrors what the Evolution details screen shows.
    uint8_t stageProgressPercent() const;
    // XP still needed to reach the next stage (0 once isMaxStage()).
    uint32_t xpToNextStage() const;
    // Full ladder, for the Evolution details screen's stage list -- backed
    // by the same kStages table stageName()/stageIndex() use internally.
    static uint8_t stageCount();
    static const char* stageNameAt(uint8_t index);
    static uint32_t stageXpThresholdAt(uint8_t index);
    uint16_t level() const;
    uint32_t xp() const { return xp_; }
    uint32_t xpForNextLevel() const;
    uint16_t bond() const { return bond_; }
    uint32_t discoveries() const { return wifiDiscoveries_ + bleDiscoveries_; }
    uint32_t wifiDiscoveries() const { return wifiDiscoveries_; }
    uint32_t bleDiscoveries() const { return bleDiscoveries_; }
    uint32_t toolCount() const;
    uint32_t ageSeconds() const;
    bool idleMode() const { return idleMode_; }

private:
    void award(uint16_t amount, FamiliarMood mood, const String& message);
    void addJournal(const String& message);
    void save();
    bool rememberIdentity(uint32_t hash, uint32_t* hashes, uint8_t& count,
                          uint8_t& cursor, const char* preferenceKey);

    Preferences* preferences_ = nullptr;
    String name_ = "Byte";
    String lastMessage_ = "Ready to explore.";
    std::vector<String> journal_;
    FamiliarMood mood_ = FamiliarMood::Content;
    uint32_t xp_ = 0;
    uint16_t bond_ = 0;
    uint32_t wifiDiscoveries_ = 0;
    uint32_t bleDiscoveries_ = 0;
    uint32_t loraDiscoveries_ = 0;
    uint32_t toolMask_ = 0;
    uint32_t ageAtBootSeconds_ = 0;
    uint32_t bootStartedMs_ = 0;
    uint32_t lastInteractionMs_ = 0;
    uint32_t lastMoodMs_ = 0;
    bool idleMode_ = false;
    bool snapshotReady_ = false;
    bool identityDirty_ = false;
    uint32_t lastIdentitySaveMs_ = 0;
    FamiliarActivity previous_{};
    static constexpr uint8_t kIdentityCapacity = 48;
    uint32_t wifiIdentityHashes_[kIdentityCapacity]{};
    uint32_t bleIdentityHashes_[kIdentityCapacity]{};
    uint8_t wifiIdentityCount_ = 0;
    uint8_t bleIdentityCount_ = 0;
    uint8_t wifiIdentityCursor_ = 0;
    uint8_t bleIdentityCursor_ = 0;
};
