#pragma once
#include <Arduino.h>

void  motorInit();

// Returns current panel angle in degrees from potentiometer.
float readPanelAngle();

// Drives panel to targetDeg. Blocks until within deadband or timeout/limit hit.
// Returns true on success, false on timeout or limit switch fault.
bool  driveToAngle(float targetDeg);

void  motorStop();
void  motorBrake();   // Active brake then release

// Serial-guided calibration — run once to find POT_ADC_MIN / POT_ADC_MAX.
void  runCalibration();
