#include "cyber_familiar.h"

#include <algorithm>
#include <cstring>

namespace {
const char* const kNames[] = {
    "Byte", "Glitch", "Nyx", "Wyrm", "Hex", "Pixel", "Mimic", "Gizmo",
};
constexpr size_t kNameCount = sizeof(kNames) / sizeof(kNames[0]);
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
    xp_ += amount;
    mood_ = mood;
    lastMoodMs_ = millis();
    addJournal(message);
    if (level() > oldLevel) {
        mood_ = FamiliarMood::Proud;
        addJournal("Level up! Now level " + String(level()) + ".");
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
        award(8, FamiliarMood::Excited, "Heard a voice from the mesh.");
    }
    if (activity.gnssFix && !previous_.gnssFix) {
        award(12, FamiliarMood::Proud, "Found our place in the world.");
    }
    if (activity.wifiConnected && !previous_.wifiConnected) {
        award(5, FamiliarMood::Content, "The uplink is alive.");
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
    xp_ += 2;
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
    xp_ += 2;
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
    award(15, FamiliarMood::Curious,
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
    award(3, FamiliarMood::Content,
          bond_ % 5 == 0 ? "We're becoming a proper team."
                         : "Happy electronic chirping.");
}

void CyberFamiliar::cycleName() {
    size_t selected = 0;
    for (size_t index = 0; index < kNameCount; ++index) {
        if (name_ == kNames[index]) selected = index;
    }
    name_ = kNames[(selected + 1) % kNameCount];
    award(1, FamiliarMood::Curious, "New designation: " + name_ + ".");
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

void CyberFamiliar::notePatrol(const String& message, uint16_t xp,
                               FamiliarMood mood) {
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

const char* CyberFamiliar::evolutionName() const {
    if (level() < 3) return "Script Sprite";
    if (loraDiscoveries_ > wifiDiscoveries_ / 2) return "Signal Wyrm";
    if (bleDiscoveries_ > wifiDiscoveries_) return "Beacon Gremlin";
    if (toolCount() >= 8) return "Grid Imp";
    if (bond_ >= 25) return "Hex Familiar";
    return "Packet Gremlin";
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
