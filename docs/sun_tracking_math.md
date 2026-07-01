# Sun-Tracking Math

**Where the formulas live:** [`lib/solar_math/solar_position.cpp`](../lib/solar_math/solar_position.cpp)
- `calculateSolarPositionRaw(...)` — the NOAA sun-position algorithm
- `panelAngleForAxis(...)` — converts the sun direction into a panel rotation angle (handles tilted roofs)

The math has two stages: **(1)** where is the sun? **(2)** how far should the panel rotate to face it?

---

## Stage 1 — Where is the sun? (NOAA Solar Calculator)
Reference: <https://gml.noaa.gov/grad/solcalc/calcdetails.html> (accuracy ~0.01°).
Input: UTC date/time + latitude/longitude. Output: solar **elevation** and **azimuth**.

The code follows these steps (function `calculateSolarPositionRaw`):

1. **Julian Day / Century** — convert the calendar date+time to an astronomical day count `JD`, then `JC = (JD − 2451545) / 36525`.
2. **Geometric mean longitude `L₀` & anomaly `M`** of the sun (polynomials in `JC`).
3. **Equation of center `C`** → **true longitude** → **apparent longitude `λ`** (corrects for Earth's orbit eccentricity and nutation).
4. **Obliquity of the ecliptic `ε`** — the tilt of Earth's axis.
5. **Declination:** `δ = asin( sin ε · sin λ )` — how far north/south the sun is.
6. **Equation of time `EoT`** — the difference between clock time and true solar time.
7. **True solar time** → **hour angle `HA`** (negative = morning, positive = afternoon):
   `trueSolarTime = UTC_minutes + EoT + 4·longitude`, `HA = trueSolarTime/4 − 180°`.
8. **Elevation** (height above horizon):
   `cos(zenith) = sin(lat)·sin(δ) + cos(lat)·cos(δ)·cos(HA)`, `elevation = 90° − zenith`.
9. **Azimuth** (compass bearing, 0°=N, 90°=E, 180°=S, 270°=W) from `δ`, `lat`, and zenith.

Result: the sun's **elevation** and **azimuth** for that instant and place.

---

## Stage 2 — How far should the panel rotate? (`panelAngleForAxis`)
A single-axis tracker rotates the panel about one fixed axis. The optimal rotation
points the panel's normal as close to the sun as possible.

The code uses a **vector-projection** method so it works for *any* axis orientation
(flat roof or sloped). Steps:

1. Build the **sun direction** as a 3-D unit vector in East-North-Up coordinates:
   `s = ( cos(el)·sin(az), cos(el)·cos(az), sin(el) )`
2. Build the **rotation-axis** unit vector from its tilt `β` and azimuth `γ`:
   `a = ( cos(β)·sin(γ), cos(β)·cos(γ), sin(β) )`
3. **Project** the sun onto the plane perpendicular to the axis (remove the part
   along the axis): `s_proj = s − (s·a) a`
4. The panel rotation angle is the angle of `s_proj` measured from the "face-up"
   reference, taken with `atan2(...)`. Positive = panel tilts **west**.

This **reduces exactly to the classic horizontal North-South formula when the roof
is flat** (β = 0, γ = 0):
`panelAngle = atan2( sin(azimuth − 180°), tan(elevation) )`

Finally the angle is **clamped** to the mechanical range `[PANEL_ANGLE_MIN, PANEL_ANGLE_MAX]`
(±30°), so near sunrise/sunset the panel pins at its limit.

### Roof angle
The axis tilt/azimuth come from `AXIS_TILT_DEG` and `AXIS_AZIMUTH_DEG` in
[`include/config.h`](../include/config.h). Set them to match how the tracker is
mounted on a given roof. `0 / 0` = flat, level, North-South (the original behavior).

---

## Sanity values (Tempe, AZ — latitude 33.4°)
| Moment | Elevation | Panel (flat roof) |
|--------|-----------|-------------------|
| Summer solstice noon | ~80° | ~0° (sun nearly overhead, due south) |
| Equinox noon | ~56.6° (= 90 − latitude) | ~0° |
| Winter solstice noon | ~33° | ~0° |
| Sunrise | low, east | −30° (clamped to east limit) |

You can watch all of these live in the [interactive simulator](simulation.md)
(keys `1`–`4` jump to these moments).
