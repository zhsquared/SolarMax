#pragma once
#include <Arduino.h>

void  sensorsInit();

// Returns averaged wind speed in mph. Updates on ANEM_SAMPLE_MS intervals.
float readWindSpeedMPH();

// Returns LDR balance: positive = east side brighter, negative = west side brighter.
// Range roughly -1.0 to +1.0.
float readLDRBalance();

bool  limitCW();    // True if CW (west) limit switch is tripped
bool  limitCCW();   // True if CCW (east) limit switch is tripped
