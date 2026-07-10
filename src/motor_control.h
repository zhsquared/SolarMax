#pragma once
#include <Arduino.h>

void  motorInit();

// Home to the limit switches and auto-measure the panel's travel speed. Call once
// after motorInit(); it establishes the position reference used by driveToAngle().
// (Replaces the potentiometer - position is tracked open-loop from here on.)
void  motorHomeAndCalibrate();

// Current panel angle estimate in degrees. Dead-reckoned from motor run-time and
// re-referenced whenever a limit switch is hit.
float readPanelAngle();

// Drive the panel to targetDeg with an open-loop timed move (fixed PWM). Blocks
// until it arrives, hits a limit, or the per-move safety cap. Returns false if the
// motor was never calibrated (homing failed).
bool  driveToAngle(float targetDeg);

void  motorStop();
void  motorBrake();   // Active brake then release
