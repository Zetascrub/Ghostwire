#include "familiar_battle_service.h"

#include <NimBLEAdvertisedDevice.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <WiFi.h>
#include <algorithm>
#include <cstring>

namespace {
// Same Nordic UART Service UUID trio chameleon_ultra_client.cpp already
// reuses here ("a de facto industry-wide UUID scheme... safe to reuse
// directly") -- one write characteristic (challenger -> host) and one
// notify characteristic (host -> challenger).
const NimBLEUUID kServiceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
const NimBLEUUID kWriteCharUuid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
const NimBLEUUID kNotifyCharUuid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

// Manufacturer data company ID 0xFFFF (reserved for testing, per the
// Bluetooth SIG) -- same category ble_spam_service.cpp already draws
// ad-hoc IDs from for its own synthetic advertisements.
constexpr uint8_t kCompanyIdLow = 0xFF;
constexpr uint8_t kCompanyIdHigh = 0xFF;
constexpr uint8_t kProtocolVersion = 1;
constexpr uint32_t kScanDurationMs = 4000;
constexpr uint32_t kConnectTimeoutMs = 6000;

constexpr uint8_t kMsgHello = 0x01;
constexpr uint8_t kMsgMove = 0x02;

FamiliarBattleService* g_owner = nullptr;

class BattleServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
        if (g_owner != nullptr) g_owner->onPeerConnected();
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        if (g_owner != nullptr) g_owner->onPeerDisconnected();
    }
};
BattleServerCallbacks battleServerCallbacks;

class BattleWriteCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        if (g_owner == nullptr) return;
        const NimBLEAttValue value = characteristic->getValue();
        g_owner->onRawInbound(value.data(), value.size());
    }
};
BattleWriteCallbacks battleWriteCallbacks;
}  // namespace

uint16_t FamiliarBattleService::deriveMaxHp(uint8_t level) {
    return static_cast<uint16_t>(20 + static_cast<uint16_t>(level) * 3);
}

uint8_t FamiliarBattleService::deriveAttack(uint8_t level, uint8_t stageIndex) {
    return static_cast<uint8_t>(std::min<uint16_t>(
        255, 4 + level + static_cast<uint16_t>(stageIndex) * 2));
}

uint8_t FamiliarBattleService::deriveDefense(uint8_t level, uint8_t stageIndex) {
    return static_cast<uint8_t>(
        std::min<uint16_t>(255, 2 + level / 2 + stageIndex));
}

void FamiliarBattleService::beginRadio() {
    // Both radios share the ESP32-S3 radio resources -- same handoff every
    // other BLE feature in this codebase uses before touching BLE.
    WiFi.scanDelete();
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    delay(150);
    if (!initialized_) {
        NimBLEDevice::init("Ghostwire VPet");
        initialized_ = true;
    }
    g_owner = this;
}

void FamiliarBattleService::teardownRadio() {
    if (initialized_) {
        NimBLEDevice::deinit(true);
        initialized_ = false;
    }
    if (g_owner == this) g_owner = nullptr;
}

void FamiliarBattleService::resetForNewBattle() {
    opponent_ = FamiliarBattleOpponent{};
    opponentAttack_ = 0;
    opponentDefense_ = 0;
    myHp_ = myMaxHp_ = 0;
    opponentHp_ = opponentMaxHp_ = 0;
    turnNumber_ = 0;
    myMoveSubmitted_ = opponentMoveSubmitted_ = false;
    helloSent_ = helloReceived_ = false;
    prngState_ = 1;
    outcome_ = FamiliarBattleOutcome::None;
    log_.clear();
    server_ = nullptr;
    writeChar_ = nullptr;
    notifyChar_ = nullptr;
    client_ = nullptr;
    remoteWriteChar_ = nullptr;
    inboundHead_ = 0;
    inboundTail_ = 0;
    connectPending_ = false;
    disconnectPending_ = false;
}

