#include "gnss_service.h"

void GnssService::begin() {
    if (started_) return;
    serial_.begin(kBaud, SERIAL_8N1, kReceivePin, kTransmitPin);
    started_ = true;
}

void GnssService::update() {
    if (!started_) return;
    while (serial_.available()) {
        parser_.encode(static_cast<char>(serial_.read()));
    }
}

void GnssService::restart() {
    if (started_) serial_.end();
    started_ = false;
    parser_ = TinyGPSPlus();
    begin();
}

bool GnssService::hasData() const {
    return parser_.charsProcessed() > 0;
}

bool GnssService::hasFix() const {
    return parser_.location.isValid() && parser_.location.age() < 5000;
}

uint32_t GnssService::charactersProcessed() const {
    return parser_.charsProcessed();
}

uint32_t GnssService::satellites() {
    return parser_.satellites.isValid() ? parser_.satellites.value() : 0;
}

double GnssService::latitude() {
    return parser_.location.isValid() ? parser_.location.lat() : 0.0;
}

double GnssService::longitude() {
    return parser_.location.isValid() ? parser_.location.lng() : 0.0;
}

double GnssService::altitudeMetres() {
    return parser_.altitude.isValid() ? parser_.altitude.meters() : 0.0;
}

double GnssService::hdop() {
    return parser_.hdop.isValid() ? parser_.hdop.hdop() : 0.0;
}

String GnssService::utcTime() {
    if (!parser_.time.isValid()) return "--:--:--";
    char value[9];
    snprintf(value, sizeof(value), "%02u:%02u:%02u",
             static_cast<unsigned>(parser_.time.hour() % 24),
             static_cast<unsigned>(parser_.time.minute() % 60),
             static_cast<unsigned>(parser_.time.second() % 60));
    return String(value);
}

bool GnssService::hasUtcDateTime() const {
    return parser_.date.isValid() && parser_.time.isValid() &&
           parser_.date.age() < 5000 && parser_.time.age() < 5000;
}

uint16_t GnssService::utcYear() { return parser_.date.year(); }
uint8_t GnssService::utcMonth() { return parser_.date.month(); }
uint8_t GnssService::utcDay() { return parser_.date.day(); }
uint8_t GnssService::utcHour() { return parser_.time.hour(); }
uint8_t GnssService::utcMinute() { return parser_.time.minute(); }
uint8_t GnssService::utcSecond() { return parser_.time.second(); }
