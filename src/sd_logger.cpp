#include "sd_logger.h"

#include <SD.h>

bool SdLogger::begin(const char* streamName, const char* csvHeader, const char* subdir) {
    stop();
    status_ = "Preparing log...";

    if (SD.cardType() == CARD_NONE) {
        status_ = "SD card unavailable";
        return false;
    }
    // subdir is commonly nested (e.g. "logs/wifi") -- ESP32's SD.mkdir()
    // isn't guaranteed to create intermediate directories in one call, so
    // walk and create each path component in turn rather than relying on
    // that. mkdir() on an already-existing directory is a harmless no-op.
    String subdirPath = "/ghostwire";
    SD.mkdir(subdirPath);
    String remaining = subdir;
    int slash;
    while ((slash = remaining.indexOf('/')) >= 0) {
        subdirPath += "/" + remaining.substring(0, slash);
        SD.mkdir(subdirPath);
        remaining = remaining.substring(slash + 1);
    }
    if (!remaining.isEmpty()) {
        subdirPath += "/" + remaining;
        SD.mkdir(subdirPath);
    }

    path_ = "";
    for (uint16_t index = 1; index < 10000; ++index) {
        char candidate[80];
        snprintf(candidate, sizeof(candidate), "%s/%s_%04u.csv", subdirPath.c_str(),
                 streamName, index);
        if (!SD.exists(candidate)) {
            path_ = candidate;
            break;
        }
    }
    if (path_.isEmpty()) {
        status_ = "No free log filename";
        return false;
    }

    file_ = SD.open(path_, FILE_WRITE);
    if (!file_) {
        status_ = "Unable to create log";
        return false;
    }
    if (!file_.println(csvHeader)) {
        file_.close();
        status_ = "Unable to write header";
        return false;
    }
    file_.flush();
    rowCount_ = 0;
    errorCount_ = 0;
    lastFlushMs_ = millis();
    active_ = true;
    status_ = "Recording";
    return true;
}

bool SdLogger::append(const String& line) {
    if (!active_) return false;
    if (!file_.println(line)) {
        ++errorCount_;
        status_ = "Write failed";
        file_.close();
        active_ = false;
        return false;
    }
    ++rowCount_;
    return true;
}

void SdLogger::update() {
    if (!active_ || millis() - lastFlushMs_ < 1000) return;
    file_.flush();
    if (file_.getWriteError()) {
        ++errorCount_;
        status_ = "Flush failed";
        file_.close();
        active_ = false;
        return;
    }
    lastFlushMs_ = millis();
}

void SdLogger::stop() {
    if (file_) {
        file_.flush();
        file_.close();
    }
    if (active_) status_ = "Saved";
    active_ = false;
}
