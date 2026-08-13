#include "cyber_familiar.h"

#include <algorithm>
#include <cstring>

namespace {
const char* const kNames[] = {
    "Byte", "Glitch", "Nyx", "Wyrm", "Hex", "Pixel", "Mimic", "Gizmo",
};
constexpr size_t kNameCount = sizeof(kNames) / sizeof(kNames[0]);

struct FamiliarStage {
    const char* name;
    uint32_t xpThreshold;
};

// Thresholds spread across the existing level curve (xp = (level-1)*100,
// capped at level 99 / 9800 xp) rather than a new curve of their own.
// Reuses 4 of the 5 names the old behavior-mix evolutionName() used
// ("Beacon Gremlin" dropped in favor of "Beacon Warden", a senior
// evolution of the same idea) for continuity with what already shipped.
constexpr FamiliarStage kStages[] = {
    {"Script Sprite", 0},
    {"Packet Gremlin", 400},
    {"Grid Imp", 1100},
    {"Signal Wyrm", 2400},
    {"Beacon Warden", 4400},
    {"Hex Familiar", 6900},
};
constexpr uint8_t kStageCount = sizeof(kStages) / sizeof(kStages[0]);

uint8_t stageIndexForXp(uint32_t xp) {
    uint8_t index = 0;
    for (uint8_t i = 0; i < kStageCount; ++i) {
        if (xp >= kStages[i].xpThreshold) index = i;
    }
    return index;
}

// Single source of truth for every reward this class hands out, whether
// triggered internally (interact(), observeTool(), update()'s telemetry
// reactions) or externally via notePatrol() -- see the enum's own comment
// (cyber_familiar.h) for what "reconciled, not rebalanced" means here.
uint16_t xpForEvent(FamiliarXpEvent event) {
    switch (event) {
        case FamiliarXpEvent::Interact: return 3;
        case FamiliarXpEvent::CycleName: return 1;
        case FamiliarXpEvent::ToolUnlocked: return 15;
        case FamiliarXpEvent::IdentitySeen: return 2;
        case FamiliarXpEvent::LoraPacketHeard: return 8;
        case FamiliarXpEvent::GnssFixFound: return 12;
        case FamiliarXpEvent::WifiUplinkConnected: return 5;
        case FamiliarXpEvent::GuardianWatchStarted: return 5;
        case FamiliarXpEvent::GuardianWatchEnded: return 3;
        case FamiliarXpEvent::GuardianDisconnectAnomaly: return 8;
        case FamiliarXpEvent::EvilPortalLive: return 5;
        case FamiliarXpEvent::EvilPortalStoppedQuiet: return 3;
        case FamiliarXpEvent::EvilPortalStoppedWithCaptures: return 15;
        case FamiliarXpEvent::EvilPortalLoginCaptured: return 10;
        case FamiliarXpEvent::PatrolStarted: return 10;
        case FamiliarXpEvent::PatrolStoppedByOperator: return 0;
        // Normalized: previously 0, 3, or 5 XP depending on which of
        // three separate "nothing new" call sites hit it.
        case FamiliarXpEvent::PatrolQuiet: return 3;
        case FamiliarXpEvent::WatchPassFound: return 10;
        case FamiliarXpEvent::PatrolCompleteFound: return 30;
        case FamiliarXpEvent::PatrolNeedsAttention: return 0;
        case FamiliarXpEvent::HandshakeMissionQuiet: return 5;
        case FamiliarXpEvent::HandshakeMissionFound: return 20;
        case FamiliarXpEvent::BattleWon: return 40;
        case FamiliarXpEvent::BattleLost: return 10;
        case FamiliarXpEvent::BattleFled: return 0;
    }
    return 0;
}
}

