#pragma once
// Simulation constants — used only by hal_sim.cpp.
// Build with: pio run -e simulate

#ifdef SIMULATE

#define SIM_SPEED_FACTOR  120UL   // 1 real second = 120 simulated seconds (~12 min/day)

// Simulated day starts 2026-06-21 07:00 UTC (= midnight local, AZ = UTC-7)
#define SIM_START_YEAR   2026
#define SIM_START_MONTH     6
#define SIM_START_DAY      21
#define SIM_START_HOUR      7
#define SIM_START_MIN       0
#define SIM_START_SEC       0

#endif // SIMULATE
