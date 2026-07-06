#pragma once
// SolarMax — portable simulated UTC clock.
//
// The simulators track a moment in time and need to (a) advance it smoothly and
// (b) read it back as calendar fields for the solar math. We deliberately avoid
// timegm()/gmtime_r() because those are POSIX-only and do NOT compile on Windows
// (MSVC). Instead we use Howard Hinnant's well-known civil<->days algorithms,
// which are exact, branch-simple, and fully portable (Windows + macOS + Linux).

#include <cmath>

struct SimDate {
    int    year = 2026, month = 6, day = 21;
    int    hour = 12, minute = 15;
    double second = 0.0;
};

// Days since 1970-01-01 for a proleptic-Gregorian date. y/m/d as a normal date.
inline long long daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned  yoe = (unsigned)(y - era * 400);                 // [0, 399]
    unsigned  doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    unsigned  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;      // [0, 146096]
    return era * 146097LL + (long long)doe - 719468LL;
}

// Inverse of daysFromCivil: turn a day count back into y/m/d.
inline void civilFromDays(long long z, int& y, int& m, int& d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned  doe = (unsigned)(z - era * 146097);              // [0, 146096]
    unsigned  yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    y = (int)(yoe + era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);     // [0, 365]
    unsigned mp  = (5 * doy + 2) / 153;                         // [0, 11]
    d = (int)(doy - (153 * mp + 2) / 5 + 1);                    // [1, 31]
    m = (int)(mp + (mp < 10 ? 3 : -9));                         // [1, 12]
    y += (m <= 2);
}

// Seconds since the 1970 epoch (UTC). This is our single scalar "sim time".
inline double toEpoch(const SimDate& t) {
    return (double)daysFromCivil(t.year, t.month, t.day) * 86400.0
         + t.hour * 3600.0 + t.minute * 60.0 + t.second;
}

// Turn a scalar sim time back into calendar fields.
inline SimDate fromEpoch(double sec) {
    long long days = (long long)std::floor(sec / 86400.0);
    double    rem  = sec - (double)days * 86400.0;
    SimDate t;
    civilFromDays(days, t.year, t.month, t.day);
    t.hour   = (int)(rem / 3600.0); rem -= t.hour * 3600.0;
    t.minute = (int)(rem / 60.0);   rem -= t.minute * 60.0;
    t.second = rem;
    return t;
}

// Decimal UTC hour (e.g. 19:30 -> 19.5) — what the solar math wants.
inline double hourUTCof(const SimDate& t) {
    return t.hour + t.minute / 60.0 + t.second / 3600.0;
}

// Convenience: epoch for a given Y/M/D H:M (UTC).
inline double epochOf(int y, int mo, int d, int h, int mi) {
    SimDate t; t.year=y; t.month=mo; t.day=d; t.hour=h; t.minute=mi; t.second=0;
    return toEpoch(t);
}
