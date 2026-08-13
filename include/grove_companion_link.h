#pragma once

#include <Arduino.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <vector>

#include "auth.h"
#include "grove_link.h"
#include "loot_entry.h"

// Mirrors the P4's PAYLOAD_SCRIPT_MAX_BYTES (poe-p4/main/main.c) -- a
// script this size or larger is rejected before ever touching the wire.
constexpr size_t kPayloadScriptMaxBytes = 512;

// Parses all three Grove frame types the P4 companion sends: heartbeat (H,
// liveness only), status (S, live telemetry), and identity (I, device id +
// firmware). Heartbeat/ack stays the sole basis for connected() -- status
// and identity are one-way pushes with their own freshness windows, tracked
// independently so a screen can prefer this data over PoeCompanionService's
// network-sourced telemetry whenever it's fresh.
//
// Also initiates Grove pairing (X25519 ECDH -> HKDF-SHA256 session key, see
// shared/protocol/auth.h) and handles the P4's response (frame type 'Q').
// Session-key persistence (Preferences) is main.cpp's job, matching how
// every other persisted setting in this codebase is handled directly there
// -- this class stays storage-agnostic and just exposes the derived key.
enum class GrovePairingState {
    Idle,
    InProgress,
    Success,
    Failed,
};

// Transient status of the most recently sent authenticated command (mirrors
// GrovePairingState in spirit: Idle until something's been sent, then
// resolved by the P4's 'K' response or a timeout). Cleared to Idle by the
// next sendCommand() call, not by reading it.
enum class GroveCommandAckState {
    Idle,
    Pending,
    Accepted,
    Rejected,
};

class GroveCompanionLink {
public:
    void begin();
    void update();
    bool connected() const;
    uint32_t lastSequence() const { return lastSequence_; }
    uint32_t validFrames() const { return validFrames_; }
    uint32_t crcErrors() const { return crcErrors_; }
    unsigned long frameAgeMs() const {
        return validFrames_ == 0 ? 0 : millis() - lastHeartbeatMs_;
    }
    unsigned long maximumGapMs() const { return maximumGapMs_; }

    // Live telemetry from the most recent valid status (S) frame.
    bool statusFresh() const;
    unsigned long statusAgeMs() const {
        return everHadStatus_ ? millis() - lastStatusMs_ : 0;
    }
    // Timestamp (not an age) of the last valid status frame, for callers
    // that want to detect "a new status frame arrived" by diffing this
    // against a previously-seen value -- statusAgeMs() itself changes every
    // millisecond and isn't useful for that.
    unsigned long lastStatusUpdateMs() const { return lastStatusMs_; }
    bool ethernetLinkUp() const { return statusLinkUp_; }
    uint32_t linkSpeedMbps() const { return statusLinkSpeedMbps_; }
    bool fullDuplex() const { return statusFullDuplex_; }
    bool internetReachable() const { return statusInternetReachable_; }
    const char* indicatorState() const;
    uint64_t uptimeMs() const { return statusUptimeSeconds_ * 1000ULL; }
    const char* resetReason() const;
    bool hasTemperature() const { return statusTempX10_ != GHOSTWIRE_GROVE_NO_TEMPERATURE; }
    float temperatureC() const { return statusTempX10_ / 10.0f; }
    uint32_t freeHeapBytes() const { return statusFreeHeapKb_ * 1024U; }
    uint32_t minimumFreeHeapBytes() const { return statusMinHeapKb_ * 1024U; }
    // Empty until the P4 has a DHCP lease -- check before use.
    const char* ip() const { return statusIp_; }
    // The payload engine's traffic-light state and the finding count from
    // its last completed scan, mirroring the P4's own LED. "idle" until the
    // first status frame with these fields arrives (older P4 firmware won't
    // send them at all, which parses the same as idle/0).
    const char* payloadRunState() const;
    size_t payloadFindingCount() const { return payloadFindingCount_; }

