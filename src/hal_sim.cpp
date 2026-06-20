#ifdef SIMULATE

// Simulated hardware layer — replaces motor_control, sensors, and time_manager
// when building with -DSIMULATE (pio run -e simulate).
//
// What it does:
//   - Time: starts at SIM_START date/time, advances at SIM_SPEED_FACTOR x real time
//   - Motor: instant moves, logs every angle change to Serial
//   - Wind:  5 mph baseline; injects 28 mph storm 2–4 PM local (21–23 UTC)
//   - LDRs: always balanced (0.0); limits trip at ±PANEL_ANGLE_MAX

#include "hal_sim.h"
#include "time_manager.h"
#include "motor_control.h"
#include "sensors.h"
#include "config.h"
#include <Arduino.h>
#include <RTClib.h>

static uint32_t _simStartMs = 0;
static float    _simAngle   = 0.0f;

// ── Time manager ──────────────────────────────────────────────────────────────

bool timeManagerInit() {
    _simStartMs = millis();
    Serial.printf("[SIM] Simulation start: %04d-%02d-%02d %02d:%02d UTC — running at %lux speed\n",
                  SIM_START_YEAR, SIM_START_MONTH, SIM_START_DAY,
                  SIM_START_HOUR, SIM_START_MIN, SIM_SPEED_FACTOR);
    Serial.println("[SIM] (~12 real minutes covers a full simulated day)");
    return true;
}

bool syncNTP() { return true; }

DateTime getCurrentTimeUTC() {
    uint32_t realElapsedMs = millis() - _simStartMs;
    uint32_t simElapsedSec = (uint32_t)((uint64_t)realElapsedMs * SIM_SPEED_FACTOR / 1000UL);
    DateTime base(SIM_START_YEAR, SIM_START_MONTH, SIM_START_DAY,
                  SIM_START_HOUR, SIM_START_MIN,  SIM_START_SEC);
    return DateTime(base.unixtime() + simElapsedSec);
}

bool isTimeForNTPResync(const DateTime& utc) {
    (void)utc;
    return false;
}

// ── Motor control ─────────────────────────────────────────────────────────────

void motorInit() {
    _simAngle = 0.0f;
    Serial.println("[SIM] Motor ready — panel at 0.0 deg");
}

float readPanelAngle() { return _simAngle; }

bool driveToAngle(float targetDeg) {
    targetDeg = constrain(targetDeg, PANEL_ANGLE_MIN, PANEL_ANGLE_MAX);
    if (fabsf(targetDeg - _simAngle) > MOTOR_DEADBAND_DEG) {
        Serial.printf("[SIM] Motor: %.1f deg → %.1f deg\n", _simAngle, targetDeg);
        _simAngle = targetDeg;
    }
    return true;
}

void motorStop()  {}
void motorBrake() {}

void runCalibration() {
    Serial.println("[SIM] Calibration not available in simulation mode.");
}

// ── Sensors ───────────────────────────────────────────────────────────────────

void sensorsInit() {
    Serial.println("[SIM] Sensors ready — wind 5 mph; storm 28 mph at 14:00–16:00 local");
}

float readWindSpeedMPH() {
    // Storm window: 2–4 PM local = 21:00–23:00 UTC (AZ is UTC-7, no DST)
    uint8_t h = getCurrentTimeUTC().hour();
    return (h >= 21 && h < 23) ? 28.0f : 5.0f;
}

float readLDRBalance() { return 0.0f; }

bool limitCW()  { return _simAngle >= PANEL_ANGLE_MAX; }
bool limitCCW() { return _simAngle <= PANEL_ANGLE_MIN; }

#endif // SIMULATE
