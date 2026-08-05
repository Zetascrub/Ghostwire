#pragma once

#include <M5Unified.h>

#include "sd_logger.h"

// Motion / IMU screen: accelerometer/gyro readout, pitch/roll, and the CSV
// log indicator. Draw-only extraction (see docs/screen-extraction.md);
// sampling the sensor (updateImu()), gyro calibration
// (beginImuCalibration()), and log start/stop stay in main.cpp, which still
// owns the underlying sensor state -- this screen just holds references to
// it.
//
// Unlike the other draw-only extractions so far, that state isn't a single
// service object: main.cpp keeps the last sample, calibration offsets, and
// availability/calibrating flags as separate globals (mirroring how
// M5Unified hands back the raw IMU reading). Rather than bundle them into a
// new struct as part of this extraction -- which would also mean touching
// updateImu() and beginImuCalibration() -- the screen just takes a reference
// to each, same as it would a service.
class ImuScreen {
public:
    ImuScreen(bool& available, m5::imu_data_t& data, bool& calibrating,
              uint16_t& calibrationSamples, float& gyroOffsetX,
              float& gyroOffsetY, float& gyroOffsetZ, SdLogger& logger)
        : available_(available),
          data_(data),
          calibrating_(calibrating),
          calibrationSamples_(calibrationSamples),
          gyroOffsetX_(gyroOffsetX),
          gyroOffsetY_(gyroOffsetY),
          gyroOffsetZ_(gyroOffsetZ),
          logger_(logger) {}

    void draw(bool fullDraw = true);

private:
    static const char* sensorTypeName();

    bool& available_;
    m5::imu_data_t& data_;
    bool& calibrating_;
    uint16_t& calibrationSamples_;
    float& gyroOffsetX_;
    float& gyroOffsetY_;
    float& gyroOffsetZ_;
    SdLogger& logger_;
};
