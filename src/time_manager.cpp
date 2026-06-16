#ifndef SIMULATE   // entire file excluded when running simulation

#include "time_manager.h"
#include "config.h"
#include <Wire.h>
#include <WiFi.h>
#include <time.h>

static RTC_DS3231 _rtc;
static bool       _rtcOk  = false;
static bool       _timeOk = false;

bool syncNTP() {
    Serial.print("[NTP] Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println(" — FAILED (timeout)");
            WiFi.disconnect(true);
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println(" — connected");

    // SNTP sync (timezone = 0 because we always work in UTC internally)
    configTime(0, 0, NTP_SERVER1, NTP_SERVER2);

    struct tm utcTm;
    Serial.print("[NTP] Syncing time");
    unsigned long ntpStart = millis();
    while (!getLocalTime(&utcTm, 1000)) {
        if (millis() - ntpStart > NTP_TIMEOUT_MS) {
            Serial.println(" — FAILED (timeout)");
            WiFi.disconnect(true);
            return false;
        }
        Serial.print(".");
    }
    Serial.println(" — OK");

    // getLocalTime with configTime(0,0,...) returns UTC
    if (_rtcOk) {
        DateTime ntpUtc(
            utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday,
            utcTm.tm_hour,        utcTm.tm_min,       utcTm.tm_sec);
        _rtc.adjust(ntpUtc);
        Serial.println("[RTC] Updated from NTP");
    }

    WiFi.disconnect(true);
    _timeOk = true;
    return true;
}

bool timeManagerInit() {
    Wire.begin(RTC_SDA, RTC_SCL);

    _rtcOk = _rtc.begin();
    if (_rtcOk) {
        Serial.println("[RTC] DS3231 found");
        if (_rtc.lostPower()) {
            Serial.println("[RTC] Battery low or first boot — time not valid yet");
        } else {
            _timeOk = true;
            Serial.println("[RTC] Time valid from battery backup");
        }
    } else {
        Serial.println("[RTC] DS3231 not detected — NTP only mode");
    }

    // Always try NTP; it updates the RTC if present
    bool ntpOk = syncNTP();

    if (!ntpOk && !_timeOk) {
        Serial.println("[TIME] FATAL: No valid time source. Check WiFi credentials and RTC wiring.");
        return false;
    }
    return true;
}

DateTime getCurrentTimeUTC() {
    if (_rtcOk) {
        return _rtc.now();   // RTC is always kept in UTC
    }
    // Fallback: ESP32 internal SNTP clock (valid only after NTP has synced)
    time_t now = time(NULL);
    struct tm t;
    gmtime_r(&now, &t);
    return DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec);
}

bool isTimeForNTPResync(const DateTime& utc) {
    // Fire once per day at DAILY_NTP_RESYNC_H UTC, within the first 30 seconds
    return (utc.hour() == DAILY_NTP_RESYNC_H && utc.minute() == 0 && utc.second() < 30);
}

#endif // SIMULATE
