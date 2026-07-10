# SolarMax 3D Sky-Dome Simulator (raylib)

A **real 3D** view of the sun crossing the sky dome and the panel tracking it -
built to *demonstrate the tracking code is correct*, not just to look nice.

It runs the **exact firmware math**: the sun path arcs and the panel angle are
computed by `lib/solar_math` and `lib/tracker_core`, the same code that runs on
the ESP32. Nothing here re-derives the astronomy.

![what you see: a hemisphere sky dome with three colored sun-path arcs, a sun
bead, and a green panel plate whose blue normal points at the sun]

## Why the equator?
It defaults to the **equator (lat 0, lon 0)**, because there the correct answers
are textbook-obvious and match the classic sun-path diagram (`sun_paths.jpg`):

| Preset (key) | Date / time (UTC) | Sun should be… | Panel should… |
|---|---|---|---|
| `3` Equinox noon | Mar 20, 12:00 | at the **zenith** (elev ≈ 90°) | sit ~flat, normal straight up |
| `1` June solstice noon | Jun 21, 12:00 | 23.5° **north** of zenith (elev ≈ 66.5°, az ≈ 0°) | ~flat (offset is along the axis) |
| `2` Dec solstice noon | Dec 21, 12:00 | 23.5° **south** of zenith (elev ≈ 66.5°, az ≈ 180°) | ~flat |
| `4` Equinox sunrise | Mar 20, 06:10 | on the horizon **due east** (az ≈ 90°) | pinned at its −30° limit |

The **Verify** panel (top-right) lists these known positions and a row turns
**green** when the live sun lands on it - so anyone can confirm correctness at a
glance. Press `0` any time to jump back to the equator demo.

> **Single-axis reminder:** at solstice noon the panel correctly sits *flat* even
> though the sun is 23.5° off - that offset is along the N–S rotation axis, which
> an east–west single-axis tracker cannot correct. That's physics, not a bug.

## Axis tilt: the manual seasonal set-angle
The rotation axis has a **tilt** that, in real operation, the operator sets **by
hand ~once a month** for best efficiency - separate from the motor, which drives
the daily east–west tracking automatically. The HUD's **PANEL TILT** block shows
two *different kinds* of tilt:

