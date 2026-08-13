#pragma once

#include <Arduino.h>
#include <FS.h>

// Binary pcap (libpcap classic format) writer, streaming one record at a
// time straight to SD -- never buffers a whole capture in RAM. Sibling to
// SdLogger rather than an extension of it: SdLogger hardcodes a .csv
// extension and an unconditional text header line, both wrong for binary
// pcap, but the lifecycle/accessor shape intentionally matches it.
class PcapLogger {
public:
    // See SdLogger::begin()'s matching parameter -- same "own subfolder
    // under /ghostwire/, default 'logs' for backward compatibility" idea.
    bool begin(const char* streamName, const char* subdir = "logs");
    bool append(const uint8_t* frame, size_t length, uint32_t timestampSec,
               uint32_t timestampUsec);
    void update();
    void stop();

    bool isActive() const { return active_; }
    uint32_t rowCount() const { return frameCount_; }
    uint32_t errorCount() const { return errorCount_; }
    uint64_t byteCount() const { return byteCount_; }
    const String& path() const { return path_; }
    const String& status() const { return status_; }

private:
    File file_;
    bool active_ = false;
    uint32_t frameCount_ = 0;
    uint32_t errorCount_ = 0;
    uint32_t lastFlushMs_ = 0;
    uint64_t byteCount_ = 0;
    String path_;
    String status_ = "Ready";
};
