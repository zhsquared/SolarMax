#include "Arduino.h"
#include <chrono>

static const auto _boot = std::chrono::steady_clock::now();

uint32_t millis() {
    auto now = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        now - _boot).count();
}

void delay(uint32_t ms) {
    usleep((useconds_t)ms * 1000u);
}

SerialClass Serial;
WiFiClass   WiFi;
TwoWire     Wire;
