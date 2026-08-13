#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define GHOSTWIRE_GROVE_BAUD 115200
#define GHOSTWIRE_GROVE_MAX_LINE 96

// Frame cadence: heartbeat (H) and status (S) go out once per second.
// Identity (I) is sent every Nth cycle since device id/firmware rarely
// change and the strings are long relative to the other frames.
#define GHOSTWIRE_GROVE_IDENTITY_EVERY_N_CYCLES 10

// A receiver treats status/identity as stale after this long without a new,
// valid frame of that type -- independent of the heartbeat/ack link timeout,
// since status/identity are one-way pushes with no ack to track.
#define GHOSTWIRE_GROVE_STATUS_TIMEOUT_MS 3000
#define GHOSTWIRE_GROVE_IDENTITY_TIMEOUT_MS 15000

// Sentinel for "no temperature reading" in the status frame's temp_x10
// field (tenths of a degree C).
#define GHOSTWIRE_GROVE_NO_TEMPERATURE -9999

static inline uint32_t ghostwire_grove_crc32(const char *data, size_t length)
{
    uint32_t crc = 0xffffffffU;
    for (size_t index = 0; index < length; ++index) {
        crc ^= (uint8_t)data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return ~crc;
}

// Single-char codes for the status frame's indicator/reset-reason fields,
// defined once so the P4's encoder and the Cardputer's decoder can't drift
// apart. Names match the strings poe-p4's build_status_json() already sends
// over HTTP, so Grove- and network-sourced telemetry read identically.
static inline char ghostwire_grove_indicator_code(const char *name)
{
    if (!name) return 'R';
    if (!strcmp(name, "booting")) return 'B';
    if (!strcmp(name, "fault")) return 'F';
    if (!strcmp(name, "lan")) return 'L';
    if (!strcmp(name, "internet")) return 'N';
    if (!strcmp(name, "ghostwire")) return 'G';
    return 'R';  // "ready" and any unrecognized name
}

static inline const char *ghostwire_grove_indicator_name(char code)
{
    switch (code) {
        case 'B': return "booting";
        case 'F': return "fault";
        case 'L': return "lan";
        case 'N': return "internet";
        case 'G': return "ghostwire";
        default: return "ready";
    }
}

static inline char ghostwire_grove_reset_code(const char *reason)
{
    if (!reason) return 'U';
    if (!strcmp(reason, "power_on")) return 'P';
    if (!strcmp(reason, "software")) return 'S';
    if (!strcmp(reason, "panic")) return 'X';
    if (!strcmp(reason, "interrupt_watchdog")) return 'I';
    if (!strcmp(reason, "task_watchdog")) return 'T';
    if (!strcmp(reason, "watchdog")) return 'W';
    if (!strcmp(reason, "deep_sleep")) return 'D';
    if (!strcmp(reason, "brownout")) return 'O';
    return 'U';  // "unknown" and any unrecognized reason
}

static inline const char *ghostwire_grove_reset_name(char code)
{
    switch (code) {
        case 'P': return "power_on";
        case 'S': return "software";
        case 'X': return "panic";
        case 'I': return "interrupt_watchdog";
        case 'T': return "task_watchdog";
        case 'W': return "watchdog";
        case 'D': return "deep_sleep";
        case 'O': return "brownout";
        default: return "unknown";
    }
}
