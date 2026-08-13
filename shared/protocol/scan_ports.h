#pragma once

#include <stddef.h>
#include <stdint.h>

// The "13 common ports" quick-scan list, shared so the Cardputer's Port Scan
// screen and the P4's button-triggered scan payload mean the same thing.
static const uint16_t GHOSTWIRE_COMMON_PORTS[] = {21,  22,  23,  25,   53,  80,
                                                   110, 139, 143, 443, 445, 3389,
                                                   8080};
#define GHOSTWIRE_COMMON_PORT_COUNT \
    (sizeof(GHOSTWIRE_COMMON_PORTS) / sizeof(GHOSTWIRE_COMMON_PORTS[0]))