- **ideal now** - the live tilt to point **straight at the sun this instant**
  (`90° − elevation`, facing the sun's compass direction). It changes continuously
  through the day (facing E in the morning → W in the afternoon). This is a 2-axis
  *reference*, not a settable value - our single-axis machine only approximates it
  via the motor plus the seasonal tilt.
- **set `<month>`** - the **seasonal** tilt to dial in for the **upcoming** month
  (`β = |latitude − declination|`, facing the equator), taken at noon on the **15th
  of the next month** (e.g. in December it shows the **mid-January** angle, so you
  set it ahead for the best average energy across January).

**AUTO-TILT (`T`)** is a demonstration toggle: when **on**, the sim drives the axis
to that monthly set-angle (and faces it toward the equator) so you can *see* the
panel track the sun correctly - it turns a flat tracker that's ~57° off the winter
noon sun into one that stays within ~10° all day. When **off**, the tilt is manual
(`[` `]`), i.e. normal operation. Full derivation and a season table are in
[docs/sun_tracking_math.md](../docs/sun_tracking_math.md#stage-4--what-angle-should-the-panel-be-set-to-for-the-season).

> Single-axis reality: the ±30° *rotation* limit still pins the panel near
> sunrise/sunset, and in high summer the sun rises/sets far to the north, behind a
> south-facing tilt - so auto-tilt helps most in winter, spring, and fall.

## Build & run
```bash
cd sim3d
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # first time: downloads raylib (~1 min)
cmake --build build -j4
./build/solarmax_sim3d
```
Needs cmake + a C++17 compiler + internet on first configure. Works on
**Windows, macOS, and Linux** (raylib is fetched and built automatically).

- On Windows, use the "Developer Command Prompt" / Visual Studio generator, or
  MinGW; the same three commands apply. The build produces
  `build/solarmax_sim3d.exe`.

## Typing exact values (the input panel)
For checking a **specific** case, use the **INPUT** panel (left side): click a
field, type, and press **Enter**.

| Field | Format | Notes |
|---|---|---|
| **Date** | `YYYY-MM-DD` | keeps the current time of day |
| **Time** | `HH:MM` | **UTC**, 24-hour |
| **Lat** | e.g. `48.85` | decimal degrees, + = north (clamped ±85°) |
| **Lon** | e.g. `2.35` | decimal degrees, + = east (clamped ±180°) |

`Tab` jumps to the next field, `Esc` cancels the edit. Time is **paused while you
type** so the value you enter stays put; press `space` to resume. Bad input is
ignored (the old value is kept). The default location is the **equator (0, 0)**, a
very known reference; type any lat/lon to move the panel elsewhere.

## Controls
| Input | Action |
|---|---|
| **click a field** | edit Date / Time / Lat / Lon (see above) |
| **drag** (left mouse) | orbit the camera |
| **scroll wheel** | zoom in / out |
| `space` | play / pause time |
| `J` / `K` | slower / faster |
| `←` / `→` | scrub time ±10 min |
| `-` / `=` | wind down / up (raise past 20 → **STOW**) |
| `[` / `]` | axis tilt − / + (manual; disabled while auto-tilt is on) |
| `,` / `.` | axis azimuth − / + (manual) |
| `A` / `Z` | latitude + / − |
| `T` | **auto-tilt** on/off (drive the axis to the monthly set-angle) |
| `1` `2` `3` `4` | presets (see table above) |
| `0` (or `E`) | reset to the **equator** demo |
| `G` | cycle the bottom plot: **off → angle vs time → speed vs time** |
| `Esc` | quit |

## Panel speed over time (the `G` plot)
The bottom plot answers a specific question: *does the panel move at a constant
speed, or does its speed change?* Press `G` twice to show **panel speed (°/hr) vs
time**, with a dashed **15°/hr** reference (what a constant-speed panel would do).
Its Y axis **auto-zooms** to the curve (min/max labelled on the left) so the
shape is visible even though the values only span a few °/hr - except at the
equator, where a minimum window keeps the genuinely-flat 15°/hr line from
zooming into noise.

The panel's true tracking speed is **only** constant (a flat 15°/hr line) at the
**equator**. Everywhere else it is **nonlinear** - fastest near solar noon, slowest
near sunrise/sunset, and the swing grows with latitude:

| Site / date | speed at sunrise → **noon** → sunset |
|---|---|
| Equator (any date) | 15.0 → **15.0** → 15.0  (flat - linear) |
| Tempe 33°N, equinox | 12.5 → **18.0** → 12.5  (nonlinear) |
| 50°N, solstice | 8.6 → **15.4** → 8.6  (strongly nonlinear) |

So sit at the equator (flat line ✓), then press `A` to raise the latitude and
watch the speed curve **bow** - that is the nonlinearity made visible, and it is
computed straight from the firmware's solar math. (The **angle** plot, `G` once,
shows the same thing as an S-curve, with the ±30° limits where the motor pins.)

## What to look for
- The blue **panel normal** points straight at the yellow **sun** whenever the
  sun is within the panel's ±30° travel - that *is* "tracking is working."
- The three arcs (**magenta** June, **blue** equinox, **red** December) are the
  sun's path for those dates, drawn from the real solar math.
- Raise **wind** past the stow threshold → state goes **STOW**, panel returns
  flat; lower it to resume tracking.
- Scrub past sunset → **NIGHT**, panel parks.
- Change **latitude** (`A`/`Z`) and watch the whole set of arcs rebuild - at
  50°N they lean south and never reach the zenith, exactly like the 50°N panel
  in `sun_paths.jpg`.

## Relationship to the other simulator
`sim/` is the lightweight **terminal** simulator (FTXUI). It has the same
controls plus a `v` key that switches its side-view for a rotatable 3D wireframe
version of this same dome. Both simulators share `sim/sky_scene.h` (geometry) and
`sim/simclock.h` (portable clock), and both drive the real control brain.
