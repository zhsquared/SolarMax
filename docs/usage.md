# Using the Repo

## Prerequisites
- **PlatformIO** (VS Code extension or CLI) — for firmware, native sim, and tests.
- **CMake + a C++17 compiler** — only for the interactive FTXUI simulator.

> If `pio` isn't on your PATH, use the full path: `~/.platformio/penv/bin/pio`.

## The build targets (cheat sheet)
| I want to… | Command |
|------------|---------|
| Run the interactive simulator | `cd sim && cmake -S . -B build && cmake --build build -j4 && ./build/solarmax_sim` |
| Auto-run a simulated day (no hardware) | `pio run -e simulate && .pio/build/simulate/program` |
| Run the unit tests | `pio test -e native` |
| Flash the real firmware | `pio run -e esp32dev -t upload` |
| Flash a serial demo (fake sensors) | `pio run -e simulate_hw -t upload && pio device monitor` |
| Validate one sensor at a time | `pio run -e bringup -t upload && pio device monitor` |

## Typical workflows

### Develop / demo without hardware
Use the **interactive simulator** ([simulation.md](simulation.md)) — it runs the real
control logic and lets you scrub time, jump to solstices, and change wind/roof angle.

### Bring up real hardware (first assembly)
1. Wire per [wiring.md](wiring.md).
2. Flash `bringup` and verify each part: RTC (I2C scan → 0x68), potentiometer, limit
   switches, anemometer.
3. Run the potentiometer calibration to find `POT_ADC_MIN` / `POT_ADC_MAX` (see
   `runCalibration()` in `motor_control`), update `config.h`.
4. Flash `esp32dev`. On boot it prints a **self-check** report before tracking.

## Configuration — everything is in `include/config.h`
Edit these before flashing a real install:

| Setting | Meaning |
|---------|---------|
| `LATITUDE`, `LONGITUDE` | Install site (decimal degrees; negative longitude = West) |
| `TIMEZONE_OFFSET` | Hours from UTC (display only) |
| `AXIS_TILT_DEG`, `AXIS_AZIMUTH_DEG` | **Roof angle** — how the rotation axis is mounted (0/0 = flat, N-S) |
| `WIFI_SSID`, `WIFI_PASS` | For NTP time sync |
| `PANEL_ANGLE_MIN/MAX` | Mechanical travel limits (±30°) |
| `POT_ADC_MIN/MAX` | Potentiometer calibration (from bring-up) |
| `WIND_STOW_MPH`, `WIND_RESUME_MPH` | Stow / resume thresholds |
| `SUN_MIN_ELEV_DEG` | Below this = night mode |
| Pin `#define`s | GPIO assignments (see [wiring.md](wiring.md)) |

## Tests & CI
- `pio test -e native` runs two suites:
  - **test_solar** — sun angles vs. known solstice/equinox values.
  - **test_pins** — every `config.h` pin checked against ESP32 constraints (input-only,
    ADC1, collisions, strapping pins).
- CI runs on push/PR (see `.github/workflows/`).

## Repo layout
See the map in [README.md](README.md#repository-map).