void CyberFamiliar::begin(Preferences& preferences) {
    preferences_ = &preferences;
    name_ = preferences.getString("fam_name", "Byte");
    xp_ = preferences.getUInt("fam_xp", 0);
    bond_ = preferences.getUShort("fam_bond", 0);
    wifiDiscoveries_ = preferences.getUInt("fam_wifi", 0);
    bleDiscoveries_ = preferences.getUInt("fam_ble", 0);
    loraDiscoveries_ = preferences.getUInt("fam_lora", 0);
    toolMask_ = preferences.getUInt("fam_tools", 0);
    ageAtBootSeconds_ = preferences.getUInt("fam_age", 0);
    idleMode_ = preferences.getBool("fam_idle", false);
    const size_t wifiBytes = preferences.getBytesLength("fam_wids");
    if (wifiBytes > 0) {
        const size_t loaded = preferences.getBytes(
            "fam_wids", wifiIdentityHashes_, sizeof(wifiIdentityHashes_));
        wifiIdentityCount_ = std::min<size_t>(kIdentityCapacity,
                                              loaded / sizeof(uint32_t));
        wifiIdentityCursor_ = wifiIdentityCount_ % kIdentityCapacity;
    }
    const size_t bleBytes = preferences.getBytesLength("fam_bids");
    if (bleBytes > 0) {
        const size_t loaded = preferences.getBytes(
            "fam_bids", bleIdentityHashes_, sizeof(bleIdentityHashes_));
        bleIdentityCount_ = std::min<size_t>(kIdentityCapacity,
                                             loaded / sizeof(uint32_t));
        bleIdentityCursor_ = bleIdentityCount_ % kIdentityCapacity;
    }
    bootStartedMs_ = millis();
    lastMoodMs_ = millis();
    addJournal("Woke up and checked the deck.");
}

void CyberFamiliar::save() {
    if (preferences_ == nullptr) return;
    preferences_->putString("fam_name", name_);
    preferences_->putUInt("fam_xp", xp_);
    preferences_->putUShort("fam_bond", bond_);
    preferences_->putUInt("fam_wifi", wifiDiscoveries_);
    preferences_->putUInt("fam_ble", bleDiscoveries_);
    preferences_->putUInt("fam_lora", loraDiscoveries_);
    preferences_->putUInt("fam_tools", toolMask_);
    preferences_->putBytes("fam_wids", wifiIdentityHashes_,
                           static_cast<size_t>(wifiIdentityCount_) *
                               sizeof(uint32_t));
    preferences_->putBytes("fam_bids", bleIdentityHashes_,
                           static_cast<size_t>(bleIdentityCount_) *
                               sizeof(uint32_t));
    preferences_->putUInt("fam_age", ageSeconds());
    preferences_->putBool("fam_idle", idleMode_);
    ageAtBootSeconds_ = ageSeconds();
    bootStartedMs_ = millis();
}

void CyberFamiliar::addJournal(const String& message) {
    lastMessage_ = message;
    journal_.push_back(message);
    while (journal_.size() > 6) journal_.erase(journal_.begin());
}

void CyberFamiliar::award(uint16_t amount, FamiliarMood mood,
                          const String& message) {
    const uint16_t oldLevel = level();
    const uint8_t oldStage = stageIndex();
    xp_ += amount;
    mood_ = mood;
    lastMoodMs_ = millis();
    addJournal(message);
    if (level() > oldLevel) {
        mood_ = FamiliarMood::Proud;
        addJournal("Level up! Now level " + String(level()) + ".");
    }
    // Checked independently of the level-up branch above (not else-if) --
    // a stage boundary is a rarer, bigger deal than a level-up, and both
    // can legitimately land in the same award() call, so both journal
    // entries should show up rather than the stage one being swallowed.
    if (stageIndex() > oldStage) {
        mood_ = FamiliarMood::Proud;
        addJournal("Evolved into " + String(stageName()) + "!");
    }
    save();
}

void CyberFamiliar::update(const FamiliarActivity& activity) {
    if (!snapshotReady_) {
        previous_ = activity;
        snapshotReady_ = true;
        return;
    }
    if (activity.loraPackets > previous_.loraPackets) {
        const uint32_t gained = activity.loraPackets - previous_.loraPackets;
        loraDiscoveries_ += gained;
        award(xpForEvent(FamiliarXpEvent::LoraPacketHeard), FamiliarMood::Excited,
              "Heard a voice from the mesh.");
    }
    if (activity.gnssFix && !previous_.gnssFix) {
        award(xpForEvent(FamiliarXpEvent::GnssFixFound), FamiliarMood::Proud,
              "Found our place in the world.");
    }
    if (activity.wifiConnected && !previous_.wifiConnected) {
        award(xpForEvent(FamiliarXpEvent::WifiUplinkConnected), FamiliarMood::Content,
              "The uplink is alive.");
    }
    if (activity.batteryPercent <= 15 && previous_.batteryPercent > 15) {
        mood_ = FamiliarMood::Worried;
        lastMoodMs_ = millis();
        addJournal("Power is low. Snacks, please.");
    }
    previous_ = activity;
    if (millis() - lastMoodMs_ > 120000) {
        mood_ = activity.batteryPercent < 25 ? FamiliarMood::Sleepy
                                             : FamiliarMood::Content;
    }
    if (identityDirty_ && millis() - lastIdentitySaveMs_ >= 5000) {
        save();
        identityDirty_ = false;
        lastIdentitySaveMs_ = millis();
    }
}

