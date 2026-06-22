#include "diagnostics.h"
#include "config.h"
#include "time_manager.h"
#include "motor_control.h"
#include "sensors.h"
#include "solar_position.h"
#include <Arduino.h>

// Print one labelled result line. level: 0=PASS 1=WARN 2=FAIL
static bool _hadFail = false;
static void report(int level, const char* label, const char* detail) {
    const char* tag = level == 0 ? "PASS" : level == 1 ? "WARN" : "FAIL";
    if (level == 2) _hadFail = true;
    Serial.printf("  [%s] %-14s %s\n", tag, label, detail);
}

bool runSelfCheck() {
    _hadFail = false;
    char buf[96];

    Serial.println("\n----- Self-check -----");

    // 1. Time source — a plausible year means the RTC/NTP gave us real time.
    DateTime utc = getCurrentTimeUTC();
    if (utc.year() >= 2024 && utc.year() <= 2099) {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d UTC",
                 utc.year(), utc.month(), utc.day(), utc.hour(), utc.minute());
        report(0, "Time source", buf);
    } else {
        snprintf(buf, sizeof(buf), "implausible year %d — RTC not set / NTP failed", utc.year());
        report(2, "Time source", buf);
    }

    // 2. Potentiometer — angle should sit inside the mechanical range (plus a
    //    small margin). Out of range usually means a disconnected wiper.
    float angle = readPanelAngle();
    if (angle >= PANEL_ANGLE_MIN - 5.0f && angle <= PANEL_ANGLE_MAX + 5.0f) {
        snprintf(buf, sizeof(buf), "%.1f deg (range %.0f..%.0f)",
                 angle, (float)PANEL_ANGLE_MIN, (float)PANEL_ANGLE_MAX);
        report(0, "Pot angle", buf);
    } else {
        snprintf(buf, sizeof(buf), "%.1f deg out of range — check wiper / ADC pin", angle);
        report(1, "Pot angle", buf);
    }

    // 3. Limit switches — both tripped at once is electrically impossible and
    //    points at a wiring fault or wrong LIMIT_ACTIVE polarity.
    bool cw = limitCW(), ccw = limitCCW();
    if (cw && ccw) {
        report(1, "Limit switches", "BOTH tripped — wiring fault or wrong LIMIT_ACTIVE");
    } else {
        snprintf(buf, sizeof(buf), "CW=%s CCW=%s", cw ? "trip" : "open", ccw ? "trip" : "open");
        report(0, "Limit switches", buf);
    }

    // 4. Wind — a sane reading is >= 0 and below a hurricane.
    float wind = readWindSpeedMPH();
    if (wind >= 0.0f && wind < 150.0f) {
        snprintf(buf, sizeof(buf), "%.1f mph", wind);
        report(0, "Wind sensor", buf);
    } else {
        snprintf(buf, sizeof(buf), "%.1f mph — implausible, check anemometer wiring / 12 V power", wind);
        report(1, "Wind sensor", buf);
    }

    // 5. Solar math — sanity-check the current computed sun position.
    SolarAngles sa = calculateSolarPosition(utc, LATITUDE, LONGITUDE);
    snprintf(buf, sizeof(buf), "el=%.1f az=%.1f panel=%.1f (%s)",
             sa.elevation, sa.azimuth, sa.panelAngle,
             sa.aboveHorizon ? "day" : "night");
    report(0, "Solar position", buf);

    Serial.printf("----- Self-check %s -----\n", _hadFail ? "FAILED" : "OK");
    return !_hadFail;
}
