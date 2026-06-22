#ifndef SIMULATE   // entire file excluded when running simulation

#include "sensors.h"
#include "config.h"

static uint32_t _lastCheckMs = 0;
static float    _windMPH     = 0.0f;

void sensorsInit() {
    // Anemometer (Adafruit 1733): plain analog input on GPIO35. The pin is
    // input-only, so there is nothing to drive — readWindSpeedMPH() samples it.
    pinMode(PIN_ANEMOMETER, INPUT);

    pinMode(PIN_LDR_EAST, INPUT);
    pinMode(PIN_LDR_WEST, INPUT);
}

float readWindSpeedMPH() {
    uint32_t now     = millis();
    uint32_t elapsed = now - _lastCheckMs;
    if (elapsed < ANEM_SAMPLE_MS) return _windMPH;
    _lastCheckMs = now;

    // Average several calibrated ADC reads (mV) to smooth noise.
    uint32_t mvSum = 0;
    for (int i = 0; i < ANEM_ADC_SAMPLES; i++) {
        mvSum += analogReadMilliVolts(PIN_ANEMOMETER);
    }
    float mv = (float)mvSum / ANEM_ADC_SAMPLES;

    // Linear map mV -> m/s (0.40 V = 0 m/s, 2.00 V = 32.4 m/s), clamped at 0.
    float mps = (mv - ANEM_V_OFFSET_MV) * (ANEM_MS_FS / (ANEM_V_FS_MV - ANEM_V_OFFSET_MV));
    if (mps < 0.0f) mps = 0.0f;

    _windMPH = mps * ANEM_MPH_PER_MS;
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