bool FamiliarBattleService::beginHost(uint32_t playerId, uint8_t stageIndex,
                                      uint8_t level) {
    end();
    resetForNewBattle();
    isHost_ = true;
    myPlayerId_ = playerId;
    myStageIndex_ = stageIndex;
    myLevel_ = level;
    myAttack_ = deriveAttack(level, stageIndex);
    myDefense_ = deriveDefense(level, stageIndex);
    myMaxHp_ = deriveMaxHp(level);
    myHp_ = myMaxHp_;

    beginRadio();
    server_ = NimBLEDevice::createServer();
    if (server_ == nullptr) {
        status_ = "BLE server unavailable";
        teardownRadio();
        state_ = FamiliarBattleState::Idle;
        return false;
    }
    // NimBLEServer::setCallbacks() defaults to owning (and later deleting)
    // the callbacks pointer -- battleServerCallbacks is a static object,
    // not heap-allocated, so that would crash on teardown. Pass false.
    server_->setCallbacks(&battleServerCallbacks, false);
    NimBLEService* service = server_->createService(kServiceUuid);
    writeChar_ = service->createCharacteristic(kWriteCharUuid,
                                               NIMBLE_PROPERTY::WRITE);
    notifyChar_ = service->createCharacteristic(kNotifyCharUuid,
                                                NIMBLE_PROPERTY::NOTIFY);
    if (writeChar_ == nullptr || notifyChar_ == nullptr) {
        status_ = "BLE characteristic setup failed";
        teardownRadio();
        state_ = FamiliarBattleState::Idle;
        return false;
    }
    writeChar_->setCallbacks(&battleWriteCallbacks);
    // NimBLEService::start() (not called here) is a deprecated no-op in
    // this NimBLE-Arduino version -- services start automatically with the
    // *server's* start() below, per its own deprecation note.
    if (!server_->start()) {
        status_ = "BLE server failed to start";
        teardownRadio();
        state_ = FamiliarBattleState::Idle;
        return false;
    }

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (advertising == nullptr) {
        status_ = "Advertising unavailable";
        teardownRadio();
        state_ = FamiliarBattleState::Idle;
        return false;
    }
    NimBLEAdvertisementData advData;
    uint8_t payload[12];
    payload[0] = kCompanyIdLow;
    payload[1] = kCompanyIdHigh;
    payload[2] = 'V';
    payload[3] = 'P';
    payload[4] = kProtocolVersion;
    payload[5] = static_cast<uint8_t>(playerId >> 24);
    payload[6] = static_cast<uint8_t>(playerId >> 16);
    payload[7] = static_cast<uint8_t>(playerId >> 8);
    payload[8] = static_cast<uint8_t>(playerId);
    payload[9] = stageIndex;
    payload[10] = level;
    payload[11] = 1;  // battle enabled
    advData.setManufacturerData(payload, sizeof(payload));
    advertising->setAdvertisementData(advData);
    advertising->addServiceUUID(kServiceUuid);
    if (!advertising->start()) {
        status_ = "Advertising failed";
        teardownRadio();
        state_ = FamiliarBattleState::Idle;
        return false;
    }

    state_ = FamiliarBattleState::Hosting;
    status_ = "Waiting for a challenger...";
    return true;
}