bool CyberFamiliar::rememberIdentity(uint32_t hash, uint32_t* hashes,
                                     uint8_t& count, uint8_t& cursor,
                                     const char* preferenceKey) {
    (void)preferenceKey;
    for (uint8_t index = 0; index < count; ++index) {
        if (hashes[index] == hash) return false;
    }
    hashes[cursor] = hash;
    cursor = (cursor + 1) % kIdentityCapacity;
    if (count < kIdentityCapacity) ++count;
    return true;
}

bool CyberFamiliar::observeWifiIdentity(const uint8_t mac[6]) {
    uint32_t hash = 2166136261UL;
    for (size_t index = 0; index < 6; ++index) {
        hash ^= mac[index];
        hash *= 16777619UL;
    }
    if (!rememberIdentity(hash, wifiIdentityHashes_, wifiIdentityCount_,
                          wifiIdentityCursor_, "fam_wids")) {
        return false;
    }
    ++wifiDiscoveries_;
    // Not routed through award(): these fire far more often than any other
    // event here (live scan traffic), and award() unconditionally saves --
    // the batched identityDirty_/lastIdentitySaveMs_ flush in update() is
    // what keeps that off the NVS write-wear budget. Level/stage-up
    // detection is a known gap on this path as a result (pre-existing, not
    // something this reconciliation changes).
    xp_ += xpForEvent(FamiliarXpEvent::IdentitySeen);
    mood_ = FamiliarMood::Curious;
    lastMoodMs_ = millis();
    addJournal("Met a new Wi-Fi signal.");
    identityDirty_ = true;
    return true;
}

bool CyberFamiliar::observeBleIdentity(const String& address) {
    uint32_t hash = 2166136261UL;
    for (size_t index = 0; index < address.length(); ++index) {
        char value = address[index];
        if (value >= 'a' && value <= 'f') value -= 32;
        hash ^= static_cast<uint8_t>(value);
        hash *= 16777619UL;
    }
    if (!rememberIdentity(hash, bleIdentityHashes_, bleIdentityCount_,
                          bleIdentityCursor_, "fam_bids")) {
        return false;
    }
    ++bleDiscoveries_;
    // See the matching comment in observeWifiIdentity() for why this
    // bypasses award().
    xp_ += xpForEvent(FamiliarXpEvent::IdentitySeen);
    mood_ = FamiliarMood::Excited;
    lastMoodMs_ = millis();
    addJournal("Met a new BLE beacon.");
    identityDirty_ = true;
    return true;
}

void CyberFamiliar::observeTool(uint8_t toolId, const char* label) {
    if (toolId >= 32) return;
    const uint32_t bit = 1UL << toolId;
    if ((toolMask_ & bit) != 0) return;
    toolMask_ |= bit;
    award(xpForEvent(FamiliarXpEvent::ToolUnlocked), FamiliarMood::Curious,
          "Learned the " + String(label) + " tool.");
}

void CyberFamiliar::interact() {
    if (millis() - lastInteractionMs_ < 1500) {
        addJournal("Easy! I only have so many pixels.");
        mood_ = FamiliarMood::Dizzy;
        return;
    }
    lastInteractionMs_ = millis();
    if (bond_ < 999) ++bond_;
    award(xpForEvent(FamiliarXpEvent::Interact), FamiliarMood::Content,
          bond_ % 5 == 0 ? "We're becoming a proper team."
                         : "Happy electronic chirping.");
}

void CyberFamiliar::cycleName() {
    size_t selected = 0;
    for (size_t index = 0; index < kNameCount; ++index) {
        if (name_ == kNames[index]) selected = index;
    }
    name_ = kNames[(selected + 1) % kNameCount];
    award(xpForEvent(FamiliarXpEvent::CycleName), FamiliarMood::Curious,
          "New designation: " + name_ + ".");
}

