#pragma once

// Mechanical and tracking defaults — override in your project's config header
// by defining these before including solar_position.h.
#ifndef PANEL_ANGLE_MIN
#define PANEL_ANGLE_MIN  -30.0f
#endif
#ifndef PANEL_ANGLE_MAX
#define PANEL_ANGLE_MAX   30.0f
#endif
#ifndef SUN_MIN_ELEV_DEG
#define SUN_MIN_ELEV_DEG   5.0f
#endif

struct SolarAngles {
    float elevation;     // Degrees above horizon (negative = below horizon)
    float azimuth;       // Degrees from north clockwise (0=N, 90=E, 180=S, 270=W)
    float panelAngle;    // Target panel rotation in degrees (negative=east, positive=west)
    bool  aboveHorizon;  // True when sun is high enough to track
};

// Core astronomical algorithm — no Arduino or RTClib dependency.
// Safe to call from native unit tests.
// hourUTC: decimal hours in UTC (e.g. 19.5 = 19:30 UTC)
// lat/lon: decimal degrees, negative lon = West
SolarAngles calculateSolarPositionRaw(int year, int month, int day,
                                       double hourUTC, float lat, float lon);

// Arduino/ESP32 convenience wrapper — not available in native unit tests.
#ifndef NATIVE_BUILD
#include <RTClib.h>
SolarAngles calculateSolarPosition(const DateTime& dt, float lat, float lon);
#endif
