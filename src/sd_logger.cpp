#include "sd_logger.h"

#include <SD.h>

bool SdLogger::begin(const char* streamName, const char* csvHeader) {
    stop();
    status_ = "Preparing log...";

    if (SD.cardType() == CARD_NONE) {
        status_ = "SD card unavailable";
        return false;
    }
    SD.mkdir("/ghostwire");
    SD.mkdir("/ghostwire/logs");

    path_ = "";
    for (uint16_t index = 1; index < 10000; ++index) {
        char candidate[64];
        snprintf(candidate, sizeof(candidate), "/ghostwire/logs/%s_%04u.csv",
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