void FamiliarBattleService::parseAdvertisement(
    const NimBLEAdvertisedDevice* advertised) {
    if (advertised == nullptr) return;

    FamiliarBattleOpponent found;
    bool haveIdentity = false;
    if (advertised->haveManufacturerData()) {
        const std::string data = advertised->getManufacturerData();
        if (data.size() >= 12) {
            const auto byte = [&data](size_t index) {
                return static_cast<uint8_t>(data[index]);
            };
            if (byte(0) == kCompanyIdLow && byte(1) == kCompanyIdHigh &&
                byte(2) == 'V' && byte(3) == 'P' &&
                byte(4) == kProtocolVersion && byte(11) != 0) {
                found.playerId = (static_cast<uint32_t>(byte(5)) << 24) |
                                 (static_cast<uint32_t>(byte(6)) << 16) |
                                 (static_cast<uint32_t>(byte(7)) << 8) |
                                 byte(8);
                found.stageIndex = byte(9);
                found.level = byte(10);
                haveIdentity = true;
            }
        }
    }
    // Fall back to a bare service-UUID match (no player/level info yet --
    // filled in once the real HELLO handshake happens after connecting).
    // Cross-platform BLE peripheral libraries (the desktop simulator uses
    // `bless`) don't all expose custom manufacturer data as reliably as
    // NimBLE does, but registering a GATT service always advertises its
    // UUID, so this keeps discovery working against those peers too.
    if (!haveIdentity && !advertised->isAdvertisingService(kServiceUuid)) {
        return;
    }

    found.address = advertised->getAddress().toString().c_str();
    found.addressType = advertised->getAddressType();
    found.rssi = advertised->getRSSI();
    if (!haveIdentity) found.level = 1;  // placeholder for the picker list

    for (auto& existing : scanResults_) {
        if (existing.address == found.address) {
            existing = found;
            return;
        }
    }
    scanResults_.push_back(found);
}

bool FamiliarBattleService::beginFind(uint32_t playerId, uint8_t stageIndex,
                                      uint8_t level) {
    end();
    resetForNewBattle();
    isHost_ = false;
    myPlayerId_ = playerId;
    myStageIndex_ = stageIndex;
    myLevel_ = level;
    myAttack_ = deriveAttack(level, stageIndex);
    myDefense_ = deriveDefense(level, stageIndex);
    myMaxHp_ = deriveMaxHp(level);
    myHp_ = myMaxHp_;

    state_ = FamiliarBattleState::Scanning;
    status_ = "Scanning...";
    scanResults_.clear();

    beginRadio();
    NimBLEScan* scanner = NimBLEDevice::getScan();
    if (scanner == nullptr) {
        status_ = "BLE scanner unavailable";
        teardownRadio();
        state_ = FamiliarBattleState::Idle;
        return false;
    }
    scanner->clearResults();
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(80);
    NimBLEScanResults results = scanner->getResults(kScanDurationMs, false);
    const int count = results.getCount();
    for (int i = 0; i < count; ++i) {
        parseAdvertisement(results.getDevice(i));
    }
    scanner->clearResults();
    teardownRadio();

    std::sort(scanResults_.begin(), scanResults_.end(),
              [](const FamiliarBattleOpponent& a, const FamiliarBattleOpponent& b) {
                  return a.rssi > b.rssi;
              });
    state_ = FamiliarBattleState::Idle;
    status_ = scanResults_.empty() ? "No VPet badges found"
                                   : String(scanResults_.size()) + " found";
    return true;
}

bool FamiliarBattleService::connectTo(size_t resultIndex) {
    if (resultIndex >= scanResults_.size()) return false;
    const FamiliarBattleOpponent target = scanResults_[resultIndex];

    beginRadio();
    client_ = NimBLEDevice::createClient();
    if (client_ == nullptr) {
        status_ = "BLE client unavailable";
        teardownRadio();
        return false;
    }
    const NimBLEAddress address(std::string(target.address.c_str()),
                                target.addressType);
    if (!client_->connect(address)) {
        status_ = "Connect failed";
        client_ = nullptr;
        teardownRadio();
        return false;
    }

    NimBLERemoteService* service = client_->getService(kServiceUuid);
    NimBLERemoteCharacteristic* notifyChar =
        service != nullptr ? service->getCharacteristic(kNotifyCharUuid)
                            : nullptr;
    remoteWriteChar_ = service != nullptr
                           ? service->getCharacteristic(kWriteCharUuid)
                           : nullptr;
    if (service == nullptr || notifyChar == nullptr ||
        remoteWriteChar_ == nullptr) {
        status_ = "VPet battle service not found";
        client_->disconnect();
        remoteWriteChar_ = nullptr;
        client_ = nullptr;
        teardownRadio();
        return false;
    }
    if (!notifyChar->subscribe(
            true, [this](NimBLERemoteCharacteristic*, uint8_t* data,
                         size_t length, bool) { onRawInbound(data, length); })) {
        status_ = "Notify subscribe failed";
        client_->disconnect();
        remoteWriteChar_ = nullptr;
        client_ = nullptr;
        teardownRadio();
        return false;
    }
    // Same 150ms settle as chameleon_ultra_client.cpp's connect(): the
    // CCCD subscription needs a moment on the peer's side before it's
    // actually ready to deliver notifications, even though the link
    // itself is already healthy at this point -- without it, HELLO's
    // response can beat that setup.
    delay(150);

    opponent_ = target;
    opponentAttack_ = deriveAttack(target.level, target.stageIndex);
    opponentDefense_ = deriveDefense(target.level, target.stageIndex);
    opponentMaxHp_ = deriveMaxHp(target.level);
    opponentHp_ = opponentMaxHp_;

    state_ = FamiliarBattleState::Connecting;
    status_ = "Connected - starting battle...";
    connectingDeadlineMs_ = millis() + kConnectTimeoutMs;
    sendHello();
    return true;
}

