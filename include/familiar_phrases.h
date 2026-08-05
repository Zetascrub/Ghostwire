#pragma once

#include <Arduino.h>
#include <cstdint>

// Word-bank phrases used by the Familiar Phrase Lab (Screen::TtsLab) and by
// the Familiar Voice pack's patrol-event cues (see cyber_familiar.cpp).
// Header-only shared constant (see docs/screen-extraction.md) since both
// main.cpp and audio_screens.cpp need it and it's plain compile-time data.
struct FamiliarPhrase {
    const char* name;
    const char* words[4];
    uint8_t wordCount;
};

inline constexpr FamiliarPhrase kTtsLabPhrases[] = {
    {"New host discovered", {"new", "host", "discovered"}, 3},
    {"New host detected", {"new", "host", "detected"}, 3},
    {"Interesting service found", {"interesting", "service", "found"}, 3},
    {"Patrol started", {"patrol", "started"}, 2},
    {"Patrol completed", {"patrol", "completed"}, 2},
    {"Warning: service detected", {"warning", "service", "detected"}, 3},
    {"Error", {"error"}, 1},
};
inline constexpr size_t kTtsLabPhraseCount =
    sizeof(kTtsLabPhrases) / sizeof(kTtsLabPhrases[0]);
