#pragma once

#include <Arduino.h>

// One entry from the P4's accumulated scan-loot log (shared/protocol/
// README.md's "Loot extraction" section). Used by both GroveCompanionLink
// and PoeCompanionService so callers (main.cpp's extractPoeLoot()) get the
// same type regardless of which transport actually served the request.
struct LootEntry {
    String ip;
    uint16_t port = 0;
};
