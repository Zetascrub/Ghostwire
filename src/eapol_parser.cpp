#include "eapol_parser.h"

#include <cstring>

bool EapolParser::parse(const uint8_t* data, size_t length, EapolInfo& info) {
    info = EapolInfo{};
    if (length < 4 || data[1] != 3) return false;  // Not an EAPOL-Key frame.

    // Fixed EAPOL-Key body layout up to key data: 4 (EAPOL header) + 1
    // (descriptor type) + 2 (key info) + 2 (key length) + 8 (replay
    // counter) + 32 (nonce) + 16 (IV) + 8 (RSC) + 8 (reserved) + 16 (MIC) +
    // 2 (key data length) = 99 bytes.
    constexpr size_t kKeyDataStart = 99;
    if (length < kKeyDataStart) return false;

    const uint16_t keyInfo =
        (static_cast<uint16_t>(data[5]) << 8) | data[6];
    const bool ack = keyInfo & 0x0080;
    const bool mic = keyInfo & 0x0100;
    const bool secure = keyInfo & 0x0200;

    if (ack && !mic) {
        info.messageNumber = 1;
    } else if (!ack && mic && !secure) {
        info.messageNumber = 2;
    } else if (ack && mic && secure) {
        info.messageNumber = 3;
    } else if (!ack && mic && secure) {
        info.messageNumber = 4;
    }

    const uint16_t keyDataLength =
        (static_cast<uint16_t>(data[97]) << 8) | data[98];
    if (keyDataLength == 0 || kKeyDataStart + keyDataLength > length) {
        return true;  // Message number known; no usable key data.
    }

    // PMKID vendor KDE (only meaningful in Message 1): tag 0xDD, length
    // 0x14, OUI 00:0F:AC, OUI type 4, then a 16-byte PMKID.
    if (info.messageNumber == 1) {
        size_t offset = kKeyDataStart;
        const size_t end = kKeyDataStart + keyDataLength;
        while (offset + 2 <= end) {
            const uint8_t tag = data[offset];
            const uint8_t tagLength = data[offset + 1];
            if (offset + 2 + tagLength > end) break;
            if (tag == 0xDD && tagLength == 0x14 && data[offset + 2] == 0x00 &&
                data[offset + 3] == 0x0F && data[offset + 4] == 0xAC &&
                data[offset + 5] == 0x04) {
                memcpy(info.pmkid, data + offset + 6, 16);
                info.hasPmkid = true;
                break;
            }
            offset += 2 + tagLength;
        }
    }
    return true;
}