void FamiliarBattleService::end() {
    if (state_ == FamiliarBattleState::Idle && !initialized_) return;
    if (client_ != nullptr) {
        client_->disconnect();
        client_ = nullptr;
    }
    if (server_ != nullptr) {
        const std::vector<uint16_t> peers = server_->getPeerDevices();
        for (const uint16_t handle : peers) server_->disconnect(handle);
        const unsigned long deadline = millis() + 300;
        while (server_->getConnectedCount() > 0 &&
               static_cast<long>(deadline - millis()) > 0) {
            delay(10);
        }
    }
    teardownRadio();
    state_ = FamiliarBattleState::Idle;
    status_ = "Idle";
    isHost_ = false;
}

void FamiliarBattleService::onPeerConnected() { connectPending_ = true; }

void FamiliarBattleService::onPeerDisconnected() { disconnectPending_ = true; }

void FamiliarBattleService::onRawInbound(const uint8_t* data, size_t length) {
    const uint8_t head = inboundHead_;
    const uint8_t next = static_cast<uint8_t>((head + 1) % kInboundQueueDepth);
    if (next == inboundTail_) return;  // queue full -- drop rather than block
    InboundMessage& slot = inboundQueue_[head];
    slot.len = static_cast<uint8_t>(
        std::min(length, sizeof(InboundMessage::data)));
    memcpy(slot.data, data, slot.len);
    inboundHead_ = next;
}

void FamiliarBattleService::update() {
    if (connectPending_) {
        connectPending_ = false;
        if (isHost_ && state_ == FamiliarBattleState::Hosting) {
            status_ = "Challenger connected - awaiting handshake...";
        }
    }
    if (disconnectPending_) {
        disconnectPending_ = false;
        if (state_ == FamiliarBattleState::Battling ||
            state_ == FamiliarBattleState::Connecting) {
            addLog("Connection lost.");
            concludeBattle(FamiliarBattleOutcome::Disconnected);
        } else if (isHost_ && state_ == FamiliarBattleState::Hosting) {
            status_ = "Waiting for a challenger...";
        }
    }
    // Drain every queued message, not just one -- a peer can send its
    // HELLO reply immediately followed by its first MOVE with no gap
    // between them, and both can land before update() next runs.
    while (inboundTail_ != inboundHead_) {
        const InboundMessage& slot = inboundQueue_[inboundTail_];
        uint8_t buf[sizeof(InboundMessage::data)];
        const size_t len = slot.len;
        memcpy(buf, slot.data, len);
        inboundTail_ = static_cast<uint8_t>((inboundTail_ + 1) % kInboundQueueDepth);
        handleIncomingMessage(buf, len);
    }
    if (state_ == FamiliarBattleState::Connecting &&
        static_cast<long>(connectingDeadlineMs_ - millis()) < 0) {
        status_ = "Handshake timed out";
        concludeBattle(FamiliarBattleOutcome::Disconnected);
    }
}

