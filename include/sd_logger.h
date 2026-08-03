#pragma once

#include <Arduino.h>
#include <FS.h>

class SdLogger {
public:
    bool begin(const char* streamName, const char* csvHeader);
    bool append(const String& line);
    void update();
    void stop();

    bool isActive() const { return active_; }
    uint32_t rowCount() const { return rowCount_; }
    uint32_t errorCount() const { return errorCount_; }
    const String& path() const { return path_; }
    const String& status() const { return status_; }

private:
    File file_;
    bool active_ = false;
    uint32_t rowCount_ = 0;
    uint32_t errorCount_ = 0;
    uint32_t lastFlushMs_ = 0;
    String path_;
    String status_ = "Ready";
};
