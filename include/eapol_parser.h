#pragma once

#include <cstddef>
#include <cstdint>

struct EapolInfo {
    uint8_t messageNumber = 0;  // 1-4 (WPA2 4-way handshake), or 0 if unclear.
    bool hasPmkid = false;
    uint8_t pmkid[16] = {};
};

// Parses an already-isolated EAPOL-Key frame -- the 802.11 header, LLC/SNAP,
// and EtherType have already been stripped by the caller, so `data` points
// at the EAPOL header's version byte. Stateless, mirroring MeshtasticDecoder.
class EapolParser {
public:
    static bool parse(const uint8_t* data, size_t length, EapolInfo& info);
};
