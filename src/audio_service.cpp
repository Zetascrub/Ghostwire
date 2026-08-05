#include "audio_service.h"

#include <AudioFileSourceID3.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <algorithm>

class GhostwireAudioOutput : public AudioOutput {
public:
    GhostwireAudioOutput(m5::Speaker_Class* speaker,
                         volatile bool* stopRequested)
        : speaker_(speaker), stopRequested_(stopRequested) {}

    bool begin() override { return true; }

    void setSettleDelayMs(uint16_t delayMs) { settleDelayMs_ = delayMs; }

    bool ConsumeSample(int16_t sample[2]) override {
        // ESP8266Audio only guarantees the left sample for mono sources.
        // Normalize it to stereo before sending interleaved PCM to M5Unified;
        // otherwise the uninitialized right sample is heard as a loud tone.
        MakeSampleStereo16(sample);
        if (used_ + 2 > kBufferSamples) {
            flush();
        }
        buffers_[buffer_][used_++] = sample[0];
        buffers_[buffer_][used_++] = sample[1];
        return true;
    }

    void flush() override {
        if (used_ == 0) return;
        // M5Unified accepts audio asynchronously. Pace the decoder to the
        // speaker instead of filling its queue with the rest of the track.
        while (speaker_->isPlaying(0) && !*stopRequested_) {
            vTaskDelay(1);
        }
        if (*stopRequested_) {
            used_ = 0;
            return;
        }
        speaker_->playRaw(buffers_[buffer_], used_, hertz, true, 1, 0);
        buffer_ = (buffer_ + 1) % 3;
        used_ = 0;
    }

    bool stop() override {
        used_ = 0;
        speaker_->stop(0);
        // stop() posts an asynchronous command to M5Unified's speaker task.
        // Do not let a new decoder reuse this channel until that task has
        // actually acknowledged the stop and released the current buffer.
        const uint32_t waitStarted = millis();
        while (speaker_->isPlaying(0) && millis() - waitStarted < 2000) {
            vTaskDelay(1);
        }
        // Ordinary playback keeps a generous settling interval. Word-bank
        // phrases can request a shorter interval for more natural pacing.
        if (settleDelayMs_ > 0) vTaskDelay(pdMS_TO_TICKS(settleDelayMs_));
        return true;
    }

private:
    static constexpr size_t kBufferSamples = 2048;
    m5::Speaker_Class* speaker_;
    volatile bool* stopRequested_;
    int16_t buffers_[3][kBufferSamples]{};
    size_t buffer_ = 0;
    size_t used_ = 0;
    uint16_t settleDelayMs_ = 150;
};

void AudioService::setVolume(uint8_t volume) {
    volume_ = volume;
    M5Cardputer.Speaker.setVolume(volume_);
}

void AudioService::playToneTest() {
    endMicrophone();
    stopPlayback();
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(volume_);
    const uint16_t notes[] = {440, 660, 880};
    for (uint16_t note : notes) {
        M5Cardputer.Speaker.tone(note, 180);
        delay(230);
    }
}

bool AudioService::beginMicrophone() {
    stopPlayback();
    M5Cardputer.Speaker.end();
    microphoneActive_ = M5Cardputer.Mic.begin();
    return microphoneActive_;
}

bool AudioService::updateMicrophone(uint16_t& level) {
    if (!microphoneActive_) return false;
    if (!M5Cardputer.Mic.record(micSamples_, kMicSamples, 17000)) return false;
    while (M5Cardputer.Mic.isRecording()) {
        delay(1);
    }
    uint32_t peak = 0;
    for (int16_t sample : micSamples_) {
        peak = std::max<uint32_t>(
            peak, abs(static_cast<int32_t>(sample)));
    }
    level = std::min<uint32_t>(100, peak / 64);
    return true;
}

void AudioService::endMicrophone() {
    if (!microphoneActive_) return;
    M5Cardputer.Mic.end();
    microphoneActive_ = false;
}

