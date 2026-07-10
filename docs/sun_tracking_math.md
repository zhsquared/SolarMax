# Sun-Tracking Math

**Where the formulas live:** [`lib/solar_math/solar_position.cpp`](../lib/solar_math/solar_position.cpp)
- `calculateSolarPositionRaw(...)` - the NOAA sun-position algorithm
- `panelAngleForAxis(...)` - converts the sun direction into a panel rotation angle (handles tilted roofs)

The math has two stages: **(1)** where is the sun? **(2)** how far should the panel rotate to face it?

---

## Stage 1 - Where is the sun? (NOAA Solar Calculator)
Reference: <https://gml.noaa.gov/grad/solcalc/calcdetails.html> (accuracy ~0.01°).
Input: UTC date/time + latitude/longitude. Output: solar **elevation** and **azimuth**.

The code follows these steps (function `calculateSolarPositionRaw`):

1. **Julian Day / Century** - convert the calendar date+time to an astronomical day count `JD`, then `JC = (JD − 2451545) / 36525`.
2. **Geometric mean longitude `L₀` & anomaly `M`** of the sun (polynomials in `JC`).
3. **Equation of center `C`** → **true longitude** → **apparent longitude `λ`** (corrects for Earth's orbit eccentricity and nutation).
4. **Obliquity of the ecliptic `ε`** - the tilt of Earth's axis.
5. **Declination:** `δ = asin( sin ε · sin λ )` - how far north/south the sun is.
6. **Equation of time `EoT`** - the difference between clock time and true solar time.
7. **True solar time** → **hour angle `HA`** (negative = morning, positive = afternoon):
   `trueSolarTime = UTC_minutes + EoT + 4·longitude`, `HA = trueSolarTime/4 − 180°`.
8. **Elevation** (height above horizon):
   `cos(zenith) = sin(lat)·sin(δ) + cos(lat)·cos(δ)·cos(HA)`, `elevation = 90° − zenith`.
9. **Azimuth** (compass bearing, 0°=N, 90°=E, 180°=S, 270°=W) from `δ`, `lat`, and zenith.

Result: the sun's **elevation** and **azimuth** for that instant and place.

---

## Stage 2 - How far should the panel rotate? (`panelAngleForAxis`)
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

> **What happens to this angle next?** The motor drives the panel to it using the
> potentiometer as feedback - see [motor_control.md](motor_control.md) for that
> conversion (pot count → angle, error → PWM duty, deadband, limits).

### Roof angle
The axis tilt/azimuth come from `AXIS_TILT_DEG` and `AXIS_AZIMUTH_DEG` in
[`include/config.h`](../include/config.h). Set them to match how the tracker is
mounted on a given roof. `0 / 0` = flat, level, North-South (the original behavior).

---

## Sanity values (Tempe, AZ - latitude 33.4°)
| Moment | Elevation | Panel (flat roof) |
|--------|-----------|-------------------|
| Summer solstice noon | ~80° | ~0° (sun nearly overhead, due south) |
| Equinox noon | ~56.6° (= 90 − latitude) | ~0° |
| Winter solstice noon | ~33° | ~0° |
| Sunrise | low, east | −30° (clamped to east limit) |

You can watch all of these live in the [interactive simulator](simulation.md)
(keys `1`–`4` jump to these moments).

---

## Stage 3 - How *fast* does the panel move? (tracking rate)
The sun's **hour angle** advances at a constant **15°/hr** (Earth's rotation). It is
tempting to assume the panel therefore also rotates at a constant 15°/hr - but it
**does not**, except in one special case. The panel angle is a *nonlinear* function
of the hour angle:

`R = atan2( −cos δ · sin ω ,  sin φ · sin δ + cos φ · cos δ · cos ω )`

where `φ` = latitude, `δ` = declination (season), `ω` = hour angle (time). Its rate
`dR/dt = (dR/dω)·15°/hr` is only constant when `dR/dω` is constant.

**The special case - the equator.** At `φ = 0` this collapses to `R = −ω`, so the
panel rotates at exactly **15°/hr all day, every day of the year** - perfectly
linear. (That is *why* the equator is the clean verification case, and also why the
panel "looks like it moves at constant speed" when the sim is parked at the equator.)

**Everywhere else it is nonlinear** - fastest near solar noon, slowest near
sunrise/sunset, and the swing grows with latitude. Rates computed from this code:

| Site / date | sunrise → **noon** → sunset (°/hr) | shape |
|---|---|---|
| Equator, any date | 15.0 → **15.0** → 15.0 | flat (linear) |
| Tempe 33°N, equinox | 12.5 → **18.0** → 12.5 | bowed up at noon |
| Tempe 33°N, solstice | 11.8 → **14.0** → 11.8 | mild bow |
| 50°N, solstice | 8.6 → **15.4** → 8.6 | strong bow (~2×) |

You can **see and check this** live: the [interactive simulator](simulation.md) and
the [3D visualizer](../sim3d/README.md) plot **panel speed vs time** (key `g` / `G`)
with a 15°/hr reference line - flat at the equator, visibly bowed as you raise the
latitude. Both plots are generated by calling `panelAngleForAxis(...)` across a day,
so they show exactly what the firmware computes. (Note: the ±30° mechanical clamp
holds the *actual motor* still at its limit near sunrise/sunset; the rate above is
the ideal, unclamped tracking demand.)

---

## Stage 4 - What angle should the panel be set to for the season?
Separate from the *moment-to-moment* tracking angle, there is a single **fixed tilt**
that best points a panel at the sun at **solar noon** for a given date. This is the
number both simulators show as **“Set panel tilt … deg, face S/N”**, and it changes
with the time of year.