void CyberFamiliar::toggleIdleMode() {
    idleMode_ = !idleMode_;
    addJournal(idleMode_ ? "I'll watch the deck while it sleeps."
                         : "Idle watch disabled.");
    save();
}

void CyberFamiliar::noteRecovery() {
    mood_ = FamiliarMood::Dizzy;
    addJournal("That reboot felt strange.");
}

void CyberFamiliar::notePatrol(const String& message, FamiliarXpEvent event,
                               FamiliarMood mood) {
    const uint16_t xp = xpForEvent(event);
    if (xp > 0) {
        award(xp, mood, message);
        return;
    }
    mood_ = mood;
    lastMoodMs_ = millis();
    addJournal(message);
}

void CyberFamiliar::resetProgress() {
    xp_ = 0;
    bond_ = 0;
    wifiDiscoveries_ = 0;
    bleDiscoveries_ = 0;
    loraDiscoveries_ = 0;
    toolMask_ = 0;
    ageAtBootSeconds_ = 0;
    bootStartedMs_ = millis();
    wifiIdentityCount_ = 0;
    bleIdentityCount_ = 0;
    wifiIdentityCursor_ = 0;
    bleIdentityCursor_ = 0;
    memset(wifiIdentityHashes_, 0, sizeof(wifiIdentityHashes_));
    memset(bleIdentityHashes_, 0, sizeof(bleIdentityHashes_));
    journal_.clear();
    mood_ = FamiliarMood::Content;
    addJournal("A fresh adventure begins.");
    if (preferences_ != nullptr) {
        preferences_->remove("fam_wids");
        preferences_->remove("fam_bids");
    }
    save();
}

const char* CyberFamiliar::moodName() const {
    switch (mood_) {
        case FamiliarMood::Curious: return "CURIOUS";
        case FamiliarMood::Excited: return "EXCITED";
        case FamiliarMood::Sleepy: return "SLEEPY";
        case FamiliarMood::Proud: return "PROUD";
        case FamiliarMood::Worried: return "WORRIED";
        case FamiliarMood::Dizzy: return "DIZZY";
        default: return "CONTENT";
    }
}

const char* CyberFamiliar::stageName() const {
    return kStages[stageIndex()].name;
}

uint8_t CyberFamiliar::stageIndex() const {
    return stageIndexForXp(xp_);
}

bool CyberFamiliar::isMaxStage() const {
    return stageIndex() == kStageCount - 1;
}

uint8_t CyberFamiliar::stageProgressPercent() const {
    const uint8_t index = stageIndex();
    if (index == kStageCount - 1) return 100;
    const uint32_t stageStart = kStages[index].xpThreshold;
    const uint32_t nextStart = kStages[index + 1].xpThreshold;
    const uint32_t span = nextStart - stageStart;
    if (span == 0) return 100;
    const uint32_t progress = xp_ - stageStart;
    return static_cast<uint8_t>(std::min<uint32_t>(100, progress * 100 / span));
}

uint32_t CyberFamiliar::xpToNextStage() const {
    const uint8_t index = stageIndex();
    if (index == kStageCount - 1) return 0;
    const uint32_t nextStart = kStages[index + 1].xpThreshold;
    return nextStart > xp_ ? nextStart - xp_ : 0;
}

uint8_t CyberFamiliar::stageCount() { return kStageCount; }

const char* CyberFamiliar::stageNameAt(uint8_t index) {
    return kStages[std::min<uint8_t>(index, kStageCount - 1)].name;
}

uint32_t CyberFamiliar::stageXpThresholdAt(uint8_t index) {
    return kStages[std::min<uint8_t>(index, kStageCount - 1)].xpThreshold;
}

uint16_t CyberFamiliar::level() const {
    return static_cast<uint16_t>(std::min<uint32_t>(99, 1 + xp_ / 100));
}

uint32_t CyberFamiliar::xpForNextLevel() const {
    return static_cast<uint32_t>(level()) * 100;
}

uint32_t CyberFamiliar::toolCount() const {
    uint32_t value = toolMask_;
    uint32_t count = 0;
    while (value != 0) {
        count += value & 1U;
        value >>= 1U;
    }
    return count;
}

uint32_t CyberFamiliar::ageSeconds() const {
    return ageAtBootSeconds_ + (millis() - bootStartedMs_) / 1000;
}
