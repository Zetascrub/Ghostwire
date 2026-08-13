#pragma once

#include <Arduino.h>
#include <FS.h>

class SdLogger {
public:
    // `subdir` nests under /ghostwire/ (default "logs", matching every
    // existing caller's prior hardcoded path) -- pass a more specific one
    // (e.g. "logs/poe") to keep a feature's own exports from piling into
    // the same shared folder as everything else, which otherwise risks
    // tripping the Files screen's 128-entry listing cap (main.cpp) once a
    // folder accumulates enough files from unrelated features.
    bool begin(const char* streamName, const char* csvHeader,
              const char* subdir = "logs");
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
