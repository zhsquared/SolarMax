# Simulation

SolarMax has **two** simulators. Both run the real control brain
(`lib/tracker_core` + `lib/solar_math`) — only the hardware is faked.

| | Native auto-runner | Interactive (FTXUI) |
|--|-------------------|---------------------|
| Build | `pio run -e simulate` | `cd sim && cmake …` |
| Run | `.pio/build/simulate/program` | `./build/solarmax_sim` |
| Style | Prints a full day and scrolls | Live TUI you control |
| Controls | none (auto) | scrub time, wind, roof angle, presets |
| Best for | quick "does it work" check | demos, presentations, exploring |

## How the fake hardware works
The firmware never touches pins directly — it calls functions like
`getCurrentTimeUTC()`, `readWindSpeedMPH()`, and `driveToAngle()`. In a simulation
build those names resolve to **fake implementations** instead of the real drivers.
The control logic can't tell the difference.

Each fake value comes from a tiny model:
- **Time** — the computer's real clock, scaled up (e.g. 120×), added to a start date.
- **Sun** — *not faked.* The real NOAA math is fed the fake clock, so the sun is genuine physics on a sped-up day.
- **Panel angle** — a single variable the fake motor stores/returns (a perfect motor).
- **Wind** — a scripted value (native runner) or a slider you control (FTXUI).
- **Limit switches / LDRs** — derived from the panel angle / constant.

## Native auto-runner (`simulate`)
Starts just before sunrise on the summer solstice and plays a full day at 120×
(~12 real minutes). Prints the boot self-check, then sun position, motor moves, and
state changes (tracking → wind-stow → night). Good for a fast confidence check.

```bash
pio run -e simulate && .pio/build/simulate/program   # Ctrl+C to stop
```

## Interactive simulator (`sim/`)
A terminal UI (FTXUI) with a live side-view of the sun vs. the panel, the current
state, sun angles, wind, and roof settings — all controllable from the keyboard.

```bash
cd sim
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # first time: downloads FTXUI
cmake --build build -j4
./build/solarmax_sim
```

Controls and what to look for are in [../sim/README.md](../sim/README.md). Highlights:
- `1`–`4` jump to summer/winter solstice, equinox, sunrise to check tracking against
  known sun angles.
- `-` / `=` change wind to trigger and clear **STOW**.
- `[` `]` / `<` `>` change **roof tilt / azimuth** and watch tracking adjust.

## Why two?
The native runner is the zero-setup smoke test (and what CI-style checks lean on).
The FTXUI simulator is the teaching/demo tool — it makes the tracking, the safety
stow, and the roof-angle effect visible and interactive without any hardware.
