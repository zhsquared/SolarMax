#pragma once
// SolarMax control brain — PURE logic, no Arduino/hardware dependencies.
// Both the real firmware (main.cpp) and the interactive simulator call step(),
// so the simulation runs the exact same state machine and tracking decisions.

#include "solar_position.h"

enum TrackerState { TS_INIT, TS_TRACKING, TS_STOW, TS_NIGHT, TS_ERROR };

inline const char* trackerStateName(TrackerState s) {
    switch (s) {
        case TS_INIT:     return "INIT";
        case TS_TRACKING: return "TRACKING";
        case TS_STOW:     return "STOW";
        case TS_NIGHT:    return "NIGHT";
        default:          return "ERROR";
    }
}

// Site + machine parameters (filled from config.h in the firmware, or live in the sim).
struct TrackerConfig {
    float lat = 33.4255f, lon = -111.94f;
    float axisTilt = 0.0f, axisAzimuth = 0.0f;   // roof/axis orientation
    float windStowMph = 20.0f, windResumeMph = 15.0f;
    float panelMin = -30.0f, panelMax = 30.0f, stowAngle = 0.0f;
};

// Everything the brain needs for one decision.
struct TrackerInputs {
    int    year = 2026, month = 6, day = 21;
    double hourUTC = 12.0;
    float  windMph = 0.0f;
    bool   timeValid = true;
};

// Result of one decision step.
struct TrackerStep {
    TrackerState state;
    float        targetAngle;   // commanded panel angle this step (deg)
    SolarAngles  sun;           // computed sun position (for display)
    const char*  note;          // transition message, or nullptr if no change
};

class TrackerCore {
public:
    TrackerState state = TS_INIT;
    TrackerStep step(const TrackerConfig& cfg, const TrackerInputs& in);
};
