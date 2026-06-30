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
// Rotation-axis orientation as installed. Defaults describe a flat, level,
// North-South horizontal axis (the original behavior). On a sloped roof, set the
// axis tilt (degrees above horizontal) and azimuth (compass dir its raised end faces).
#ifndef AXIS_TILT_DEG
#define AXIS_TILT_DEG      0.0f
#endif
#ifndef AXIS_AZIMUTH_DEG
#define AXIS_AZIMUTH_DEG   0.0f
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
// axisTiltDeg / axisAzimuthDeg describe the rotation axis as installed on the roof
// (default = flat, level, N-S). The panel angle is the rotation about that axis.
SolarAngles calculateSolarPositionRaw(int year, int month, int day,
                                       double hourUTC, float lat, float lon,
                                       float axisTiltDeg = AXIS_TILT_DEG,
                                       float axisAzimuthDeg = AXIS_AZIMUTH_DEG);

// Optimal single-axis rotation angle (deg, +=west) for a sun at (elevDeg, azDeg)
// and a rotation axis with the given tilt/azimuth. Exposed for the simulator/tests.
double panelAngleForAxis(double elevDeg, double azDeg,
                         double axisTiltDeg, double axisAzimuthDeg);

// Arduino/ESP32 convenience wrapper — not available in native unit tests.
#ifndef NATIVE_BUILD
#include <RTClib.h>
SolarAngles calculateSolarPosition(const DateTime& dt, float lat, float lon,
                                    float axisTiltDeg = AXIS_TILT_DEG,
                                    float axisAzimuthDeg = AXIS_AZIMUTH_DEG);
#endif