void FamiliarBattleService::sendHello() {
    if (!isHost_) {
        // Only the challenger originates the shared seed -- the host
        // adopts whatever it receives in the challenger's HELLO.
        prngState_ = static_cast<uint32_t>(random(1, 2147483647)) ^
                    (static_cast<uint32_t>(micros()) << 1);
        if (prngState_ == 0) prngState_ = 1;
    }
    uint8_t buf[13];
    buf[0] = kMsgHello;
    buf[1] = static_cast<uint8_t>(myPlayerId_ >> 24);
    buf[2] = static_cast<uint8_t>(myPlayerId_ >> 16);
    buf[3] = static_cast<uint8_t>(myPlayerId_ >> 8);
    buf[4] = static_cast<uint8_t>(myPlayerId_);
    buf[5] = myStageIndex_;
    buf[6] = myLevel_;
    buf[7] = static_cast<uint8_t>(myHp_ >> 8);
    buf[8] = static_cast<uint8_t>(myHp_);
    buf[9] = static_cast<uint8_t>(prngState_ >> 24);
    buf[10] = static_cast<uint8_t>(prngState_ >> 16);
    buf[11] = static_cast<uint8_t>(prngState_ >> 8);
    buf[12] = static_cast<uint8_t>(prngState_);
    if (sendRaw(buf, sizeof(buf))) helloSent_ = true;
}

void FamiliarBattleService::sendMove(FamiliarBattleMove move) {
    uint8_t buf[2] = {kMsgMove, static_cast<uint8_t>(move)};
    sendRaw(buf, sizeof(buf));
}

bool FamiliarBattleService::sendRaw(const uint8_t* data, size_t length) {
    if (isHost_) {
        return notifyChar_ != nullptr && notifyChar_->notify(data, length);
    }
    return remoteWriteChar_ != nullptr &&
           remoteWriteChar_->writeValue(data, length, true);
}

void FamiliarBattleService::handleIncomingMessage(const uint8_t* data,
                                                  size_t length) {
    if (length == 0) return;
    switch (data[0]) {
        case kMsgHello: {
            if (length < 13) return;
            const uint32_t peerId = (static_cast<uint32_t>(data[1]) << 24) |
                                    (static_cast<uint32_t>(data[2]) << 16) |
                                    (static_cast<uint32_t>(data[3]) << 8) |
                                    data[4];
            const uint8_t peerStage = data[5];
            const uint8_t peerLevel = data[6];
            const uint16_t peerHp =
                (static_cast<uint16_t>(data[7]) << 8) | data[8];
            const uint32_t peerSeed = (static_cast<uint32_t>(data[9]) << 24) |
                                      (static_cast<uint32_t>(data[10]) << 16) |
                                      (static_cast<uint32_t>(data[11]) << 8) |
                                      data[12];
            opponent_.playerId = peerId;
            opponent_.stageIndex = peerStage;
            opponent_.level = peerLevel;
            opponentAttack_ = deriveAttack(peerLevel, peerStage);
            opponentDefense_ = deriveDefense(peerLevel, peerStage);
            opponentMaxHp_ = deriveMaxHp(peerLevel);
            opponentHp_ = peerHp > 0 ? peerHp : opponentMaxHp_;
            helloReceived_ = true;
            if (isHost_) {
                prngState_ = peerSeed == 0 ? 1 : peerSeed;
                sendHello();
            }
            if (helloSent_ && helloReceived_ &&
                state_ != FamiliarBattleState::Battling) {
                state_ = FamiliarBattleState::Battling;
                status_ = "Battle!";
                turnNumber_ = 1;
                addLog("A challenger's Familiar (Lv " + String(peerLevel) +
                      ") appeared!");
            }
            break;
        }
        case kMsgMove: {
            if (length < 2 || state_ != FamiliarBattleState::Battling) return;
            opponentMove_ = data[1] <= 3
                                ? static_cast<FamiliarBattleMove>(data[1])
                                : FamiliarBattleMove::Attack;
            if (opponentMove_ == FamiliarBattleMove::Flee) {
                addLog("Opponent fled the battle.");
                concludeBattle(FamiliarBattleOutcome::OpponentFled);
                return;
            }
            opponentMoveSubmitted_ = true;
            resolveTurnIfReady();
            break;
        }
        default:
            break;
    }
}

