#include "grove_companion_link.h"

#include <esp_system.h>
#include <mbedtls/md.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "grove_link.h"

namespace {
constexpr int kRxPin = 2;
constexpr int kTxPin = 1;
constexpr unsigned long kLinkTimeoutMs = 5000;
// Status (S) frames are the widest at 14 fields as of the payload-state/
// finding-count extension; sized with a little headroom above that.
constexpr int kMaxFields = 16;
constexpr unsigned long kPairingTimeoutMs = 5000;
constexpr unsigned long kCommandTimeoutMs = 3000;
// The P4's 'Q' response carries time(NULL); anything below this (~Nov 2023)
// means its own NTP sync hasn't happened yet, so treating it as a real
// offset would silently compute bogus auth windows instead of just refusing
// commands with a clear "not clock-synced" state.
constexpr unsigned long long kMinPlausibleUnixTime = 1700000000ULL;

// Splits `text` in place on commas (each comma becomes '\0') and fills
// `tokens` with a pointer to the start of each field. Returns the field
// count, capped at `kMaxFields` so a malformed/oversized frame can't write
// past the array.
int splitFields(char* text, char* tokens[kMaxFields]) {
    int count = 0;
    char* cursor = text;
    tokens[count++] = cursor;
    while (*cursor != '\0' && count < kMaxFields) {
        if (*cursor == ',') {
            *cursor = '\0';
            tokens[count++] = cursor + 1;
        }
        ++cursor;
    }
    return count;
}

void hexEncode(char* dest, const uint8_t* src, size_t srcLen) {
    static const char kDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < srcLen; ++i) {
        dest[i * 2] = kDigits[src[i] >> 4];
        dest[i * 2 + 1] = kDigits[src[i] & 0x0F];
    }
    dest[srcLen * 2] = '\0';
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool hexDecode(uint8_t* dest, size_t destLen, const char* src) {
    if (strlen(src) != destLen * 2) return false;
    for (size_t i = 0; i < destLen; ++i) {
        const int hi = hexNibble(src[i * 2]);
        const int lo = hexNibble(src[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        dest[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

// HKDF-SHA256 (RFC 5869), restricted to okmLen <= 32: Arduino-ESP32's
// prebuilt mbedtls links mbedtls_md_hmac but not mbedtls_hkdf, so this hand-
// rolls just the HKDF construction on top of the HMAC primitive that IS
// available. Since our one use (a 32-byte session key) exactly equals
// SHA-256's output size, HKDF-Expand only ever needs its first round
// (T(1) = HMAC(PRK, info || 0x01)), so this doesn't need the general
// multi-round expand loop -- restricting to that case keeps this a faithful,
// simple implementation of the standard rather than a novel one, and it
// produces byte-identical output to the P4's real mbedtls_hkdf() given the
// same salt/ikm/info.
bool hkdfSha256(const uint8_t* salt, size_t saltLen, const uint8_t* ikm,
                size_t ikmLen, const uint8_t* info, size_t infoLen,
                uint8_t* okm, size_t okmLen) {
    if (okmLen > 32) return false;
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md == nullptr) return false;
    uint8_t prk[32];
    if (mbedtls_md_hmac(md, salt, saltLen, ikm, ikmLen, prk) != 0) return false;
    uint8_t buffer[128];
    if (infoLen + 1 > sizeof(buffer)) return false;
    memcpy(buffer, info, infoLen);
    buffer[infoLen] = 0x01;
    uint8_t t1[32];
    if (mbedtls_md_hmac(md, prk, sizeof(prk), buffer, infoLen + 1, t1) != 0) {
        return false;
    }
    memcpy(okm, t1, okmLen);
    return true;
}
}  // namespace

void GroveCompanionLink::begin() {
    // Absorb multi-second stalls elsewhere in the main loop (e.g. blocking
    // PoE companion mDNS/HTTP discovery) without dropping frames. The
    // default 256-byte buffer only covers ~1.5s of backlog at this baud and
    // frame rate; a ~4.5s discovery stall was measured pushing it close to
    // overflow. Must be set before begin().
    serial_.setRxBufferSize(1024);
    serial_.begin(GHOSTWIRE_GROVE_BAUD, SERIAL_8N1, kRxPin, kTxPin);
}

bool GroveCompanionLink::connected() const {
    return validFrames_ > 0 && millis() - lastHeartbeatMs_ < kLinkTimeoutMs;
}

bool GroveCompanionLink::statusFresh() const {
    return everHadStatus_ &&
           millis() - lastStatusMs_ < GHOSTWIRE_GROVE_STATUS_TIMEOUT_MS;
}

bool GroveCompanionLink::identityFresh() const {
    return everHadIdentity_ &&
           millis() - lastIdentityMs_ < GHOSTWIRE_GROVE_IDENTITY_TIMEOUT_MS;
}

const char* GroveCompanionLink::indicatorState() const {
    return ghostwire_grove_indicator_name(statusIndicatorCode_);
}

const char* GroveCompanionLink::resetReason() const {
    return ghostwire_grove_reset_name(statusResetCode_);
}

void GroveCompanionLink::processHeartbeat(char* fields) {
    // fields: "<sequence>,<ts_ms>"
    char* sequenceEnd = nullptr;
    const uint32_t sequence = strtoul(fields, &sequenceEnd, 10);
    if (sequenceEnd == fields || *sequenceEnd != ',') return;

    const unsigned long now = millis();
    if (validFrames_ > 0) {
        const unsigned long gap = now - lastHeartbeatMs_;
        if (gap > maximumGapMs_) maximumGapMs_ = gap;
    }
    lastSequence_ = sequence;
    lastHeartbeatMs_ = now;
    ++validFrames_;
    char response[48];
    const int payloadLength = snprintf(response, sizeof(response), "GW1,A,%lu",
                                       static_cast<unsigned long>(sequence));
    const uint32_t crc = ghostwire_grove_crc32(response, payloadLength);
    serial_.printf("%s,%08lX\n", response, static_cast<unsigned long>(crc));
}

void GroveCompanionLink::processStatus(char* fields) {
    // fields: "<seq>,<link>,<speed>,<duplex>,<internet>,<ind>,<uptime_s>,
    //          <reset>,<temp_x10>,<heap_kb>,<min_heap_kb>,<ip>,
    //          <payload_state>,<finding_count>"
    char* tokens[kMaxFields];
    if (splitFields(fields, tokens) != 14) return;

    statusLinkUp_ = atoi(tokens[1]) != 0;
    statusLinkSpeedMbps_ = static_cast<uint32_t>(strtoul(tokens[2], nullptr, 10));
    statusFullDuplex_ = atoi(tokens[3]) != 0;
    statusInternetReachable_ = atoi(tokens[4]) != 0;
    statusIndicatorCode_ = tokens[5][0] != '\0' ? tokens[5][0] : 'R';
    statusUptimeSeconds_ = strtoull(tokens[6], nullptr, 10);
    statusResetCode_ = tokens[7][0] != '\0' ? tokens[7][0] : 'U';
    statusTempX10_ = static_cast<int>(strtol(tokens[8], nullptr, 10));
    statusFreeHeapKb_ = static_cast<uint32_t>(strtoul(tokens[9], nullptr, 10));
    statusMinHeapKb_ = static_cast<uint32_t>(strtoul(tokens[10], nullptr, 10));
    snprintf(statusIp_, sizeof(statusIp_), "%s", tokens[11]);
    payloadStateCode_ = tokens[12][0] != '\0' ? tokens[12][0] : 'I';
    payloadFindingCount_ = static_cast<size_t>(strtoul(tokens[13], nullptr, 10));

    lastStatusMs_ = millis();
    everHadStatus_ = true;
}

const char* GroveCompanionLink::payloadRunState() const {
    switch (payloadStateCode_) {
        case 'R': return "running";
        case 'S': return "success";
        case 'E': return "error";
        case 'I':
        default: return "idle";
    }
}

void GroveCompanionLink::processIdentity(char* fields) {
    // fields: "<seq>,<device_id>,<firmware>"
    char* tokens[kMaxFields];
    if (splitFields(fields, tokens) != 3) return;

    snprintf(deviceId_, sizeof(deviceId_), "%s", tokens[1]);
    snprintf(firmwareVersion_, sizeof(firmwareVersion_), "%s", tokens[2]);

    lastIdentityMs_ = millis();
    everHadIdentity_ = true;
}

// --- Grove pairing (X25519 ECDH -> HKDF-SHA256 session key) -------------
// Full design/threat-model writeup: shared/protocol/auth.h. Reuses mbedtls
// (already bundled with Arduino-ESP32, the same library the P4 side uses
// via raw ESP-IDF) rather than hand-rolling any crypto primitive.
bool GroveCompanionLink::ensurePairingRngReady() {
    if (rngReady_) return true;
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&ctrDrbg_);
    static const char personalization[] = "ghostwire-cardputer-pairing";
    const int result = mbedtls_ctr_drbg_seed(
        &ctrDrbg_, mbedtls_entropy_func, &entropy_,
        reinterpret_cast<const unsigned char*>(personalization),
        sizeof(personalization) - 1);
    rngReady_ = result == 0;
    if (!rngReady_) {
        Serial.printf("[grove] pairing RNG seed FAILED (%d)\n", result);
    }
    return rngReady_;
}

bool GroveCompanionLink::beginPairing() {
    if (!ensurePairingRngReady()) {
        pairingState_ = GrovePairingState::Failed;
        return false;
    }
    if (pairingCtxActive_) {
        mbedtls_ecdh_free(&pairingCtx_);
        pairingCtxActive_ = false;
    }
    mbedtls_ecdh_init(&pairingCtx_);
    const int setupResult = mbedtls_ecdh_setup(&pairingCtx_, MBEDTLS_ECP_DP_CURVE25519);
    if (setupResult != 0) {
        Serial.printf("[grove] pairing ecdh_setup FAILED (%d)\n", setupResult);
        pairingState_ = GrovePairingState::Failed;
        return false;
    }
    pairingCtxActive_ = true;

    // mbedtls_ecdh_make_public()/read_public() speak the TLS ECPoint wire
    // format (RFC 8422: a 1-byte length prefix followed by the point),
    // *not* a bare public key -- for Curve25519 that's 1 + 32 = 33 bytes,
    // with byte 0 always 32. Stripped/re-added here so the actual Grove
    // wire format stays the clean 32-byte raw key (64 hex chars) documented
    // in shared/protocol/README.md.
    uint8_t ownPublicTls[33];
    size_t ownPublicTlsLen = 0;
    const int makePublicResult = mbedtls_ecdh_make_public(
        &pairingCtx_, &ownPublicTlsLen, ownPublicTls, sizeof(ownPublicTls),
        mbedtls_ctr_drbg_random, &ctrDrbg_);
    if (makePublicResult != 0 || ownPublicTlsLen != sizeof(ownPublicTls) ||
        ownPublicTls[0] != 32) {
        Serial.printf("[grove] pairing make_public FAILED (%d, len=%u)\n",
                      makePublicResult, static_cast<unsigned>(ownPublicTlsLen));
        mbedtls_ecdh_free(&pairingCtx_);
        pairingCtxActive_ = false;
        pairingState_ = GrovePairingState::Failed;
        return false;
    }
    const uint8_t* ownPublic = ownPublicTls + 1;

    ++pairingSequence_;
    char publicHex[65];
    hexEncode(publicHex, ownPublic, 32);
    // "GW1,P," (6) + up to 10 digits + comma (11) + 64 hex chars: 81 without
    // the trailing nul, so 80 could truncate at the high end of uint32_t --
    // sized to GHOSTWIRE_GROVE_MAX_LINE for headroom instead. Also matters
    // beyond just correctness: `length` below is used to CRC exactly what
    // was written, and a truncated snprintf() return value larger than what
    // actually fit would have made that read past the buffer.
    char payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int length = snprintf(payload, sizeof(payload), "GW1,P,%lu,%s",
                                static_cast<unsigned long>(pairingSequence_),
                                publicHex);
    const uint32_t crc = ghostwire_grove_crc32(payload, length);
    serial_.printf("%s,%08lX\n", payload, static_cast<unsigned long>(crc));

    pairingState_ = GrovePairingState::InProgress;
    pairingStartMs_ = millis();
    return true;
}

void GroveCompanionLink::processPairingResponse(char* fields) {
    // fields: "<seq>,<pubkey_hex_64>,<unix_time>"
    if (pairingState_ != GrovePairingState::InProgress || !pairingCtxActive_) {
        return;
    }
    char* tokens[kMaxFields];
    if (splitFields(fields, tokens) != 3) return;
    const uint32_t sequence = strtoul(tokens[0], nullptr, 10);
    if (sequence != pairingSequence_) return;  // stale/unrelated response

    uint8_t peerPublic[32];
    if (!hexDecode(peerPublic, sizeof(peerPublic), tokens[1])) {
        pairingState_ = GrovePairingState::Failed;
        mbedtls_ecdh_free(&pairingCtx_);
        pairingCtxActive_ = false;
        return;
    }

    // Re-wrap in the TLS ECPoint format read_public() expects -- see the
    // matching comment in beginPairing().
    uint8_t peerPublicTls[33];
    peerPublicTls[0] = sizeof(peerPublic);
    memcpy(peerPublicTls + 1, peerPublic, sizeof(peerPublic));

    uint8_t sharedSecret[32];
    size_t sharedLen = 0;
    bool ok = mbedtls_ecdh_read_public(&pairingCtx_, peerPublicTls,
                                       sizeof(peerPublicTls)) == 0;
    if (ok) {
        ok = mbedtls_ecdh_calc_secret(&pairingCtx_, &sharedLen, sharedSecret,
                                      sizeof(sharedSecret),
                                      mbedtls_ctr_drbg_random, &ctrDrbg_) == 0 &&
             sharedLen == sizeof(sharedSecret);
    }
    uint8_t derivedKey[GHOSTWIRE_AUTH_KEY_BYTES];
    if (ok) {
        ok = hkdfSha256(
                 reinterpret_cast<const uint8_t*>(GHOSTWIRE_AUTH_HKDF_SALT),
                 strlen(GHOSTWIRE_AUTH_HKDF_SALT), sharedSecret, sharedLen,
                 reinterpret_cast<const uint8_t*>(GHOSTWIRE_AUTH_HKDF_INFO),
                 strlen(GHOSTWIRE_AUTH_HKDF_INFO), derivedKey,
                 sizeof(derivedKey));
    }
    mbedtls_ecdh_free(&pairingCtx_);
    pairingCtxActive_ = false;

    if (!ok) {
        pairingState_ = GrovePairingState::Failed;
        return;
    }
    memcpy(sessionKey_, derivedKey, sizeof(sessionKey_));
    sessionKeyValid_ = true;
    pairingState_ = GrovePairingState::Success;

    // See kMinPlausibleUnixTime above -- an unsynced P4 clock means commands
    // stay unavailable (hasClockSync() false) even though pairing itself
    // succeeded, rather than silently computing bogus auth windows.
    const unsigned long long p4UnixTime = strtoull(tokens[2], nullptr, 10);
    if (p4UnixTime >= kMinPlausibleUnixTime) {
        clockOffsetSeconds_ = static_cast<int64_t>(p4UnixTime) -
                              static_cast<int64_t>(millis() / 1000);
        clockOffsetValid_ = true;
        Serial.printf("[grove] pairing clock offset set (%lld)\n",
                      static_cast<long long>(clockOffsetSeconds_));
    } else {
        clockOffsetValid_ = false;
        Serial.println("[grove] pairing succeeded but the P4's clock isn't "
                       "synced yet; commands unavailable until re-pairing");
    }
}

int64_t GroveCompanionLink::currentUnixTime() const {
    return clockOffsetSeconds_ + static_cast<int64_t>(millis() / 1000);
}

bool GroveCompanionLink::computeAuthTag(const uint8_t* message, size_t messageLen,
                                        int64_t nowUtc,
                                        uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const {
    if (!sessionKeyValid_) return false;
    const int64_t window = nowUtc / GHOSTWIRE_AUTH_WINDOW_SECONDS;
    uint8_t buffer[64];
    if (messageLen + 8 > sizeof(buffer)) return false;
    memcpy(buffer, message, messageLen);
    for (int i = 0; i < 8; ++i) {
        buffer[messageLen + i] = static_cast<uint8_t>(window >> (56 - i * 8));
    }
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md == nullptr) return false;
    uint8_t fullTag[32];
    if (mbedtls_md_hmac(md, sessionKey_, sizeof(sessionKey_), buffer,
                        messageLen + 8, fullTag) != 0) {
        return false;
    }
    memcpy(tagOut, fullTag, GHOSTWIRE_AUTH_TAG_BYTES);
    return true;
}

bool GroveCompanionLink::sendCommand(uint8_t slot) {
    if (!sessionKeyValid_ || !clockOffsetValid_ || slot > 1) return false;

    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    if (!computeAuthTag(&slot, 1, currentUnixTime(), tag)) return false;
    char tagHex[GHOSTWIRE_AUTH_TAG_BYTES * 2 + 1];
    hexEncode(tagHex, tag, GHOSTWIRE_AUTH_TAG_BYTES);

    ++commandSequence_;
    char payload[GHOSTWIRE_GROVE_MAX_LINE];
    const int length = snprintf(payload, sizeof(payload), "GW1,C,%lu,%u,%s",
                                static_cast<unsigned long>(commandSequence_),
                                static_cast<unsigned>(slot), tagHex);
    const uint32_t crc = ghostwire_grove_crc32(payload, length);
    serial_.printf("%s,%08lX\n", payload, static_cast<unsigned long>(crc));

    commandAckState_ = GroveCommandAckState::Pending;
    commandStartMs_ = millis();
    return true;
}

void GroveCompanionLink::processCommandAck(char* fields) {
    // fields: "<seq>,<accepted>"
    if (commandAckState_ != GroveCommandAckState::Pending) return;
    char* tokens[kMaxFields];
    if (splitFields(fields, tokens) != 2) return;
    const uint32_t sequence = strtoul(tokens[0], nullptr, 10);
    if (sequence != commandSequence_) return;  // stale/unrelated ack

    commandAckState_ = atoi(tokens[1]) != 0 ? GroveCommandAckState::Accepted
                                            : GroveCommandAckState::Rejected;
}

void GroveCompanionLink::loadSessionKey(const uint8_t key[GHOSTWIRE_AUTH_KEY_BYTES]) {
    memcpy(sessionKey_, key, sizeof(sessionKey_));
    sessionKeyValid_ = true;
}

void GroveCompanionLink::forgetSessionKey() {
    memset(sessionKey_, 0, sizeof(sessionKey_));
    sessionKeyValid_ = false;
    clockOffsetValid_ = false;
    clockOffsetSeconds_ = 0;
    pairingState_ = GrovePairingState::Idle;
}

bool GroveCompanionLink::buildWifiCommandTag(uint8_t slot, uint32_t nonce,
                                             uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const {
    if (!clockOffsetValid_) return false;
    uint8_t message[5] = {
        slot,
        static_cast<uint8_t>(nonce >> 24),
        static_cast<uint8_t>(nonce >> 16),
        static_cast<uint8_t>(nonce >> 8),
        static_cast<uint8_t>(nonce),
    };
    return computeAuthTag(message, sizeof(message), currentUnixTime(), tagOut);
}

bool GroveCompanionLink::buildWifiScriptTag(uint8_t slot, uint32_t nonce, const String& script,
                                            uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const {
    if (!clockOffsetValid_) return false;
    uint8_t scriptHash[32];
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md == nullptr ||
        mbedtls_md(md, reinterpret_cast<const unsigned char*>(script.c_str()), script.length(),
                  scriptHash) != 0) {
        return false;
    }
    uint8_t message[1 + 4 + sizeof(scriptHash)];
    message[0] = slot;
    message[1] = static_cast<uint8_t>(nonce >> 24);
    message[2] = static_cast<uint8_t>(nonce >> 16);
    message[3] = static_cast<uint8_t>(nonce >> 8);
    message[4] = static_cast<uint8_t>(nonce);
    memcpy(message + 5, scriptHash, sizeof(scriptHash));
    return computeAuthTag(message, sizeof(message), currentUnixTime(), tagOut);
}

bool GroveCompanionLink::buildWifiLootTag(uint32_t nonce,
                                          uint8_t tagOut[GHOSTWIRE_AUTH_TAG_BYTES]) const {
    if (!clockOffsetValid_) return false;
    const uint8_t message[4] = {
        static_cast<uint8_t>(nonce >> 24),
        static_cast<uint8_t>(nonce >> 16),
        static_cast<uint8_t>(nonce >> 8),
        static_cast<uint8_t>(nonce),
    };
    return computeAuthTag(message, sizeof(message), currentUnixTime(), tagOut);
}

bool GroveCompanionLink::readGroveLine(String& outLine, unsigned long timeoutMs) {
    const unsigned long deadline = millis() + timeoutMs;
    outLine = "";
    while (millis() < deadline) {
        if (serial_.available() > 0) {
            const char value = static_cast<char>(serial_.read());
            if (value == '\n') return true;
            if (value != '\r' && outLine.length() < 200) outLine += value;
        } else {
            delay(1);
        }
    }
    return false;
}

bool GroveCompanionLink::waitForGroveFrame(char type, uint32_t sequence, String& fields,
                                           unsigned long timeoutMs) {
    const unsigned long deadline = millis() + timeoutMs;
    while (true) {
        const unsigned long now = millis();
        if (now >= deadline) return false;
        String line;
        if (!readGroveLine(line, deadline - now)) return false;
        if (!line.startsWith("GW1,") || line.length() < 6) continue;
        const int crcSeparator = line.lastIndexOf(',');
        if (crcSeparator < 6) continue;
        const uint32_t receivedCrc =
            strtoul(line.c_str() + crcSeparator + 1, nullptr, 16);
        if (receivedCrc != ghostwire_grove_crc32(line.c_str(), static_cast<size_t>(crcSeparator))) {
            ++crcErrors_;
            continue;
        }
        if (line.charAt(4) != type) continue;
        const String rest = line.substring(6, crcSeparator);  // "<seq>,<field1>,..."
        const int firstComma = rest.indexOf(',');
        const uint32_t gotSequence = static_cast<uint32_t>(
            (firstComma < 0 ? rest : rest.substring(0, firstComma)).toInt());
        if (gotSequence != sequence) continue;
        fields = firstComma < 0 ? String("") : rest.substring(firstComma + 1);
        return true;
    }
}

bool GroveCompanionLink::sendScript(uint8_t slot, const String& script) {
    if (!sessionKeyValid_ || !clockOffsetValid_ || slot > 1) return false;
    if (script.length() == 0 || script.length() >= kPayloadScriptMaxBytes) return false;
    // update()'s own line_/lineLength_ accumulator may hold an in-progress
    // partial line right now -- discard it rather than let update() splice
    // stale prefix bytes onto whatever arrives after this call returns.
    lineLength_ = 0;

    const uint32_t sequence = ++groveExchangeSequence_;
    char beginPayload[GHOSTWIRE_GROVE_MAX_LINE];
    const int beginLength = snprintf(
        beginPayload, sizeof(beginPayload), "GW1,U,%lu,%u,%u",
        static_cast<unsigned long>(sequence), static_cast<unsigned>(slot),
        static_cast<unsigned>(script.length()));
    const uint32_t beginCrc = ghostwire_grove_crc32(beginPayload, beginLength);
    serial_.printf("%s,%08lX\n", beginPayload, static_cast<unsigned long>(beginCrc));

    String beginFields;
    if (!waitForGroveFrame('V', sequence, beginFields, 3000) || beginFields != "1") {
        return false;
    }

    // 28 raw bytes/chunk keeps every "D" frame safely under
    // GHOSTWIRE_GROVE_MAX_LINE once hex-encoded ("GW1,D,<seq>,<56 hex
    // chars>,<crc32>\n") -- a full kPayloadScriptMaxBytes upload is ~19
    // chunks, a quick burst on a link this session already measured as
    // clean/reliable.
    constexpr size_t kChunkBytes = 28;
    for (size_t offset = 0; offset < script.length(); offset += kChunkBytes) {
        const size_t chunkLen = std::min(kChunkBytes, script.length() - offset);
        char chunkHex[kChunkBytes * 2 + 1];
        hexEncode(chunkHex, reinterpret_cast<const uint8_t*>(script.c_str() + offset), chunkLen);
        char chunkPayload[GHOSTWIRE_GROVE_MAX_LINE];
        const int chunkPayloadLength = snprintf(
            chunkPayload, sizeof(chunkPayload), "GW1,D,%lu,%s",
            static_cast<unsigned long>(sequence), chunkHex);
        const uint32_t chunkCrc = ghostwire_grove_crc32(chunkPayload, chunkPayloadLength);
        serial_.printf("%s,%08lX\n", chunkPayload, static_cast<unsigned long>(chunkCrc));
    }

    const uint32_t nonce = esp_random();
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    if (!buildWifiScriptTag(slot, nonce, script, tag)) return false;
    char tagHex[GHOSTWIRE_AUTH_TAG_BYTES * 2 + 1];
    hexEncode(tagHex, tag, GHOSTWIRE_AUTH_TAG_BYTES);

    char finishPayload[GHOSTWIRE_GROVE_MAX_LINE];
    const int finishLength = snprintf(
        finishPayload, sizeof(finishPayload), "GW1,F,%lu,%lu,%s",
        static_cast<unsigned long>(sequence), static_cast<unsigned long>(nonce), tagHex);
    const uint32_t finishCrc = ghostwire_grove_crc32(finishPayload, finishLength);
    serial_.printf("%s,%08lX\n", finishPayload, static_cast<unsigned long>(finishCrc));

    String finishFields;
    // Hashing + NVS commit on the P4 can take a moment; more generous than
    // the begin-ack timeout above.
    if (!waitForGroveFrame('K', sequence, finishFields, 5000)) return false;
    return finishFields == "1";
}

bool GroveCompanionLink::requestLoot(std::vector<LootEntry>& out) {
    out.clear();
    if (!sessionKeyValid_ || !clockOffsetValid_) return false;
    lineLength_ = 0;  // see the matching comment in sendScript()

    const uint32_t sequence = ++groveExchangeSequence_;
    const uint32_t nonce = esp_random();
    uint8_t tag[GHOSTWIRE_AUTH_TAG_BYTES];
    if (!buildWifiLootTag(nonce, tag)) return false;
    char tagHex[GHOSTWIRE_AUTH_TAG_BYTES * 2 + 1];
    hexEncode(tagHex, tag, GHOSTWIRE_AUTH_TAG_BYTES);

    char requestPayload[GHOSTWIRE_GROVE_MAX_LINE];
    const int requestLength = snprintf(
        requestPayload, sizeof(requestPayload), "GW1,X,%lu,%lu,%s",
        static_cast<unsigned long>(sequence), static_cast<unsigned long>(nonce), tagHex);
    const uint32_t requestCrc = ghostwire_grove_crc32(requestPayload, requestLength);
    serial_.printf("%s,%08lX\n", requestPayload, static_cast<unsigned long>(requestCrc));

    String countFields;
    if (!waitForGroveFrame('N', sequence, countFields, 3000)) return false;
    const int entryCount = countFields.toInt();
    if (entryCount <= 0) return entryCount == 0;  // 0 is a valid (empty) result

    for (int i = 0; i < entryCount; ++i) {
        String entryFields;
        if (!waitForGroveFrame('E', sequence, entryFields, 2000)) return false;
        const int comma = entryFields.indexOf(',');
        if (comma < 0) return false;
        LootEntry entry;
        entry.ip = entryFields.substring(0, comma);
        entry.port = static_cast<uint16_t>(entryFields.substring(comma + 1).toInt());
        out.push_back(entry);
    }
    return true;
}

void GroveCompanionLink::processLine() {
    line_[lineLength_] = '\0';
    char* crcSeparator = strrchr(line_, ',');
    // "GW1,<type>,<fields...>,<crc>" -- type is a single char at index 4,
    // fields start at index 6.
    if (crcSeparator == nullptr || strncmp(line_, "GW1,", 4) != 0 ||
        line_[5] != ',') {
        return;
    }
    char* crcEnd = nullptr;
    const uint32_t receivedCrc = strtoul(crcSeparator + 1, &crcEnd, 16);
    if (crcEnd == crcSeparator + 1 || *crcEnd != '\0' ||
        receivedCrc != ghostwire_grove_crc32(line_, crcSeparator - line_)) {
        ++crcErrors_;
        return;
    }
    *crcSeparator = '\0';  // fields end where the CRC used to start

    const char frameType = line_[4];
    char* fields = line_ + 6;
    switch (frameType) {
        case 'H': processHeartbeat(fields); break;
        case 'S': processStatus(fields); break;
        case 'I': processIdentity(fields); break;
        case 'Q': processPairingResponse(fields); break;
        case 'K': processCommandAck(fields); break;
        default: break;  // unknown frame type; ignore (forward-compatible)
    }
}

void GroveCompanionLink::update() {
    while (serial_.available() > 0) {
        const char value = static_cast<char>(serial_.read());
        if (value == '\n') {
            if (lineLength_ > 0) processLine();
            lineLength_ = 0;
        } else if (value != '\r') {
            if (lineLength_ < sizeof(line_) - 1) {
                line_[lineLength_++] = value;
            } else {
                lineLength_ = 0;
                ++crcErrors_;
            }
        }
    }
    if (pairingState_ == GrovePairingState::InProgress &&
        millis() - pairingStartMs_ > kPairingTimeoutMs) {
        pairingState_ = GrovePairingState::Failed;
        if (pairingCtxActive_) {
            mbedtls_ecdh_free(&pairingCtx_);
            pairingCtxActive_ = false;
        }
    }
    if (commandAckState_ == GroveCommandAckState::Pending &&
        millis() - commandStartMs_ > kCommandTimeoutMs) {
        commandAckState_ = GroveCommandAckState::Rejected;
    }
}
