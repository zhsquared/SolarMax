#include <Arduino.h>
#include "config.h"
#include "solar_position.h"
#include "motor_control.h"
#include "sensors.h"
#include "time_manager.h"
#include "diagnostics.h"

// ── State Machine ─────────────────────────────────────────────────────────────
enum SystemState {
    STATE_INIT,      // Power-on: home panel, determine starting state
    STATE_TRACKING,  // Normal operation: follow the sun every 5 minutes
    STATE_STOW,      // High wind: hold flat until wind drops
    STATE_NIGHT,     // Sun below horizon: park east, wait for sunrise
    STATE_ERROR      // No valid time source — operator intervention needed
};

static SystemState state       = STATE_INIT;
static uint32_t    lastTrackMs = 0;

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

    if (!timeManagerInit()) {
        state = STATE_ERROR;
        return;
    }

    // Uncomment the line below on first hardware assembly to calibrate the pot:
    // runCalibration(); while(true);

    runSelfCheck();   // Print a peripheral health report before tracking starts

    state = STATE_INIT;
}

// ── Main Loop ─────────────────────────────────────────────────────────────────
void loop() {
    DateTime    utc  = getCurrentTimeUTC();
    SolarAngles sa   = calculateSolarPosition(utc, LATITUDE, LONGITUDE);
    float       wind = readWindSpeedMPH();

    // Daily NTP re-sync keeps long-term time accuracy
    if (isTimeForNTPResync(utc)) {
        syncNTP();
    }

    switch (state) {

        // ── INIT ──────────────────────────────────────────────────────────────
        case STATE_INIT:
            Serial.println("[STATE] INIT — homing panel to east position");
            driveToAngle(PANEL_ANGLE_MIN);
            state      = sa.aboveHorizon ? STATE_TRACKING : STATE_NIGHT;
            lastTrackMs = 0;  // Force immediate tracking update on entry
            Serial.printf("[STATE] → %s\n", sa.aboveHorizon ? "TRACKING" : "NIGHT");
            break;

        // ── TRACKING ──────────────────────────────────────────────────────────
        case STATE_TRACKING:
            if (wind >= WIND_STOW_MPH) {
                Serial.printf("[WIND]  %.1f mph — above stow threshold (%.0f mph)\n",
                              wind, WIND_STOW_MPH);
                Serial.println("[STATE] TRACKING → STOW");
                driveToAngle(PANEL_STOW_ANGLE);
                state = STATE_STOW;
                break;
            }

            if (!sa.aboveHorizon) {
                Serial.println("[STATE] TRACKING → NIGHT (sun set)");
                driveToAngle(PANEL_ANGLE_MIN);  // Park east for tomorrow
                state = STATE_NIGHT;
                break;
            }

            if (millis() - lastTrackMs >= TRACKING_INTERVAL_MS) {
                printSolarState(sa, utc);
                driveToAngle(sa.panelAngle);
                checkLDRDiagnostic(sa);
                lastTrackMs = millis();
            }
            break;

        // ── STOW ──────────────────────────────────────────────────────────────
        case STATE_STOW:
            if (wind < WIND_RESUME_MPH) {
                Serial.printf("[WIND]  %.1f mph — below resume threshold (%.0f mph)\n",
                              wind, WIND_RESUME_MPH);
                if (sa.aboveHorizon) {
                    Serial.println("[STATE] STOW → TRACKING");
                    state       = STATE_TRACKING;
                    lastTrackMs = 0;
                } else {
                    Serial.println("[STATE] STOW → NIGHT");
                    driveToAngle(PANEL_ANGLE_MIN);
                    state = STATE_NIGHT;
                }
            }
            break;

        // ── NIGHT ─────────────────────────────────────────────────────────────
        case STATE_NIGHT: {
            static uint32_t lastNightPrintMs = 0;
            if (millis() - lastNightPrintMs >= 10000) {
                int localHour = (utc.hour() + 24 + TIMEZONE_OFFSET) % 24;
                Serial.printf("[NIGHT] Waiting for sunrise... sim time %02d:%02d local\n",
                              localHour, utc.minute());
                lastNightPrintMs = millis();
            }
            if (sa.aboveHorizon) {
                Serial.println("[STATE] NIGHT → TRACKING (sunrise)");
                state       = STATE_TRACKING;
                lastTrackMs = 0;
            }
            break;
        }

        // ── ERROR ─────────────────────────────────────────────────────────────
        case STATE_ERROR:
            Serial.println("[ERROR] No valid time source — system halted.");
            Serial.println("[ERROR] Fix WiFi credentials or connect DS3231 RTC, then reset.");
            delay(10000);
            break;
    }

    delay(1000);  // 1-second main loop tick
}
