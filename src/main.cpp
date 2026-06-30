#include <Arduino.h>
#include "config.h"
#include "solar_position.h"
#include "tracker_core.h"
#include "motor_control.h"
#include "sensors.h"
#include "time_manager.h"
#include "diagnostics.h"

// The decision brain (shared with the interactive simulator).
static TrackerCore   core;
static TrackerConfig cfg;
static uint32_t      lastTrackMs = 0;

// Fill the config brain from the compile-time settings in config.h.
static void loadConfig() {
    cfg.lat = LATITUDE;            cfg.lon = LONGITUDE;
    cfg.axisTilt = AXIS_TILT_DEG;  cfg.axisAzimuth = AXIS_AZIMUTH_DEG;
    cfg.windStowMph = WIND_STOW_MPH; cfg.windResumeMph = WIND_RESUME_MPH;
    cfg.panelMin = PANEL_ANGLE_MIN;  cfg.panelMax = PANEL_ANGLE_MAX;
    cfg.stowAngle = PANEL_STOW_ANGLE;
}

// ── Diagnostic: compare LDR balance to expected sun direction ─────────────────
static void checkLDRDiagnostic(const SolarAngles& sa) {
    if (!sa.aboveHorizon) return;
    float balance = readLDRBalance();

    // If the algorithm says sun is in west (positive panel angle) but
    // east LDR is significantly brighter, something is wrong.
    bool algorithmSaysWest = sa.panelAngle > 5.0f;
    bool ldrSaysEast       = balance > (float)LDR_FLAG_THRESH / 4095.0f;

    bool algorithmSaysEast = sa.panelAngle < -5.0f;
    bool ldrSaysWest       = balance < -(float)LDR_FLAG_THRESH / 4095.0f;

    if ((algorithmSaysWest && ldrSaysEast) || (algorithmSaysEast && ldrSaysWest)) {
        Serial.printf("[DIAG] WARNING: LDR balance=%.2f disagrees with calculated panel angle=%.1f deg\n",
                      balance, sa.panelAngle);
        Serial.println("[DIAG] Possible causes: panel obstruction, dust on LDR, or mounting error");
    }
}

static void printSolarState(const SolarAngles& sa, const DateTime& utc) {
    // Convert UTC to local for human-readable display only
    int localHour = (utc.hour() + 24 + TIMEZONE_OFFSET) % 24;
    Serial.printf("[SUN]  Local %02d:%02d  El=%.1f deg  Az=%.1f deg  Panel target=%.1f deg\n",
                  localHour, utc.minute(),
                  sa.elevation, sa.azimuth, sa.panelAngle);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n============================");
    Serial.println("  SolarMax v1.0  — Noopy Industries");
    Serial.println("============================");

    motorInit();
    sensorsInit();
    loadConfig();

    if (!timeManagerInit()) {
        core.state = TS_ERROR;
        return;
    }

    // Uncomment the line below on first hardware assembly to calibrate the pot:
    // runCalibration(); while(true);

    runSelfCheck();   // Print a peripheral health report before tracking starts
}

// ── Main Loop ─────────────────────────────────────────────────────────────────
void loop() {
    DateTime utc = getCurrentTimeUTC();

    // Daily NTP re-sync keeps long-term time accuracy
    if (isTimeForNTPResync(utc)) syncNTP();

    // Gather inputs and let the shared brain decide the next state + target angle.
    TrackerInputs in;
    in.year = utc.year(); in.month = utc.month(); in.day = utc.day();
    in.hourUTC = utc.hour() + utc.minute() / 60.0 + utc.second() / 3600.0;
    in.windMph = readWindSpeedMPH();
    in.timeValid = true;

    TrackerStep s = core.step(cfg, in);

    if (s.state == TS_ERROR) {
        Serial.println("[ERROR] No valid time source — fix WiFi/RTC and reset.");
        delay(10000);
        return;
    }

    if (s.note) Serial.printf("[STATE] %s\n", s.note);

    // Drive to the commanded angle. The motor deadband means this only physically
    // moves when the error exceeds MOTOR_DEADBAND_DEG, which naturally throttles
    // sun-following to a move every few minutes.
    driveToAngle(s.targetAngle);

    // Periodic human-readable telemetry.
    if (s.state == TS_TRACKING && (s.note || millis() - lastTrackMs >= TRACKING_INTERVAL_MS)) {
        printSolarState(s.sun, utc);
        checkLDRDiagnostic(s.sun);
        lastTrackMs = millis();
    } else if (s.state == TS_NIGHT) {
        static uint32_t lastNightPrintMs = 0;
        if (s.note || millis() - lastNightPrintMs >= 10000) {
            int localHour = (utc.hour() + 24 + TIMEZONE_OFFSET) % 24;
            Serial.printf("[NIGHT] Waiting for sunrise... %02d:%02d local\n",
                          localHour, utc.minute());
            lastNightPrintMs = millis();
        }
    }

    delay(1000);  // 1-second main loop tick
}
