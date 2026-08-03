#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <vector>

class NimBLEAdvertisedDevice;

struct BleDeviceInfo {
    String address;
    String name;
    String service;
    String manufacturerData;
    String payloadData;
    String manufacturer;
    int rssi;
    bool connectable;
    uint8_t addressType;
    uint8_t advertisementType;
    uint8_t serviceCount;
    uint16_t payloadLength;
};

class BleScanner {
public:
    bool scan(std::vector<BleDeviceInfo>& devices, String& status,
              uint32_t durationSeconds = 5);
    void stop();
    bool beginContinuous(String& status);
    bool nextResult(BleDeviceInfo& device);
    bool isContinuous() const { return continuous_; }
    uint32_t advertisementCount() const { return advertisementCount_; }
    uint32_t droppedCount() const { return droppedCount_; }
    void onAdvertisement(const NimBLEAdvertisedDevice* advertised);

private:
    struct RawAdvertisement {
        char address[18];
        char name[32];
        char service[112];
        char manufacturerData[52];
        char payloadData[100];
        char manufacturer[28];
        int16_t rssi;
        uint16_t payloadLength;
        uint8_t addressType;
        uint8_t advertisementType;
        uint8_t serviceCount;
        bool connectable;
    };

    bool initialized_ = false;
    bool continuous_ = false;
    QueueHandle_t queue_ = nullptr;
    StaticQueue_t queueControl_{};
    static constexpr size_t kQueueCapacity = 20;
    uint8_t queueStorage_[kQueueCapacity * sizeof(RawAdvertisement)]{};
    volatile uint32_t advertisementCount_ = 0;
    volatile uint32_t droppedCount_ = 0;
};
