#ifndef SIMULATE   // entire file excluded when running simulation

#include "motor_control.h"
#include "config.h"
#include <Arduino.h>

// ── Position tracking WITHOUT a potentiometer ─────────────────────────────────
// There is no encoder or usable position sensor, so we estimate the panel angle by
// DEAD RECKONING: the motor runs at a fixed PWM (MOTOR_PWM_MOVE), which gives a
// repeatable travel speed, so  time * speed = angle moved.
//
// Two references keep the estimate honest, both from the LIMIT SWITCHES at the
// mechanical ends (PANEL_ANGLE_MIN = east, PANEL_ANGLE_MAX = west):
//   • At boot we home to the east limit, then sweep to the west limit while timing
//     it — that measures the travel speed (deg/s) automatically, no manual step.
//   • Any time a limit switch trips during operation, we snap the estimate to that
//     end's exact angle. The tracker naturally pins east each morning and west each
//     evening, so accumulated drift is erased twice a day for free.

static float estimatedAngle = 0.0f;   // software position estimate (degrees)
static float degPerSec      = 0.0f;   // travel speed measured at MOTOR_PWM_MOVE
static bool  calibrated     = false;  // true once homing + speed measurement succeed

// ── Low-level drive. CW moves toward the WEST/+ limit, CCW toward the EAST/- limit.
static void driveCW (uint8_t s) { ledcWrite(PWM_CHANNEL_L, 0); ledcWrite(PWM_CHANNEL_R, s); }
static void driveCCW(uint8_t s) { ledcWrite(PWM_CHANNEL_R, 0); ledcWrite(PWM_CHANNEL_L, s); }

static bool limitWest() { return digitalRead(PIN_LIMIT_CW)  == LIMIT_ACTIVE; }  // at +MAX
static bool limitEast() { return digitalRead(PIN_LIMIT_CCW) == LIMIT_ACTIVE; }  // at -MIN

void motorStop() {
    ledcWrite(PWM_CHANNEL_R, 0);
    ledcWrite(PWM_CHANNEL_L, 0);
}

void motorBrake() {
    ledcWrite(PWM_CHANNEL_R, 255);
    ledcWrite(PWM_CHANNEL_L, 255);
    delay(80);
    motorStop();
}

void motorInit() {
    ledcSetup(PWM_CHANNEL_R, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_L, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_RPWM, PWM_CHANNEL_R);
    ledcAttachPin(PIN_MOTOR_LPWM, PWM_CHANNEL_L);

    pinMode(PIN_MOTOR_REN, OUTPUT);
    pinMode(PIN_MOTOR_LEN, OUTPUT);
    digitalWrite(PIN_MOTOR_REN, HIGH);
    digitalWrite(PIN_MOTOR_LEN, HIGH);

    pinMode(PIN_LIMIT_CW,  INPUT_PULLDOWN);
    pinMode(PIN_LIMIT_CCW, INPUT_PULLDOWN);
    motorStop();
}

// Drive one direction at the fixed move speed until that end's limit trips or the
// timeout elapses. Returns true if the limit was reached.
static bool runToLimit(bool west, unsigned long timeoutMs) {
    unsigned long start = millis();
    west ? driveCW(MOTOR_PWM_MOVE) : driveCCW(MOTOR_PWM_MOVE);
    while (!(west ? limitWest() : limitEast())) {
        if (millis() - start > timeoutMs) { motorStop(); return false; }
        delay(5);
    }
    motorStop();
    return true;
}

void motorHomeAndCalibrate() {
    Serial.println("[MOTOR] Homing to east limit...");
    if (!runToLimit(false, HOMING_TIMEOUT_MS)) {
        Serial.println("[MOTOR] ERROR: never reached east limit — check limit-switch wiring");
        calibrated = false;
        return;
    }
    estimatedAngle = PANEL_ANGLE_MIN;                 // now parked at the east end

    Serial.println("[MOTOR] Sweeping to west limit to measure travel speed...");
    unsigned long t0 = millis();
    if (!runToLimit(true, HOMING_TIMEOUT_MS)) {
        Serial.println("[MOTOR] ERROR: never reached west limit — check limit-switch wiring");
        calibrated = false;
        return;
    }
    unsigned long sweepMs = millis() - t0;
    estimatedAngle = PANEL_ANGLE_MAX;                 // now parked at the west end

    float range = PANEL_ANGLE_MAX - PANEL_ANGLE_MIN;  // total sweep in degrees
    degPerSec   = (sweepMs > 0) ? range / (sweepMs / 1000.0f) : 0.0f;
    calibrated  = (degPerSec > 0.0f);

    Serial.printf("[MOTOR] Calibrated: %.0f deg in %.1f s = %.2f deg/s\n",
                  range, sweepMs / 1000.0f, degPerSec);
}

// Software estimate of the current panel angle (replaces the old pot reading).
float readPanelAngle() { return estimatedAngle; }

bool driveToAngle(float targetDeg) {
    targetDeg = constrain(targetDeg, PANEL_ANGLE_MIN, PANEL_ANGLE_MAX);

    if (!calibrated) {
        Serial.println("[MOTOR] ERROR: not calibrated — check limit switches and reset.");
        return false;
    }

    float delta = targetDeg - estimatedAngle;
    if (fabsf(delta) <= MOTOR_DEADBAND_DEG) return true;   // already close enough

    bool west = (delta > 0.0f);                            // + = west = CW
    unsigned long runMs = (unsigned long)(fabsf(delta) / degPerSec * 1000.0f);
    if (runMs > DRIVE_TIMEOUT_MS) runMs = DRIVE_TIMEOUT_MS; // safety cap; big moves finish over 2 ticks

    unsigned long start = millis();
    west ? driveCW(MOTOR_PWM_MOVE) : driveCCW(MOTOR_PWM_MOVE);
    while (millis() - start < runMs) {
        // Hit the mechanical end early → snap the estimate to it (drift correction).
        if (west && limitWest()) { motorStop(); estimatedAngle = PANEL_ANGLE_MAX; return true; }
        if (!west && limitEast()){ motorStop(); estimatedAngle = PANEL_ANGLE_MIN; return true; }
        delay(5);
    }
    motorStop();

    // Dead-reckoned arrival: advance the estimate by how long we actually drove.
    float moved = (millis() - start) / 1000.0f * degPerSec;
    estimatedAngle += west ? moved : -moved;
    estimatedAngle = constrain(estimatedAngle, PANEL_ANGLE_MIN, PANEL_ANGLE_MAX);
    return true;
}

#endif // SIMULATE
