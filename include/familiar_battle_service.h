#pragma once

#include <Arduino.h>
#include <vector>

class NimBLEServer;
class NimBLECharacteristic;
class NimBLEClient;
class NimBLERemoteCharacteristic;
class NimBLEAdvertisedDevice;

enum class FamiliarBattleState : uint8_t {
    Idle,
    Hosting,     // advertising + GATT server, waiting for a challenger
    Scanning,    // Find Opponent: a bounded scan is in progress
    Connecting,  // Find Opponent: connected, waiting on the HELLO handshake
    Battling,
    Result,
};

enum class FamiliarBattleMove : uint8_t {
    Attack = 0,
    Defend = 1,
    Special = 2,
    Flee = 3,
};

enum class FamiliarBattleOutcome : uint8_t {
    None,
    Victory,
    Defeat,
    Fled,
    OpponentFled,
    Disconnected,
};

struct FamiliarBattleOpponent {
    String address;   // "XX:XX:XX:XX:XX:XX", re-resolved to a NimBLEAddress
                      // at connectTo() time rather than keeping a raw
                      // NimBLEAdvertisedDevice* alive across the scan ->
                      // pick-from-list -> connect gap.
    uint8_t addressType = 0;
    uint32_t playerId = 0;
    uint8_t stageIndex = 0;
    uint8_t level = 0;
    int rssi = 0;
};

// Manual "Direct Challenge" BLE PvP battles between two Familiars -- see
// Ideas/vpet-battle-system.md. Deliberately scoped to one BLE role active
// at a time (Host: advertise + GATT server, waiting to be challenged --
// Find: bounded scan, then connect as a GATT client to a chosen target),
// matching every other BLE feature in this codebase rather than the doc's
// full passive concurrent-roaming design (advertise+scan+server+client all
// running at once), which is a natural follow-up once this is proven on
// hardware. See CHANGELOG.md "Unreleased" for the scoping rationale.
//
// NimBLE's own callbacks (onWrite, notify subscriptions, connect/
// disconnect) run on the host task, not the caller's task -- they only
// ever push the raw bytes into a small SPSC ring buffer (a peer can send
// two notifications back-to-back with no gap, so a single-slot handoff
// isn't enough here); update() (call every loop() tick while a battle
// screen is active) drains it and does all real state mutation on the
// caller's own task.
class FamiliarBattleService {
public:
    // Stat formulas are centralized here (not on CyberFamiliar) so both
    // "my" stats and an opponent's (derived from their HELLO-reported
    // level/stageIndex, never trusted as raw values) come from one place.
    static uint16_t deriveMaxHp(uint8_t level);
    static uint8_t deriveAttack(uint8_t level, uint8_t stageIndex);
    static uint8_t deriveDefense(uint8_t level, uint8_t stageIndex);

    // `playerId` should be stable across boots -- main.cpp derives it from
    // ESP.getEfuseMac(), the same source already used for other per-device
    // IDs in this app.
    bool beginHost(uint32_t playerId, uint8_t stageIndex, uint8_t level);
    // Bounded, blocking scan (same shape as ble_scanner.cpp's scan() /
    // chameleon_ultra_client.cpp's connect()) -- populates scanResults().
    // Call again to rescan.
    bool beginFind(uint32_t playerId, uint8_t stageIndex, uint8_t level);
    const std::vector<FamiliarBattleOpponent>& scanResults() const {
        return scanResults_;
    }
    bool connectTo(size_t resultIndex);
    void end();
    void update();

    FamiliarBattleState state() const { return state_; }
    const String& status() const { return status_; }
    const FamiliarBattleOpponent& opponent() const { return opponent_; }

    uint16_t myHp() const { return myHp_; }
    uint16_t myMaxHp() const { return myMaxHp_; }
    uint16_t opponentHp() const { return opponentHp_; }
    uint16_t opponentMaxHp() const { return opponentMaxHp_; }
    uint16_t turnNumber() const { return turnNumber_; }
    bool myMoveSubmitted() const { return myMoveSubmitted_; }
    // No-op if a move is already submitted for the current turn.
    void submitMove(FamiliarBattleMove move);
    FamiliarBattleOutcome outcome() const { return outcome_; }
    const std::vector<String>& log() const { return log_; }