    // Device identity from the most recent valid identity (I) frame.
    bool identityFresh() const;
    const char* deviceId() const { return deviceId_; }
    const char* firmwareVersion() const { return firmwareVersion_; }

    // Grove pairing. beginPairing() returns false immediately if the RNG
    // couldn't be seeded; otherwise the result arrives asynchronously (the
    // P4's 'Q' response, or a timeout) -- poll pairingState().
    bool beginPairing();
    GrovePairingState pairingState() const { return pairingState_; }
    bool hasSessionKey() const { return sessionKeyValid_; }
    // Valid only when hasSessionKey() is true.
    const uint8_t* sessionKey() const { return sessionKey_; }
    // Re-injects a previously-persisted key (e.g. loaded from Preferences at
    // boot) without going through a new pairing handshake.
    void loadSessionKey(const uint8_t key[GHOSTWIRE_AUTH_KEY_BYTES]);
    // Clears the in-memory session key/clock offset and resets pairing to
    // Idle. Callers still need to clear their own persisted copy (main.cpp
    // owns the Preferences entry, matching how loadSessionKey() doesn't
    // load from Preferences itself either).
    void forgetSessionKey();

    // Companion Mode command channel: authenticated payload-slot triggers
    // over Grove. Requires both hasSessionKey() (paired) and hasClockSync()
    // (derived from the P4's clock at pairing time -- see
    // shared/protocol/README.md's "Grove pairing and command authentication"
    // section). sendCommand() only reports whether the request could be
    // sent, not whether the P4 accepted it -- that arrives asynchronously as
    // the 'K' response; poll commandAckState().
    bool hasClockSync() const { return clockOffsetValid_; }
    bool sendCommand(uint8_t slot);
    GroveCommandAckState commandAckState() const { return commandAckState_; }
    // For PoeCompanionService's Wi-Fi command path: same session key/clock
    // offset, but a 5-byte authenticated message (slot || nonce) instead of
    // Grove's bare slot byte -- see shared/protocol/README.md's "Wi-Fi
    // command channel" section for why Wi-Fi needs the nonce and Grove
    // doesn't. Requires hasSessionKey() and hasClockSync(), same as
    // sendCommand().
    bool buildWifiCommandTag(uint8_t slot, uint32_t nonce,
                             uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const;

    // Script upload / loot extraction (shared/protocol/README.md). Unlike
    // sendCommand() above, these are synchronous/blocking -- each drives a
    // short multi-frame exchange (begin/data/finish, or request/count/
    // entries) and waits (bounded) for the P4's final reply, matching how
    // every other *explicit* action in this codebase blocks (HTTPClient
    // calls, SD writes) rather than needing a polled async state machine.
    // Both require hasSessionKey() and hasClockSync(), same as sendCommand().
    bool sendScript(uint8_t slot, const String& script);
    bool requestLoot(std::vector<LootEntry>& out);
    // For PoeCompanionService's Wi-Fi equivalents (POST /v1/payload,
    // /v1/loot) -- same message shapes sendScript()/requestLoot() use
    // internally (slot||nonce||sha256(script), and nonce alone), so both
    // transports authenticate identically.
    bool buildWifiScriptTag(uint8_t slot, uint32_t nonce, const String& script,
                            uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const;
    bool buildWifiLootTag(uint32_t nonce, uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const;

private:
    void processLine();
    // Each takes the mutable, comma-separated field list after "GW1,<type>,"
    // (CRC and its separator already stripped by processLine) and splits it
    // in place, so no allocation is needed to parse a frame.
    void processHeartbeat(char* fields);
    void processStatus(char* fields);
    void processIdentity(char* fields);
    void processPairingResponse(char* fields);
    void processCommandAck(char* fields);
    bool ensurePairingRngReady();
    // Mirrors the P4's ghostwire_auth_compute_tag() (shared/protocol/auth.h)
    // byte for byte, using the same mbedtls_md_hmac primitive already
    // proven available here from pairing's HKDF. `nowUtc` is this device's
    // best guess at current Unix time -- see currentUnixTime().
    bool computeAuthTag(const uint8_t* message, size_t messageLen,
                        int64_t nowUtc,
                        uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const;
    // clockOffsetSeconds_ + millis()/1000, valid only once hasClockSync().
    int64_t currentUnixTime() const;
    // Blocking helpers for sendScript()/requestLoot() above -- read raw
    // bytes directly from serial_ into a local buffer, deliberately
    // bypassing update()'s own line_/lineLength_ accumulator so resuming
    // normal heartbeat/status processing afterward isn't disturbed.
    // Heartbeat/status/identity frames (and anything else not matching
    // what's being waited for) arrive interleaved during the wait and are
    // simply skipped, not processed, since acting on them here would risk
    // re-entrant state changes mid-transfer.
    bool readGroveLine(String& outLine, unsigned long timeoutMs);
    // Waits up to timeoutMs for a validated (CRC-checked) frame of the
    // given type letter and sequence; anything else -- including a CRC
    // failure, which still counts toward crcErrors_ -- is skipped and the
    // wait continues. `fields` receives everything between "GW1,<type>,
    // <seq>," and the CRC field.
    bool waitForGroveFrame(char type, uint32_t sequence, String& fields,
                           unsigned long timeoutMs);

    HardwareSerial serial_{2};
    char line_[96] = {};
    size_t lineLength_ = 0;

    uint32_t lastSequence_ = 0;
    uint32_t validFrames_ = 0;
    uint32_t crcErrors_ = 0;
    unsigned long lastHeartbeatMs_ = 0;
    unsigned long maximumGapMs_ = 0;

    bool statusLinkUp_ = false;
    uint32_t statusLinkSpeedMbps_ = 0;
    bool statusFullDuplex_ = false;
    bool statusInternetReachable_ = false;
    char statusIndicatorCode_ = 'R';
    uint64_t statusUptimeSeconds_ = 0;
    char statusResetCode_ = 'U';
    int statusTempX10_ = GHOSTWIRE_GROVE_NO_TEMPERATURE;
    uint32_t statusFreeHeapKb_ = 0;
    uint32_t statusMinHeapKb_ = 0;
    char statusIp_[16] = {};
    char payloadStateCode_ = 'I';
    size_t payloadFindingCount_ = 0;
    unsigned long lastStatusMs_ = 0;
    bool everHadStatus_ = false;

    char deviceId_[32] = {};
    char firmwareVersion_[16] = {};
    unsigned long lastIdentityMs_ = 0;
    bool everHadIdentity_ = false;

    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctrDrbg_;
    bool rngReady_ = false;
    mbedtls_ecdh_context pairingCtx_;
    bool pairingCtxActive_ = false;
    uint32_t pairingSequence_ = 0;
    unsigned long pairingStartMs_ = 0;
    GrovePairingState pairingState_ = GrovePairingState::Idle;
    uint8_t sessionKey_[GHOSTWIRE_AUTH_KEY_BYTES] = {};
    bool sessionKeyValid_ = false;

    // Derived once at successful pairing from the P4's NTP-synced clock (its
    // 'Q' response's unix_time field) -- the Cardputer has no RTC/NTP of its
    // own outside a manual TimeStatus sync, and requiring that first would
    // reintroduce a Wi-Fi dependency for a Grove-only feature. See
    // shared/protocol/README.md.
    bool clockOffsetValid_ = false;
    int64_t clockOffsetSeconds_ = 0;

    uint32_t commandSequence_ = 0;
    unsigned long commandStartMs_ = 0;
    GroveCommandAckState commandAckState_ = GroveCommandAckState::Idle;

    // Separate from commandSequence_ (async, command-triggering only) --
    // used by the synchronous sendScript()/requestLoot() exchanges.
    uint32_t groveExchangeSequence_ = 0;
};
