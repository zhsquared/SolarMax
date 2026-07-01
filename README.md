# SolarMax

ESP32 firmware for a single-axis solar tracker. It points the panel by **computing
the sun's position from date, time, and location** (NOAA Solar Calculator) instead
of using light sensors — with automatic high-wind stow and night park.

## 📖 Documentation
Full docs are in **[docs/](docs/README.md)**:
- [Architecture](docs/architecture.md) · [Data flow](docs/data_flow.md) · [Sun-tracking math](docs/sun_tracking_math.md)
- [Usage / build guide](docs/usage.md) · [Simulation](docs/simulation.md)
- [Wiring](docs/wiring.md) · [Bill of materials](docs/links_to_parts.md)

## Hardware
- ESP32 DevKit V1
- BTS7960 43 A H-bridge motor driver + DC worm-gear motor (potentiometer feedback)
- DS3231 RTC (I2C) + WiFi NTP for timekeeping
- Adafruit 1733 analog anemometer for wind-stow protection
- Limit switches, buck converter, fail-safe E-stop (see [docs/wiring.md](docs/wiring.md))

## Build targets
| Env / build | Command | Use |
|---|---|---|
| `esp32dev` | `pio run -e esp32dev -t upload` | Real firmware |
| `simulate` | `pio run -e simulate && .pio/build/simulate/program` | Native day simulation (no hardware) |
| `sim/` (CMake) | `cd sim && cmake -S . -B build && cmake --build build && ./build/solarmax_sim` | Interactive terminal simulator |
| `bringup` | `pio run -e bringup -t upload` | Validate each sensor |
| `native` | `pio test -e native` | Unit tests on your Mac |

## Configuration
All settings live in **[include/config.h](include/config.h)** — location, roof angle
(`AXIS_TILT_DEG` / `AXIS_AZIMUTH_DEG`), WiFi, thresholds, and pins. See
[docs/usage.md](docs/usage.md#configuration--everything-is-in-includeconfigh).

## State machine
`INIT → TRACKING → STOW` (high wind) → `TRACKING`/`NIGHT` (sun down) → `TRACKING`.
Details in [docs/architecture.md](docs/architecture.md#state-machine).

## Tests
```bash
pio test -e native
```
Covers solar angles at the solstices/equinox, panel sign convention, limit clamping,
and ESP32 pin validation.
