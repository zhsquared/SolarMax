#pragma once
#include <RTClib.h>

// Initialize I2C, detect RTC, attempt NTP sync.
// Returns false only if no valid time source is available at all.
bool     timeManagerInit();

// Connect to WiFi, sync NTP, update RTC if present. Returns true on success.
bool     syncNTP();

// Returns current UTC time (from RTC if available, else ESP32 internal SNTP).
DateTime getCurrentTimeUTC();

// True when it's time for the daily NTP re-sync (avoids drift accumulation).
bool     isTimeForNTPResync(const DateTime& utc);
