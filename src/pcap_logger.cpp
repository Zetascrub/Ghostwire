#include "pcap_logger.h"

#include <SD.h>
#include <cstring>

namespace {
constexpr uint32_t kPcapMagic = 0xa1b2c3d4;
constexpr uint16_t kPcapVersionMajor = 2;
constexpr uint16_t kPcapVersionMinor = 4;
constexpr uint32_t kSnapLen = 2048;
constexpr uint32_t kLinktypeIeee80211 = 105;  // Plain 802.11, no radiotap.
}  // namespace

bool PcapLogger::begin(const char* streamName, const char* subdir) {
    stop();
    status_ = "Preparing capture...";

    if (SD.cardType() == CARD_NONE) {
        status_ = "SD card unavailable";
        return false;
    }
    // See the matching comment in SdLogger::begin() -- same reasoning for
    // walking subdir's path components rather than trusting SD.mkdir() to
    // create nested directories in one call.
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
        snprintf(candidate, sizeof(candidate), "%s/%s_%04u.pcap", subdirPath.c_str(),
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
        status_ = "Unable to create capture";
        return false;
    }

    // Global pcap header. Every multi-byte field below is written in this
    // MCU's native (little-endian) byte order via memcpy, including the
    // magic number itself -- that's what signals "little-endian file" to
    // readers, and keeps every subsequent field self-consistent.
    uint8_t header[24];
    memcpy(header + 0, &kPcapMagic, 4);
    memcpy(header + 4, &kPcapVersionMajor, 2);
    memcpy(header + 6, &kPcapVersionMinor, 2);
    memset(header + 8, 0, 8);  // thiszone, sigfigs: both 0, standard.
    memcpy(header + 16, &kSnapLen, 4);
    memcpy(header + 20, &kLinktypeIeee80211, 4);
    if (file_.write(header, sizeof(header)) != sizeof(header)) {
        file_.close();
        status_ = "Unable to write header";
        return false;
    }
    file_.flush();
    frameCount_ = 0;
    errorCount_ = 0;
    byteCount_ = sizeof(header);
    lastFlushMs_ = millis();
    active_ = true;
    status_ = "Capturing";
    return true;
}

bool PcapLogger::append(const uint8_t* frame, size_t length,
                        uint32_t timestampSec, uint32_t timestampUsec) {
    if (!active_) return false;
    const uint32_t capturedLength = static_cast<uint32_t>(length);
    uint8_t recordHeader[16];
    memcpy(recordHeader + 0, &timestampSec, 4);
    memcpy(recordHeader + 4, &timestampUsec, 4);
    memcpy(recordHeader + 8, &capturedLength, 4);
    memcpy(recordHeader + 12, &capturedLength, 4);
    if (file_.write(recordHeader, sizeof(recordHeader)) != sizeof(recordHeader) ||
        file_.write(frame, length) != length) {
        ++errorCount_;
        status_ = "Write failed";
        file_.close();
        active_ = false;
        return false;
    }
    ++frameCount_;
    byteCount_ += sizeof(recordHeader) + length;
    return true;
}

void PcapLogger::update() {
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

void PcapLogger::stop() {
    if (file_) {
        file_.flush();
        file_.close();
    }
    if (active_) status_ = "Saved";
    active_ = false;
}
