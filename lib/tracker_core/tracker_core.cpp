#include "tracker_core.h"

// One decision step. Mirrors the original main.cpp state machine exactly, but as
// pure logic: it returns the target angle and any transition note instead of
// driving motors or printing — the caller does the I/O.
TrackerStep TrackerCore::step(const TrackerConfig& c, const TrackerInputs& in) {
    TrackerStep out;
    out.note = nullptr;

    if (!in.timeValid) state = TS_ERROR;

    SolarAngles sa = calculateSolarPositionRaw(in.year, in.month, in.day, in.hourUTC,
                                               c.lat, c.lon, c.axisTilt, c.axisAzimuth);
    out.sun = sa;
    out.targetAngle = c.panelMin;   // safe default (homed east)

    switch (state) {
        case TS_INIT:
            out.targetAngle = c.panelMin;            // home to east limit
            state    = sa.aboveHorizon ? TS_TRACKING : TS_NIGHT;
            out.note = sa.aboveHorizon ? "INIT -> TRACKING" : "INIT -> NIGHT";
            break;

        case TS_TRACKING:
            if (in.windMph >= c.windStowMph) {
                out.targetAngle = c.stowAngle;
                state = TS_STOW;  out.note = "TRACKING -> STOW (high wind)";
            } else if (!sa.aboveHorizon) {
                out.targetAngle = c.panelMin;
                state = TS_NIGHT; out.note = "TRACKING -> NIGHT (sunset)";
            } else {
                out.targetAngle = sa.panelAngle;     // follow the sun
            }
            break;

        case TS_STOW:
            out.targetAngle = c.stowAngle;           // hold flat
            if (in.windMph < c.windResumeMph) {
                if (sa.aboveHorizon) {
                    out.targetAngle = sa.panelAngle;
                    state = TS_TRACKING; out.note = "STOW -> TRACKING";
                } else {
                    out.targetAngle = c.panelMin;
                    state = TS_NIGHT;    out.note = "STOW -> NIGHT";
                }
            }
            break;

        case TS_NIGHT:
            out.targetAngle = c.panelMin;            // parked east
            if (sa.aboveHorizon) {
                out.targetAngle = sa.panelAngle;
                state = TS_TRACKING; out.note = "NIGHT -> TRACKING (sunrise)";
            }
            break;

        case TS_ERROR:
            out.targetAngle = c.stowAngle;
            break;
    }

    out.state = state;
    return out;
}
