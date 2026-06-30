// NOAA Solar Calculator algorithm — accuracy ~0.01°
// Reference: https://gml.noaa.gov/grad/solcalc/calcdetails.html
//
// Assumes a horizontal single-axis tracker with a North-South rotation axis.
// The panel tilts East (negative angles) in the morning and West (positive)
// in the afternoon. Angles are clamped to the limits defined in solar_position.h.

#include "solar_position.h"
#include <math.h>

static inline double d2r(double d) { return d * M_PI / 180.0; }
static inline double r2d(double r) { return r * 180.0 / M_PI; }

// Optimal rotation angle of a single-axis tracker whose axis has the given tilt
// (above horizontal) and azimuth (deg from north, clockwise). Returns degrees,
// positive = panel tilts west. Method: project the sun direction onto the plane
// perpendicular to the axis and take its angle from the "face-up" reference.
// Reduces exactly to the horizontal N-S formula when tilt=0, azimuth=0.
double panelAngleForAxis(double elevDeg, double azDeg,
                         double axisTiltDeg, double axisAzimuthDeg) {
    double el = d2r(elevDeg), az = d2r(azDeg);
    double bt = d2r(axisTiltDeg), ga = d2r(axisAzimuthDeg);

    // Unit vectors in East-North-Up coordinates
    double sE = cos(el)*sin(az), sN = cos(el)*cos(az), sU = sin(el);   // sun
    double aE = cos(bt)*sin(ga), aN = cos(bt)*cos(ga), aU = sin(bt);   // axis

    // n0 = "panel faces up" reference = Up projected onto the plane ⊥ axis
    double n0E = -aU*aE, n0N = -aU*aN, n0U = 1.0 - aU*aU;
    double n0mag = sqrt(n0E*n0E + n0N*n0N + n0U*n0U);
    if (n0mag < 1e-9) return 0.0;            // degenerate (near-vertical axis)
    n0E/=n0mag; n0N/=n0mag; n0U/=n0mag;

    // w = n0 × axis  → in-plane direction of positive (westward) rotation
    double wE = n0N*aU - n0U*aN, wN = n0U*aE - n0E*aU, wU = n0E*aN - n0N*aE;

    // Sun projected onto the plane ⊥ axis
    double dotSa = sE*aE + sN*aN + sU*aU;
    double pE = sE - dotSa*aE, pN = sN - dotSa*aN, pU = sU - dotSa*aU;

    return r2d(atan2(pE*wE + pN*wN + pU*wU, pE*n0E + pN*n0N + pU*n0U));
}

static double julianDay(int year, int month, int day, double hourUTC) {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    double jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    return jd + (hourUTC - 12.0) / 24.0;
}

SolarAngles calculateSolarPositionRaw(int year, int month, int day,
                                       double hourUTC, float lat_f, float lon_f,
                                       float axisTiltDeg, float axisAzimuthDeg) {
    SolarAngles result = {0.0f, 0.0f, 0.0f, false};

    const double lat = (double)lat_f;
    const double lon = (double)lon_f;

    double JD  = julianDay(year, month, day, hourUTC);
    double JC  = (JD - 2451545.0) / 36525.0;

    // Geometric mean longitude and anomaly (degrees)
    double L0   = fmod(280.46646 + JC * (36000.76983 + JC * 0.0003032), 360.0);
    double M    = fmod(357.52911 + JC * (35999.05029 - 0.0001537 * JC), 360.0);
    double Mrad = d2r(M);

    // Equation of center → sun's true and apparent longitude
    double C      = sin(Mrad)   * (1.914602 - JC * (0.004817 + 0.000014 * JC))
                  + sin(2*Mrad) * (0.019993 - 0.000101 * JC)
                  + sin(3*Mrad) *  0.000289;
    double omega  = 125.04 - 1934.136 * JC;
    double lambda = d2r(L0 + C - 0.00569 - 0.00478 * sin(d2r(omega)));

    // Corrected obliquity of the ecliptic
    double eps0 = 23.0 + (26.0 + (21.448 - JC * (46.815 + JC * (0.00059 - JC * 0.001813))) / 60.0) / 60.0;
    double eps  = d2r(eps0 + 0.00256 * cos(d2r(omega)));

    // Solar declination
    double dec  = asin(sin(eps) * sin(lambda));

    // Equation of time (minutes)
    double e    = 0.016708634 - JC * (0.000042037 + 0.0000001267 * JC);
    double y    = tan(eps / 2.0) * tan(eps / 2.0);
    double L0r  = d2r(L0);
    double EoT  = 4.0 * r2d(
          y * sin(2*L0r)
        - 2*e  * sin(Mrad)
        + 4*e*y * sin(Mrad)  * cos(2*L0r)
        - 0.5*y*y * sin(4*L0r)
        - 1.25*e*e * sin(2*Mrad));

    // True solar time (minutes). dt is UTC so no timezone term needed.
    double trueSolarTime = fmod(hourUTC * 60.0 + EoT + 4.0 * lon, 1440.0);
    if (trueSolarTime < 0) trueSolarTime += 1440.0;

    // Hour angle: negative = morning, positive = afternoon
    double HA     = trueSolarTime / 4.0 - 180.0;
    double HArad  = d2r(HA);
    double latRad = d2r(lat);

    // Solar zenith and elevation
    double cosZ = fmax(-1.0, fmin(1.0,
                  sin(latRad)*sin(dec) + cos(latRad)*cos(dec)*cos(HArad)));
    double zenith    = r2d(acos(cosZ));
    double elevation = 90.0 - zenith;

    // Solar azimuth (degrees from north, clockwise)
    double azimuth = 0.0;
    double sinZ    = sin(d2r(zenith));
    if (sinZ > 1e-6) {
        double cosAz = fmax(-1.0, fmin(1.0,
                       (sin(latRad) * cosZ - sin(dec)) / (cos(latRad) * sinZ)));
        double azAcos = r2d(acos(cosAz));
        azimuth = (HA > 0) ? fmod(azAcos + 180.0, 360.0)
                           : fmod(540.0 - azAcos,  360.0);
    }

    // Panel tracking angle about the (possibly tilted) rotation axis.
    // Negative = tilt east (morning), Positive = tilt west (afternoon).
    float panelAngle = 0.0f;
    if (elevation > 1.0) {
        double r = panelAngleForAxis(elevation, azimuth, axisTiltDeg, axisAzimuthDeg);
        panelAngle = (float)fmax((double)PANEL_ANGLE_MIN, fmin((double)PANEL_ANGLE_MAX, r));
    }

    result.elevation    = (float)elevation;
    result.azimuth      = (float)azimuth;
    result.panelAngle   = panelAngle;
    result.aboveHorizon = elevation >= SUN_MIN_ELEV_DEG;
    return result;
}

// DateTime convenience wrapper — excluded from native builds.
#ifndef NATIVE_BUILD
#include <RTClib.h>
SolarAngles calculateSolarPosition(const DateTime& dt, float lat, float lon,
                                   float axisTiltDeg, float axisAzimuthDeg) {
    double h = dt.hour() + dt.minute() / 60.0 + dt.second() / 3600.0;
    return calculateSolarPositionRaw(dt.year(), dt.month(), dt.day(), h, lat, lon,
                                     axisTiltDeg, axisAzimuthDeg);
}
#endif
