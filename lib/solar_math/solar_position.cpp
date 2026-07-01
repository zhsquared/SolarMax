// NOAA Solar Calculator algorithm — accuracy ~0.01°
// Reference: https://gml.noaa.gov/grad/solcalc/calcdetails.html
//
// Big picture: given a date, time (UTC) and a location on Earth, we work out
// where the Sun is in the sky (its elevation and azimuth), then work out how
// far to rotate the solar panel so it faces the Sun.
//
// Assumes a horizontal single-axis tracker with a North-South rotation axis.
// The panel tilts East (negative angles) in the morning and West (positive)
// in the afternoon. Angles are clamped to the limits defined in solar_position.h.

#include "solar_position.h"
#include <math.h>

// Small helpers: convert between degrees and radians. Trig functions in C work
// in radians, but humans (and this file) mostly think in degrees.
static inline double d2r(double d) { return d * M_PI / 180.0; }   // degrees -> radians
static inline double r2d(double r) { return r * 180.0 / M_PI; }   // radians -> degrees

// Optimal rotation angle of a single-axis tracker whose axis has the given tilt
// (above horizontal) and azimuth (deg from north, clockwise). Returns degrees,
// positive = panel tilts west. Method: project the sun direction onto the plane
// perpendicular to the axis and take its angle from the "face-up" reference.
// Reduces exactly to the horizontal N-S formula when tilt=0, azimuth=0.
double panelAngleForAxis(double elevDeg, double azDeg,
                         double axisTiltDeg, double axisAzimuthDeg) {
    double el = d2r(elevDeg), az = d2r(azDeg);      // el, az: sun's elevation & azimuth, in radians
    double bt = d2r(axisTiltDeg), ga = d2r(axisAzimuthDeg);  // bt, ga: axis tilt & azimuth, in radians

    // Describe both directions as unit vectors in East-North-Up (ENU) coordinates.
    // Each vector points "toward" the sun / along the rotation axis.
    double sE = cos(el)*sin(az), sN = cos(el)*cos(az), sU = sin(el);   // s = unit vector toward the sun
    double aE = cos(bt)*sin(ga), aN = cos(bt)*cos(ga), aU = sin(bt);   // a = unit vector along the rotation axis

    // n0 = the "panel faces straight up" reference direction. It is the Up vector
    // with its along-axis part removed, so it lies in the plane the panel sweeps
    // through as it rotates. (n0E, n0N, n0U) are its East/North/Up components.
    double n0E = -aU*aE, n0N = -aU*aN, n0U = 1.0 - aU*aU;
    double n0mag = sqrt(n0E*n0E + n0N*n0N + n0U*n0U);   // n0mag: length of n0, used to normalise it
    if (n0mag < 1e-9) return 0.0;            // degenerate (near-vertical axis)
    n0E/=n0mag; n0N/=n0mag; n0U/=n0mag;      // scale n0 to unit length

    // w = n0 × axis: a second in-plane reference, perpendicular to n0. It points
    // in the direction of positive (westward) rotation, so it lets us measure
    // signed rotation angle. (wE, wN, wU) are its East/North/Up components.
    double wE = n0N*aU - n0U*aN, wN = n0U*aE - n0E*aU, wU = n0E*aN - n0N*aE;

    // Project the sun vector onto the plane perpendicular to the axis by removing
    // its along-axis component. Only this in-plane part affects panel rotation.
    double dotSa = sE*aE + sN*aN + sU*aU;              // dotSa: how much of the sun lies along the axis
    double pE = sE - dotSa*aE, pN = sN - dotSa*aN, pU = sU - dotSa*aU;   // p = sun projected into the panel's plane

    // Angle of the projected sun measured from n0 (face-up) toward w (west).
    // atan2 gives the correct signed angle in all four quadrants.
    return r2d(atan2(pE*wE + pN*wN + pU*wU, pE*n0E + pN*n0N + pU*n0U));
}

// Convert a calendar date + UTC hour into a Julian Day number: a continuous count
// of days used in astronomy so that time arithmetic is easy. The integer formula
// handles the Gregorian calendar (leap years, century rules) exactly.
static double julianDay(int year, int month, int day, double hourUTC) {
    int a = (14 - month) / 12;      // a, y, m: integer helpers that shift Jan/Feb into the previous year
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    double jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;  // jd: whole-day part
    return jd + (hourUTC - 12.0) / 24.0;    // add the fractional day (Julian days start at noon)
}