bool AudioService::recordWav(const char* path, uint32_t durationMs,
                             String& error) {
    constexpr uint32_t sampleRate = 16000;
    constexpr uint16_t channels = 1;
    constexpr uint16_t bitsPerSample = 16;
    stopPlayback();
    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.end();
    if (!M5Cardputer.Mic.begin()) {
        error = "Microphone unavailable";
        return false;
    }
    File output = SD.open(path, FILE_WRITE);
    if (!output) {
        M5Cardputer.Mic.end();
        error = "Cannot create recording";
        return false;
    }

    uint8_t header[44]{};
    memcpy(header, "RIFF", 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    const uint32_t fmtSize = 16;
    const uint16_t pcm = 1;
    const uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    const uint16_t blockAlign = channels * bitsPerSample / 8;
    memcpy(header + 16, &fmtSize, 4);
    memcpy(header + 20, &pcm, 2);
    memcpy(header + 22, &channels, 2);
    memcpy(header + 24, &sampleRate, 4);
    memcpy(header + 28, &byteRate, 4);
    memcpy(header + 32, &blockAlign, 2);
    memcpy(header + 34, &bitsPerSample, 2);
    memcpy(header + 36, "data", 4);
    output.write(header, sizeof(header));

    const uint32_t targetSamples = sampleRate * durationMs / 1000;
    uint32_t writtenSamples = 0;
    bool cancelled = false;
    while (writtenSamples < targetSamples) {
        const size_t count = std::min<size_t>(
            kMicSamples, targetSamples - writtenSamples);
        if (!M5Cardputer.Mic.record(micSamples_, count, sampleRate)) {
            error = "Microphone capture failed";
            break;
        }
        while (M5Cardputer.Mic.isRecording()) delay(1);
        output.write(reinterpret_cast<uint8_t*>(micSamples_), count * 2);
        writtenSamples += count;
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() &&
            M5Cardputer.Keyboard.isPressed() &&
            M5Cardputer.Keyboard.keysState().esc) {
            cancelled = true;
            break;
        }
    }
    M5Cardputer.Mic.end();

    const uint32_t dataSize = writtenSamples * 2;
    const uint32_t riffSize = 36 + dataSize;
    output.seek(4);
    output.write(reinterpret_cast<const uint8_t*>(&riffSize), 4);
    output.seek(40);
    output.write(reinterpret_cast<const uint8_t*>(&dataSize), 4);
    output.close();
    if (!error.isEmpty()) return false;
    if (cancelled) {
        error = "Recording cancelled";
        return false;
    }
    return writtenSamples > 0;
}

bool AudioService::startMp3(const char* path, uint16_t settleDelayMs) {
    endMicrophone();
    stopPlayback();
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(volume_);

    file_ = new AudioFileSourceSD(path);
    if (file_ == nullptr || !file_->isOpen()) {
        stopPlayback();
        return false;
    }
    id3_ = new AudioFileSourceID3(file_);
    // Keep the PCM buffers alive for the lifetime of the firmware. The
    // speaker worker consumes them asynchronously and rapid replay must not
    // replace their storage while it is settling from the previous track.
    if (output_ == nullptr) {
        output_ =
            new GhostwireAudioOutput(&M5Cardputer.Speaker, &stopRequested_);
    }
    output_->setSettleDelayMs(settleDelayMs);
    decoder_ = new AudioGeneratorMP3();
    if (!decoder_->begin(id3_, output_)) {
        releasePlayback();
        return false;
    }

    if (playbackTaskHandle_ == nullptr) {
        const BaseType_t taskCreated = xTaskCreatePinnedToCore(
            playbackTaskEntry, "ghostwire_audio", 10240, this, 2,
            &playbackTaskHandle_, 0);
        if (taskCreated != pdPASS) {
            releasePlayback();
            return false;
        }
    }
    stopRequested_ = false;
    playing_ = true;
    return true;
}

void AudioService::playbackTaskEntry(void* context) {
    static_cast<AudioService*>(context)->playbackTask();
}

void AudioService::playbackTask() {
    for (;;) {
        if (!playing_) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        while (!stopRequested_ && decoder_ != nullptr &&
               decoder_->isRunning()) {
            if (!decoder_->loop()) break;
            taskYIELD();
        }

        releasePlayback();
        playing_ = false;
    }
}

void AudioService::stopPlayback() {
    if (playing_) {
        stopRequested_ = true;
        M5Cardputer.Speaker.stop(0);
        // The persistent worker owns teardown. Do not permit another track to
        // be created until it has completed the previous track's cleanup.
        while (playing_) {
            delay(1);
        }
    }
}

void AudioService::releasePlayback() {
    if (decoder_ != nullptr) decoder_->stop();
    if (output_ != nullptr) output_->stop();
    if (id3_ != nullptr) id3_->close();
    if (file_ != nullptr) file_->close();
    delete decoder_;
    delete id3_;
    delete file_;
    decoder_ = nullptr;
    id3_ = nullptr;
    file_ = nullptr;
    stopRequested_ = false;
}

bool AudioService::isPlaying() const {
    return playing_;
}
