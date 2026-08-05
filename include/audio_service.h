#pragma once

#include <Arduino.h>

class AudioFileSourceSD;
class AudioFileSourceID3;
class AudioGeneratorMP3;
class GhostwireAudioOutput;

class AudioService {
public:
    void setVolume(uint8_t volume);
    void playToneTest();
    bool beginMicrophone();
    bool updateMicrophone(uint16_t& level);
    void endMicrophone();
    bool recordWav(const char* path, uint32_t durationMs,
                   String& error);

    bool startMp3(const char* path, uint16_t settleDelayMs = 150);
    void stopPlayback();
    bool isPlaying() const;

private:
    static void playbackTaskEntry(void* context);
    void playbackTask();
    void releasePlayback();

    static constexpr size_t kMicSamples = 256;
    int16_t micSamples_[kMicSamples]{};
    bool microphoneActive_ = false;
    uint8_t volume_ = 96;
    volatile bool playing_ = false;
    volatile bool stopRequested_ = false;
    TaskHandle_t playbackTaskHandle_ = nullptr;
    AudioFileSourceSD* file_ = nullptr;
    AudioFileSourceID3* id3_ = nullptr;
    AudioGeneratorMP3* decoder_ = nullptr;
    GhostwireAudioOutput* output_ = nullptr;
};
