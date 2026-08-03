#define DISABLE_CODE_FOR_RECEIVER
#define SEND_PWM_BY_TIMER

#include "ir_service.h"

#include <IRremote.hpp>

void IrService::begin() {
    if (initialized_) return;
    IrSender.begin(kTransmitPin);
    initialized_ = true;
}

void IrService::sendSelfTest() {
    begin();

    // A deliberately non-protocol test pattern. It is easy to see through
    // many phone cameras but is not intended to operate consumer equipment.
    static const uint16_t pattern[] = {
        1200, 600, 300, 300, 300, 600, 300, 300, 600, 300, 300, 1200,
    };
    for (int repetition = 0; repetition < 8; ++repetition) {
        IrSender.sendRaw(pattern, sizeof(pattern) / sizeof(pattern[0]),
                         kCarrierKhz);
        delay(70);
    }
    ++transmissionCount_;
}

uint32_t IrService::transmissionCount() const {
    return transmissionCount_;
}
