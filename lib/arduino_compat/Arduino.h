#pragma once
// Minimal Arduino API shim for native (Mac) simulation builds.
// Only compiled when platform = native (enforced by library.json).

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

// ── Primitive types & constants ───────────────────────────────────────────────
typedef bool     boolean;
typedef uint8_t  byte;

#define HIGH    1
#define LOW     0
#define INPUT   0
#define OUTPUT  1
#define INPUT_PULLUP   2
#define INPUT_PULLDOWN 3
#define RISING  1
#define FALLING 2

#define IRAM_ATTR   // no-op on native
#define F(s) (s)    // no-op on native (Arduino flash-string helper)

// ── Timing ────────────────────────────────────────────────────────────────────
uint32_t millis();
void     delay(uint32_t ms);

// ── Math ──────────────────────────────────────────────────────────────────────
template<typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) {
    return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x);
}

// ── GPIO / peripheral stubs (no-ops) ─────────────────────────────────────────
inline void pinMode(int, int)               {}
inline void digitalWrite(int, int)          {}
inline int  digitalRead(int)                { return LOW; }
inline int  analogRead(int)                 { return 0; }
inline uint32_t analogReadMilliVolts(int)   { return 0; }
inline void analogReadResolution(int)       {}
inline void attachInterrupt(int, void(*)(), int) {}
inline int  digitalPinToInterrupt(int p)    { return p; }
inline void ledcSetup(int, int, int)        {}
inline void ledcAttachPin(int, int)         {}
inline void ledcWrite(int, int)             {}
inline void configTime(int, int, const char*, const char* = nullptr) {}
inline bool getLocalTime(struct tm*, int = 0)   { return false; }
#define WL_CONNECTED 3

// ── Serial ────────────────────────────────────────────────────────────────────
class SerialClass {
public:
    void begin(int) {}

    void print(const char* s)   { fputs(s, stdout); fflush(stdout); }
    void println(const char* s) { puts(s); fflush(stdout); }
    void println()              { putchar('\n'); fflush(stdout); }

    void print(int v)    { printf("%d",    v); fflush(stdout); }
    void println(int v)  { printf("%d\n",  v); fflush(stdout); }
    void print(float v)  { printf("%.2f",  v); fflush(stdout); }
    void println(float v){ printf("%.2f\n",v); fflush(stdout); }

    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
    }
};
extern SerialClass Serial;

// ── WiFi stub ─────────────────────────────────────────────────────────────────
class WiFiClass {
public:
    void begin(const char*, const char*) {}
    int  status()              { return WL_CONNECTED; }
    void disconnect(bool = false) {}
};
extern WiFiClass WiFi;

// ── Wire (I2C) stub ───────────────────────────────────────────────────────────
class TwoWire {
public:
    void begin(int, int) {}
};
extern TwoWire Wire;
