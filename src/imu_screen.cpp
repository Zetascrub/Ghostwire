#include "imu_screen.h"

#include <M5Cardputer.h>
#include <cmath>

#include "branding.h"
#include "screen_chrome.h"

const char* ImuScreen::sensorTypeName() {
    switch (M5.Imu.getType()) {
        case m5::imu_bmi270: return "BMI270";
        case m5::imu_mpu6050: return "MPU6050";
        case m5::imu_mpu6886: return "MPU6886";
        case m5::imu_mpu9250: return "MPU9250";
        case m5::imu_sh200q: return "SH200Q";
        case m5::imu_none: return "not detected";
        default: return "unknown";
    }
}

void ImuScreen::draw(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Motion / IMU", fullDraw);
    auto& display = M5Cardputer.Display;
    if (!available_) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 38);
        display.printf("Sensor: %s", sensorTypeName());
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 58);
        display.print("IMU is unavailable.");
        ScreenChrome::drawFooter("R: retry   Backspace/Q: back");
        return;
    }

    const float gx = data_.gyro.x - gyroOffsetX_;
    const float gy = data_.gyro.y - gyroOffsetY_;
    const float gz = data_.gyro.z - gyroOffsetZ_;
    const float accelMagnitude =
        sqrtf(data_.accel.x * data_.accel.x + data_.accel.y * data_.accel.y +
              data_.accel.z * data_.accel.z);
    const float gyroMagnitude = sqrtf(gx * gx + gy * gy + gz * gz);
    // The sensor's native XY frame is rotated relative to the landscape
    // screen/keyboard. Rotate it 90 degrees clockwise for human-facing
    // pitch and roll while retaining native axes in the diagnostic rows.
    const float screenAccelX = data_.accel.y;
    const float screenAccelY = -data_.accel.x;
    const float roll = atan2f(screenAccelY, data_.accel.z) * 180.0F / PI;
    const float pitch =
        atan2f(screenAccelX,
               sqrtf(screenAccelY * screenAccelY +
                     data_.accel.z * data_.accel.z)) *
        180.0F / PI;
    const bool stationary =
        accelMagnitude > 0.85F && accelMagnitude < 1.15F &&
        gyroMagnitude < 2.0F;

    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  %s%s", sensorTypeName(),
                   calibrating_ ? "CALIBRATING" : (stationary ? "STILL" : "MOVING"),
                   logger_.isActive() ? "  REC" : "");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Accel g  X%6.2f Y%6.2f Z%6.2f", data_.accel.x,
                   data_.accel.y, data_.accel.z);
    display.setCursor(8, 63);
    display.printf("Gyro d/s X%6.1f Y%6.1f Z%6.1f", gx, gy, gz);
    display.setCursor(8, 80);
    display.printf("Pitch %6.1f   Roll %6.1f", pitch, roll);
    display.setCursor(8, 97);
    if (calibrating_) {
        display.printf("Keep still: %u%%", calibrationSamples_);
    } else if (logger_.isActive()) {
        String logName = logger_.path();
        const int slash = logName.lastIndexOf('/');
        if (slash >= 0) logName = logName.substring(slash + 1);
        display.printf("%s  %lu rows", logName.c_str(),
                       static_cast<unsigned long>(logger_.rowCount()));
    } else {
        display.printf("|a| %.2fg   |w| %.1f d/s", accelMagnitude,
                       gyroMagnitude);
    }
    ScreenChrome::drawFooter("Tab: actions   Q: back");
}
