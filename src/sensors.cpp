#ifndef SIMULATE   // entire file excluded when running simulation

#include "sensors.h"
#include "config.h"

static volatile uint32_t _pulses     = 0;
static uint32_t          _lastCheckMs = 0;
static float             _windMPH    = 0.0f;

static void IRAM_ATTR anemometerISR() {
    _pulses++;
}

void sensorsInit() {
    pinMode(PIN_ANEMOMETER, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER), anemometerISR, RISING);

    pinMode(PIN_LDR_EAST, INPUT);
    pinMode(PIN_LDR_WEST, INPUT);
}

float readWindSpeedMPH() {
    uint32_t now     = millis();
    uint32_t elapsed = now - _lastCheckMs;
    if (elapsed < ANEM_SAMPLE_MS) return _windMPH;

    uint32_t count = _pulses;
    _pulses        = 0;
    _lastCheckMs   = now;

    float hz   = (float)count / (elapsed / 1000.0f);
    _windMPH   = hz * ANEM_MPH_PER_HZ;
    return _windMPH;
}

float readLDRBalance() {
    int east = analogRead(PIN_LDR_EAST);
    int west = analogRead(PIN_LDR_WEST);
    // Positive = east brighter (sun on east side of panel = panel behind the sun)
    return (float)(east - west) / 4095.0f;
}

bool limitCW() {
    return digitalRead(PIN_LIMIT_CW) == LIMIT_ACTIVE;
}

bool limitCCW() {
    return digitalRead(PIN_LIMIT_CCW) == LIMIT_ACTIVE;
}

#endif // SIMULATE