void FamiliarBattleService::submitMove(FamiliarBattleMove move) {
    if (state_ != FamiliarBattleState::Battling || myMoveSubmitted_) return;
    myMove_ = move;
    myMoveSubmitted_ = true;
    sendMove(move);
    if (move == FamiliarBattleMove::Flee) {
        addLog("You fled the battle.");
        concludeBattle(FamiliarBattleOutcome::Fled);
        return;
    }
    resolveTurnIfReady();
}

void FamiliarBattleService::resolveTurnIfReady() {
    if (!(myMoveSubmitted_ && opponentMoveSubmitted_)) return;

    // Fixed processing order (lower playerId's move resolves first) so
    // both devices draw from the shared PRNG in the same sequence.
    struct Actor {
        bool isMe;
        FamiliarBattleMove move;
    };
    Actor order[2] = {{true, myMove_}, {false, opponentMove_}};
    if (myPlayerId_ > opponent_.playerId) std::swap(order[0], order[1]);

    const bool myDefending = (myMove_ == FamiliarBattleMove::Defend);
    const bool opponentDefending = (opponentMove_ == FamiliarBattleMove::Defend);

    for (const Actor& actor : order) {
        if (actor.move != FamiliarBattleMove::Attack &&
            actor.move != FamiliarBattleMove::Special) {
            continue;
        }
        const uint8_t atk = actor.isMe ? myAttack_ : opponentAttack_;
        const uint8_t def = actor.isMe ? opponentDefense_ : myDefense_;
        const bool targetDefending = actor.isMe ? opponentDefending : myDefending;
        const uint32_t roll = nextRandom();
        const bool isSpecial = actor.move == FamiliarBattleMove::Special;
        if (isSpecial && roll % 100 < 15) {
            addLog(String(actor.isMe ? "Your" : "Opponent's") + " Special missed!");
            continue;
        }
        int32_t damage = static_cast<int32_t>(atk) - static_cast<int32_t>(def) / 2;
        if (isSpecial) damage = damage * 3 / 2;
        const int32_t variance = static_cast<int32_t>(roll % 41) - 20;  // +/-20%
        damage += damage * variance / 100;
        if (targetDefending) damage /= 2;
        if (damage < 1) damage = 1;

        if (actor.isMe) {
            opponentHp_ = opponentHp_ > damage ? opponentHp_ - damage : 0;
        } else {
            myHp_ = myHp_ > damage ? myHp_ - damage : 0;
        }
        addLog(String(actor.isMe ? "You" : "Opponent") + " used " +
              (isSpecial ? "Special" : "Attack") + " for " + String(damage) +
              " dmg.");
    }

    ++turnNumber_;
    myMoveSubmitted_ = false;
    opponentMoveSubmitted_ = false;

    if (myHp_ == 0 || opponentHp_ == 0) {
        concludeBattle(opponentHp_ == 0 ? FamiliarBattleOutcome::Victory
                                        : FamiliarBattleOutcome::Defeat);
    }
}

void FamiliarBattleService::concludeBattle(FamiliarBattleOutcome outcome) {
    outcome_ = outcome;
    state_ = FamiliarBattleState::Result;
    status_ = "Battle over";
}

uint32_t FamiliarBattleService::nextRandom() {
    uint32_t x = prngState_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prngState_ = x;
    return x;
}

void FamiliarBattleService::addLog(const String& line) {
    log_.push_back(line);
    if (log_.size() > 40) log_.erase(log_.begin());
}
