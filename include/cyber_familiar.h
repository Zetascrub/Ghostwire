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
    void resetProgress();

    const String& name() const { return name_; }
    const String& lastMessage() const { return lastMessage_; }
    const std::vector<String>& journal() const { return journal_; }
    FamiliarMood mood() const { return mood_; }
    const char* moodName() const;
    const char* evolutionName() const;
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