**How we find it.** At solar noon the sun is due south (northern hemisphere) at its
highest elevation of the day, `θ_noon`. A panel whose surface is tilted from
horizontal by `β` has its normal `β` above the horizon; to point that normal at the
noon sun we need:

`β = 90° − θ_noon`

and the panel faces the direction the noon sun is in (toward the equator - south in
the northern hemisphere, north in the southern). Because the noon elevation is

`θ_noon = 90° − |φ − δ|`   (φ = latitude, δ = declination),

this simplifies to the classic result:

> **Set-angle:  β = |φ − δ|**, facing the equator.

Declination `δ` swings from **+23.44°** (June solstice) to **−23.44°** (December
solstice) and is **0°** at the equinoxes, so `β` is **shallow in summer** and
**steep in winter**. The code doesn’t hard-code this formula - it finds `θ_noon`
by scanning the day with the real solar math (`solarNoon(...)` in
[`sim/sky_scene.h`](../sim/sky_scene.h): the highest elevation reached that day) and
returns `β = 90° − θ_noon`. That way the displayed set-angle is guaranteed to agree
with the sun-position code.

### Why this is *not* the hard calculation
The genuinely hard part is **where the sun is** - declination `δ`, the equation of
time, the hour angle. That is all done once, in **Stage 1**
(`calculateSolarPositionRaw`, the NOAA algorithm, accurate to ~0.01° and covered by
unit tests). *Given* the sun's position, the set-angle is a one-liner on top of it:
sample the sun's elevation across the target day, take the **maximum** (that instant
is solar noon), and subtract from 90°.

```cpp
// sim/sky_scene.h - solarNoon(): the whole calculation
float best = -90.0f;
for (int m = 0; m <= 24*60; m += 2) {                 // every 2 minutes of the day
    SolarAngles s = calculateSolarPositionRaw(year, month, day, m/60.0, lat, lon);
    if (s.elevation > best) { best = s.elevation; n.az = s.azimuth; }
}
n.tilt = 90.0f - best;                                // β = 90° − noon elevation
```

So we **never re-derive declination** or plug the analytic `β = |φ − δ|` in code - we
let the already-validated solar model report the noon elevation and take `90 − that`.
Benefits: (1) no second, error-prone astronomy calculation to get wrong; (2) the
displayed tilt is *guaranteed* consistent with the sun the simulator draws; (3) it
transfers unchanged to any site/date/axis because it only depends on the elevation
the model already produces. The analytic `β = |φ − δ|` is how you **check** it by
hand - not how the code arrives at it.

In the 3D sim the *monthly* value is exactly this, aimed a month ahead:
`monthNoon = solarNoon(nextMonthYear, nextMonth, 15, lat, lon)`
([`sim3d/main_3d.cpp`](../sim3d/main_3d.cpp)) - the 15th-of-next-month set-angle used
by **AUTO-TILT** (see below).

### Set-angle by season (verified from the code)
| Site | Summer solstice | Equinox | Winter solstice |
|------|-----------------|---------|-----------------|
| Tempe, AZ (33.4°N) | **10°** (= 33.4 − 23.4) | **33.4°** (= latitude) | **56.8°** (= 33.4 + 23.4) |
| Equator (0°) | 23.4° (facing **north**) | 0° (flat) | 23.4° (facing south) |
| 50°N | 26.6° | 50° | 73.4° |

Rule of thumb this matches: *summer tilt ≈ latitude − 23.4°, winter ≈ latitude + 23.4°,
equinox ≈ latitude.*

### How this relates to our single-axis tracker
Our tracker rotates about a horizontal **North–South** axis, so it tilts the panel
**east↔west** to follow the sun across the sky during the day (Stage 2). The
set-angle `β` above is a **north–south** tilt - a different axis. So on a purely
horizontal tracker the seasonal set-angle is what you would use if you also gave the
mount a fixed north–south tilt (via `AXIS_TILT_DEG` in
[`include/config.h`](../include/config.h)) or adjusted it by season. Setting the
mounting tilt to the **equinox value (≈ latitude)** is the common “good all-year”
choice; adjusting toward the summer/winter values a few times a year captures a bit
more energy. This is also the reason a single-axis tracker is ~23° off the sun at
solstice noon (see the note in Stage 2): that offset is exactly the seasonal `β` its
east–west axis cannot provide.

### Monthly setting and the AUTO-TILT demo
Because the operator only adjusts the tilt a few times a year, the useful number is
the **best fixed tilt for the month ahead**, not the instantaneous one. The
simulator shows both:

- **ideal now:** the live tilt to point *straight at the sun this instant*
  (`90° − elevation`, facing the sun) - a continuously-changing 2-axis reference.
- **set `<month>`:** `β` evaluated at **noon on the 15th of the next month**, which
  represents that month’s average sun (e.g. in December you set the mid-January
  angle so all of January is well-served). This is the value an operator would dial in.

The 3D simulator’s **AUTO-TILT** button drives the axis tilt to that monthly value
purely to *demonstrate* the tracking is correct - it is not how the product runs
itself (the tilt is a manual, ~monthly adjustment). With the tilt set correctly the
east–west motor then keeps the panel on the sun through the day: at Tempe’s winter
solstice this turns a **57°** worst-case miss (flat axis) into about **10°**, and at
the equinox a **33°** miss into **10°** - verified by sampling
`panelAngleForAxis(...)` across the day against the true sun vector. (Residual comes
from the ±30° rotation limit near sunrise/sunset and, in summer, the sun passing to
the north of a south-facing tilt.)