SolarAngles calculateSolarPositionRaw(int year, int month, int day,
                                       double hourUTC, float lat_f, float lon_f,
                                       float axisTiltDeg, float axisAzimuthDeg) {
    SolarAngles result = {0.0f, 0.0f, 0.0f, false};

    const double lat = (double)lat_f;   // lat: observer latitude (deg, + = north)
    const double lon = (double)lon_f;   // lon: observer longitude (deg, + = east)

    // Time inputs for every formula below.
    double JD  = julianDay(year, month, day, hourUTC);   // JD: Julian Day for the requested instant
    double JC  = (JD - 2451545.0) / 36525.0;             // JC: Julian Centuries since the J2000.0 epoch

    // Where the Sun would be if Earth's orbit were a perfect circle (degrees).
    double L0   = fmod(280.46646 + JC * (36000.76983 + JC * 0.0003032), 360.0);   // L0: geometric mean longitude of the Sun
    double M    = fmod(357.52911 + JC * (35999.05029 - 0.0001537 * JC), 360.0);   // M: geometric mean anomaly of the Sun
    double Mrad = d2r(M);   // Mrad: M in radians, for the trig terms below

    // Equation of center: the correction (degrees) for Earth's orbit actually being
    // an ellipse, which speeds the Sun up and slows it down through the year.
    double C      = sin(Mrad)   * (1.914602 - JC * (0.004817 + 0.000014 * JC))
                  + sin(2*Mrad) * (0.019993 - 0.000101 * JC)
                  + sin(3*Mrad) *  0.000289;
    double omega  = 125.04 - 1934.136 * JC;   // omega: longitude of the Moon's ascending node, drives the nutation wobble
    // Apparent longitude: true longitude (L0 + C) adjusted for nutation and the
    // aberration of light, giving the Sun's true apparent position (radians).
    double lambda = d2r(L0 + C - 0.00569 - 0.00478 * sin(d2r(omega)));   // lambda: apparent ecliptic longitude of the Sun

    // Obliquity = the tilt of Earth's rotation axis relative to its orbit; this is
    // what causes the seasons. eps0 is the mean tilt, eps adds a small nutation term.
    double eps0 = 23.0 + (26.0 + (21.448 - JC * (46.815 + JC * (0.00059 - JC * 0.001813))) / 60.0) / 60.0;  // eps0: mean obliquity (deg)
    double eps  = d2r(eps0 + 0.00256 * cos(d2r(omega)));   // eps: corrected obliquity, in radians

    // Solar declination: how far north (+) or south (-) of the equator the Sun is.
    double dec  = asin(sin(eps) * sin(lambda));   // dec: solar declination, in radians

    // Equation of time: the gap (in minutes) between clock-style "mean" solar time
    // and the Sun's actual position, caused by orbit eccentricity plus axial tilt.
    double e    = 0.016708634 - JC * (0.000042037 + 0.0000001267 * JC);   // e: eccentricity of Earth's orbit
    double y    = tan(eps / 2.0) * tan(eps / 2.0);   // y: tan^2(eps/2), an auxiliary term for the equation of time
    double L0r  = d2r(L0);   // L0r: mean longitude in radians
    double EoT  = 4.0 * r2d(
          y * sin(2*L0r)
        - 2*e  * sin(Mrad)
        + 4*e*y * sin(Mrad)  * cos(2*L0r)
        - 0.5*y*y * sin(4*L0r)
        - 1.25*e*e * sin(2*Mrad));   // EoT: equation of time, in minutes

    // True solar time: the local time defined by the actual Sun (minutes past midnight).
    // 4*lon converts longitude to a time offset (Earth turns 1° every 4 minutes).
    // dt is UTC so no timezone term needed.
    double trueSolarTime = fmod(hourUTC * 60.0 + EoT + 4.0 * lon, 1440.0);
    if (trueSolarTime < 0) trueSolarTime += 1440.0;   // wrap into the 0..1440 minute range

    // Hour angle: how far the Sun is from due south, in degrees.
    // Negative = morning (Sun in the east), positive = afternoon (Sun in the west).
    double HA     = trueSolarTime / 4.0 - 180.0;   // HA: solar hour angle (deg); /4 undoes the 4 min-per-degree scaling
    double HArad  = d2r(HA);    // HArad: hour angle in radians
    double latRad = d2r(lat);   // latRad: latitude in radians

    // Zenith angle = angle between the Sun and straight up; elevation = angle above
    // the horizon. The clamp keeps rounding error from pushing cosZ outside [-1, 1].
    double cosZ = fmax(-1.0, fmin(1.0,
                  sin(latRad)*sin(dec) + cos(latRad)*cos(dec)*cos(HArad)));   // cosZ: cosine of the zenith angle
    double zenith    = r2d(acos(cosZ));   // zenith: angle from vertical (deg)
    double elevation = 90.0 - zenith;     // elevation: angle above the horizon (deg)

    // Azimuth: the Sun's compass bearing, measured in degrees from north, going clockwise.
    double azimuth = 0.0;                  // azimuth: result, degrees from north (clockwise)
    double sinZ    = sin(d2r(zenith));     // sinZ: sine of the zenith, used as the denominator below
    if (sinZ > 1e-6) {                     // skip the undefined case when the Sun is exactly overhead
        double cosAz = fmax(-1.0, fmin(1.0,
                       (sin(latRad) * cosZ - sin(dec)) / (cos(latRad) * sinZ)));   // cosAz: cosine of the azimuth
        double azAcos = r2d(acos(cosAz));  // azAcos: raw azimuth (0..180) before choosing east vs west half
        // acos only returns 0..180, so use the hour angle to pick the correct half of the sky.
        azimuth = (HA > 0) ? fmod(azAcos + 180.0, 360.0)
                           : fmod(540.0 - azAcos,  360.0);
    }

    // Panel tracking angle about the (possibly tilted) rotation axis.
    // Negative = tilt east (morning), Positive = tilt west (afternoon).
    float panelAngle = 0.0f;               // panelAngle: commanded rotation of the panel (deg)
    if (elevation > 1.0) {                 // only track when the Sun is meaningfully above the horizon
        double r = panelAngleForAxis(elevation, azimuth, axisTiltDeg, axisAzimuthDeg);   // r: ideal angle before limits
        panelAngle = (float)fmax((double)PANEL_ANGLE_MIN, fmin((double)PANEL_ANGLE_MAX, r));   // clamp to mechanical limits
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
    double h = dt.hour() + dt.minute() / 60.0 + dt.second() / 3600.0;   // h: time of day as a fractional hour (UTC)
    return calculateSolarPositionRaw(dt.year(), dt.month(), dt.day(), h, lat, lon,
                                     axisTiltDeg, axisAzimuthDeg);
}
#endif
