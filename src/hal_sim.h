#pragma once
// Simulation constants — used only by hal_sim.cpp.
// Build with: pio run -e simulate

#ifdef SIMULATE

#define SIM_SPEED_FACTOR  120UL   // 1 real second = 120 simulated seconds (~12 min/day)

// Simulated day starts 2026-06-21 12:15 UTC (= 5:15 AM local, ~8 min before sunrise)
#define SIM_START_YEAR   2026
#define SIM_START_MONTH     6
#define SIM_START_DAY      21
#define SIM_START_HOUR     12
#define SIM_START_MIN      15
#define SIM_START_SEC       0

#endif // SIMULATE
