# SolarMax Interactive Simulator (terminal)

A lightweight terminal UI (FTXUI) that runs the **real control brain**
(`lib/tracker_core`) on a simulated sun and wind, so you can watch the panel
track and demonstrate it live.

It uses the *same* code as the firmware: the sun math (`lib/solar_math`) and the
state machine (`lib/tracker_core`) are compiled straight in — only the clock,
wind, and motor are simulated.

## Build & run
```bash
cd sim
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # first time: downloads FTXUI
cmake --build build -j4
./build/solarmax_sim
```
(Needs cmake + a C++17 compiler + internet on first configure to fetch FTXUI.)

## Controls
| Key | Action |
|-----|--------|
| `space` | play / pause time |
| `j` / `k` | slower / faster |
| `,` `.` (or ← →) | scrub time ±10 min |
| `-` `=` | wind down / up |
| `[` `]` | axis tilt − / + (roof pitch) |
| `<` `>` | axis azimuth − / + (roof facing) |
| `a` `z` | latitude + / − |
| `1` | jump to **summer solstice** noon |
| `2` | jump to **winter solstice** noon |
| `3` | jump to **equinox** noon |
| `4` | jump to **sunrise** |
| `q` | quit |

## What to look for
- **Tracking:** the cyan **panel normal** points at the yellow **sun**; the green
  panel surface tilts to follow it through the day.
- **Limits:** near sunrise/sunset the panel pins at ±30° (mechanical limit).
- **Wind stow:** raise wind past the stow threshold → state turns **STOW** and the
  panel returns flat; lower it to resume.
- **Night:** after sunset the state goes **NIGHT** and the panel parks east.
- **Roof angle:** change axis tilt/azimuth and watch the tracking adjust.

## Sanity checks (known sun positions)
| Preset | Expect |
|--------|--------|
| Summer solstice noon (Tempe) | elevation ≈ 80°, panel ≈ 0° |
| Winter solstice noon | elevation ≈ 33°, panel ≈ 0° |
| Equinox noon | elevation ≈ 56.6° (= 90 − latitude), panel ≈ 0° |
| Sunrise | low sun in the east, panel pinned to −30° |
