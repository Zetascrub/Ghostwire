#pragma once

#include <Arduino.h>
#include <vector>

// Audio Self-Test menu, Familiar Phrase Lab, Microphone level, MP3 file
// browser, and the shared "Now Playing" screen (used by three different
// playback start points -- see docs/screen-extraction.md). Grouped as one
// module since they're all reached from the Audio tool and share the list
// cursor. Playback control, file loading, and mic sampling stay in
// main.cpp.
class AudioScreens {
public:
    AudioScreens(size_t& listSelection, size_t& listOffset,
                uint8_t& ttsLabPhrase, String& ttsLabStatus,
                uint32_t& ttsLabPlaybackMs, uint16_t& microphoneLevel,
                std::vector<String>& audioFiles)
        : listSelection_(listSelection),
          listOffset_(listOffset),
          ttsLabPhrase_(ttsLabPhrase),
          ttsLabStatus_(ttsLabStatus),
          ttsLabPlaybackMs_(ttsLabPlaybackMs),
          microphoneLevel_(microphoneLevel),
          audioFiles_(audioFiles) {}

    void drawMenu();
    void drawTtsLab();
    void drawMicrophone();
    // Partial redraw for the microphone level meter's periodic tick.
    void updateMicrophoneMeter();
    void drawAudioFiles();

    // Shared "Now Playing" screen: used when starting playback from the MP3
    // browser, the general Files browser, or an AI text-to-speech reply.
    void drawNowPlaying(const String& name, const String& source);

private:
    size_t& listSelection_;
    size_t& listOffset_;
    uint8_t& ttsLabPhrase_;
    String& ttsLabStatus_;
    uint32_t& ttsLabPlaybackMs_;
    uint16_t& microphoneLevel_;
    std::vector<String>& audioFiles_;
};
