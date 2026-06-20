#pragma once
// Minimal RTClib shim — provides DateTime for native simulation builds.
// Only the fields and constructors used by this project are implemented.

#include <stdint.h>
#include <time.h>

class DateTime {
    uint32_t _unix;

    struct tm _decode() const {
        struct tm t = {};
        time_t u = (time_t)_unix;
        gmtime_r(&u, &t);
        return t;
    }

public:
    explicit DateTime(uint32_t unixtime = 0) : _unix(unixtime) {}

    DateTime(uint16_t year, uint8_t month, uint8_t day,
             uint8_t hour = 0, uint8_t min = 0, uint8_t sec = 0) {
        struct tm t = {};
        t.tm_year = year - 1900;
        t.tm_mon  = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min  = min;
        t.tm_sec  = sec;
        _unix = (uint32_t)timegm(&t);
    }

    uint32_t unixtime() const { return _unix; }
    uint16_t year()     const { return (uint16_t)(_decode().tm_year + 1900); }
    uint8_t  month()    const { return (uint8_t) (_decode().tm_mon  + 1);    }
    uint8_t  day()      const { return (uint8_t)  _decode().tm_mday;         }
    uint8_t  hour()     const { return (uint8_t)  _decode().tm_hour;         }
    uint8_t  minute()   const { return (uint8_t)  _decode().tm_min;          }
    uint8_t  second()   const { return (uint8_t)  _decode().tm_sec;          }
};

// Stub — real RTC hardware not used in simulation
class RTC_DS3231 {
public:
    bool     begin()              { return false; }
    bool     lostPower()          { return true;  }
    DateTime now()                { return DateTime(0); }
    void     adjust(const DateTime&) {}
};
