# SolarMax Interactive Simulator (terminal)

A lightweight terminal UI (FTXUI) that runs the **real control brain**
(`lib/tracker_core`) on a simulated sun and wind, so you can watch the panel
track and demonstrate it live.

It uses the *same* code as the firmware: the sun math (`lib/solar_math`) and the
state machine (`lib/tracker_core`) are compiled straight in — only the clock,
wind, and motor are simulated.

**Two views (press `v` to switch):**
- **Side view** — a flat E–W cross-section (sun ray + panel + normal).
- **3D wireframe sky dome** — a rotatable hemisphere with the three seasonal
  sun-path arcs, the live sun, and the panel plate. This is the terminal cousin
  of the full 3D window in [`../sim3d`](../sim3d) (raylib), which is the one to
  use for presentations.

A **Verify** panel (right) checks the live sun against known equator positions
and lights a row green when it matches — press `0` for the equator demo, then
`1`/`2`/`3`/`4`.

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
| `v` | cycle view: **side → 3D wireframe dome → angle/speed plot** |
| `g` | in the plot view: switch **panel angle** ⇄ **panel speed** vs time |
| `o` / `p` | rotate 3D view left / right (yaw) |
| `n` / `m` | tilt 3D view down / up (pitch) |
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
| `0` | jump to the **equator demo** (lat 0, lon 0, equinox noon) |
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
Press `0` for the **equator demo** (lat 0, lon 0), then a preset — the Verify
panel confirms these automatically:

| Preset | Expect (equator) |
|--------|------------------|
| `3` Equinox noon | sun at the **zenith** (elev ≈ 90°), panel ~flat |
| `1` June solstice noon | elev ≈ 66.5°, azimuth ≈ 0° (**N**) |
| `2` Dec solstice noon | elev ≈ 66.5°, azimuth ≈ 180° (**S**) |
| `4` Equinox sunrise | on the horizon **due east** (az ≈ 90°), panel pinned −30° |

At your real site (Tempe, the default before pressing `0`): summer-solstice noon
elevation ≈ 80°, winter ≈ 33°, equinox ≈ 56.6° (= 90 − latitude), panel ≈ 0°.
