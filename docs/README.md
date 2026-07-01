# SolarMax Documentation

SolarMax is a single-axis solar tracker. Instead of light sensors, it **computes
the sun's position from date, time, and location** (NOAA algorithm) and points the
panel to match — with automatic high-wind stow and night park.

## Start here
| Doc | What it covers |
|-----|----------------|
| [architecture.md](architecture.md) | The big picture: modules, layers, build targets, the state machine |
| [data_flow.md](data_flow.md) | How data moves each control cycle (time → sun → decision → motor) |
| [sun_tracking_math.md](sun_tracking_math.md) | **Where the formulas are** and how they work (NOAA + tilted-axis) |
| [usage.md](usage.md) | Build, flash, simulate, test, and configure the repo |
| [simulation.md](simulation.md) | The two simulators and how the fake hardware works |

## Hardware docs (already in this folder)
| Doc | What it covers |
|-----|----------------|
| [wiring.md](wiring.md) | Full wiring reference (verified against datasheets) |
| [wiring_diagram.pdf](wiring_diagram.pdf) | Schematic: power, signals, dividers |
| [wiring_breadboard.pdf](wiring_breadboard.pdf) | Breadboard "look-and-connect" layout |
| [breadboard_placement.md](breadboard_placement.md) | Column-by-column placement map |
| [links_to_parts.md](links_to_parts.md) | Bill of materials with purchase links |

## Repository map
```
SolarMax/
├── src/           firmware (ESP32): main loop, drivers, diagnostics, sim/bring-up tools
├── lib/
│   ├── solar_math/    NOAA sun position + tilted-axis panel angle (pure, testable)
│   ├── tracker_core/  the control "brain": state machine (pure, shared with the sim)
│   └── arduino_compat/ Arduino/RTClib shims so firmware compiles natively
├── include/config.h   ALL settings (pins, location, thresholds, roof angle)
├── sim/           interactive FTXUI terminal simulator (CMake)
├── test/          native unit tests (sun math, pin validation)
├── docs/          this folder
└── platformio.ini build environments
```