    // Public only so the free-function/small-class NimBLE callback shims
    // in the .cpp can reach them -- not part of the intended external API.
    void onRawInbound(const uint8_t* data, size_t length);
    void onPeerConnected();
    void onPeerDisconnected();

private:
    void beginRadio();  // shared Wi-Fi-off + NimBLEDevice::init() handoff
    void teardownRadio();
    void resetForNewBattle();
    void parseAdvertisement(const NimBLEAdvertisedDevice* advertised);
    void sendHello();
    void sendMove(FamiliarBattleMove move);
    bool sendRaw(const uint8_t* data, size_t length);
    void handleIncomingMessage(const uint8_t* data, size_t length);
    // Flee is just MOVE:FLEE (the doc's own vocabulary) -- fleeing ends the
    // battle unilaterally rather than waiting on resolveTurnIfReady(), so
    // it's handled inline wherever a Flee move is seen, not a separate
    // message type.
    void resolveTurnIfReady();
    void concludeBattle(FamiliarBattleOutcome outcome);
    uint32_t nextRandom();
    void addLog(const String& line);

    FamiliarBattleState state_ = FamiliarBattleState::Idle;
    String status_ = "Idle";
    bool isHost_ = false;
    bool initialized_ = false;

    uint32_t myPlayerId_ = 0;
    uint8_t myStageIndex_ = 0;
    uint8_t myLevel_ = 0;
    uint8_t myAttack_ = 0;
    uint8_t myDefense_ = 0;

    FamiliarBattleOpponent opponent_;
    uint8_t opponentAttack_ = 0;
    uint8_t opponentDefense_ = 0;

    uint16_t myHp_ = 0, myMaxHp_ = 0;
    uint16_t opponentHp_ = 0, opponentMaxHp_ = 0;
    uint16_t turnNumber_ = 0;
    bool myMoveSubmitted_ = false;
    bool opponentMoveSubmitted_ = false;
    FamiliarBattleMove myMove_ = FamiliarBattleMove::Attack;
    FamiliarBattleMove opponentMove_ = FamiliarBattleMove::Attack;
    bool helloSent_ = false;
    bool helloReceived_ = false;
    uint32_t prngState_ = 1;
    FamiliarBattleOutcome outcome_ = FamiliarBattleOutcome::None;
    std::vector<String> log_;
    std::vector<FamiliarBattleOpponent> scanResults_;

    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* writeChar_ = nullptr;   // host: challenger writes here
    NimBLECharacteristic* notifyChar_ = nullptr;  // host: we notify here
    NimBLEClient* client_ = nullptr;
    NimBLERemoteCharacteristic* remoteWriteChar_ = nullptr;
    unsigned long connectingDeadlineMs_ = 0;

    // Small SPSC ring buffer for inbound handoff from the NimBLE host task
    // (the sole producer, via onRawInbound()) to the main loop task (the
    // sole consumer, via update()). A single-slot handoff (this used to be
    // one, matching ChameleonUltraClient's responseBuf_) isn't enough here
    // -- unlike that strictly request/response-paced protocol, a peer can
    // send its HELLO reply immediately followed by its first MOVE with no
    // gap, and both notifications can land before update() next runs,
    // silently clobbering the first with the second. Head is only written
    // by the producer, tail only by the consumer -- safe without a mutex.
    struct InboundMessage {
        uint8_t data[16];
        uint8_t len;
    };
    static constexpr size_t kInboundQueueDepth = 8;
    InboundMessage inboundQueue_[kInboundQueueDepth]{};
    volatile uint8_t inboundHead_ = 0;
    volatile uint8_t inboundTail_ = 0;
    volatile bool connectPending_ = false;
    volatile bool disconnectPending_ = false;
};
