# Motor Position Control - turning a target angle into movement

Stage 2 of [the tracking math](sun_tracking_math.md) gives a **target panel angle**
in degrees. This doc covers what happens next: **how that number becomes actual
motor movement** - without a position sensor.

**Where the code is:** [`src/motor_control.cpp`](../src/motor_control.cpp) (real
firmware - excluded from simulation builds by `#ifndef SIMULATE`). It is called
once per control cycle from [`src/main.cpp`](../src/main.cpp): `driveToAngle(targetAngle)`.
Tuning constants are in [`include/config.h`](../include/config.h).

> **No potentiometer.** Earlier versions read a pot for position feedback. The pot
> isn't coupled to the panel on this build, so position is now tracked **open-loop**:
> the motor runs at a fixed speed and we count time, referenced by the two limit
> switches. The pot stays wired but is unused for control.

```
BOOT ─► home to EAST limit ─► sweep to WEST limit (timed) ─► speed = 60° / sweep_time
                                                                    │
RUN  ─► target ─► clamp ±30° ─► run motor at fixed PWM for  time = |Δangle| / speed
                                     │ hit a limit switch? → snap estimate to that end
                                     └ else → estimate += (run time × speed)
```

---

## 1. How position is known: dead reckoning + limit homing
There is no encoder and no usable position sensor, so the firmware **estimates** the
angle by dead reckoning: the motor runs at a **fixed PWM** (`MOTOR_PWM_MOVE`), which
gives a repeatable travel speed, so

```
angle moved = run time × speed
```

Two things keep that estimate honest, both from the **limit switches** at the
mechanical ends (`PANEL_ANGLE_MIN` = −30° east, `PANEL_ANGLE_MAX` = +30° west):

- **Homing establishes a known start.** On boot it drives to the east limit - now it
  *knows* the panel is at −30°.
- **The limits re-zero drift.** Whenever a limit switch trips during operation, the
  estimate snaps to that end's exact angle. The tracker naturally pins **east every
  morning and west every evening**, so accumulated error is erased **twice a day for
  free** - open-loop drift can never build up beyond a single day.

---

## 2. Auto-calibration at boot (`motorHomeAndCalibrate`)
The travel speed is measured automatically at every power-up - the operator does
nothing. Called from `setup()` right after `motorInit()`:

1. Drive to the **east** limit (`runToLimit`) → set `estimatedAngle = −30°`.
2. Start a timer, drive to the **west** limit, stop the timer → `sweepMs`.
3. `speed = (PANEL_ANGLE_MAX − PANEL_ANGLE_MIN) / sweepMs` (degrees per second), and
   `estimatedAngle = +30°`.

It prints, e.g.:
```
[MOTOR] Homing to east limit...
[MOTOR] Sweeping to west limit to measure travel speed...
[MOTOR] Calibrated: 60 deg in 18.4 s = 3.26 deg/s
```

The panel never powers off in normal operation, so this one-time boot sweep is only
ever seen while (re)programming - never in the field.

> **The limit switches are now load-bearing.** If one is miswired and never trips,
> homing times out (`HOMING_TIMEOUT_MS`), prints
> `never reached … limit - check limit-switch wiring`, and the tracker stays
> un-calibrated (it won't drive until reset). Verify them first.

---

## 3. Moving to a target (`driveToAngle`)
Each control cycle:

```
target  = clamp(target, -30, +30)
delta   = target - estimatedAngle
if |delta| <= MOTOR_DEADBAND_DEG (1°)  → done, don't move   // deadband stops hunting
direction = (delta > 0) ? WEST (CW) : EAST (CCW)
runMs   = |delta| / speed × 1000       // time to cover the gap
drive at MOTOR_PWM_MOVE for runMs, but:
    • if the end limit trips → stop, snap estimate to that limit, done
    • otherwise → estimate += (actual run time × speed)
```

Because tracking only nudges the panel about a degree at a time (the deadband
throttles it), each move is a short pulse of a second or two. A large move is capped
at `DRIVE_TIMEOUT_MS` per call and simply finishes on the next cycle (the estimate
advanced, so the remaining `delta` is smaller).

The **direction mapping:** `driveCW` moves toward the **west/+** end (`PIN_LIMIT_CW`);
`driveCCW` toward the **east/−** end (`PIN_LIMIT_CCW`). `R_EN`/`L_EN` are held HIGH to
enable the BTS7960; **stop** = both PWM channels 0 (a worm gear won't back-drive, so
the panel holds position with the motor off).

---

## 4. Accuracy and trade-offs
Open-loop timing is not as precise as a real sensor - that's the deal we accept for
having no position feedback:

- Speed varies a little with **battery voltage, load (a steep panel needs more
  torque), friction, and temperature**, so the estimate drifts by a few degrees
  across a day. For a solar panel that's a tiny energy loss (cosine of a few degrees
  is well under 1%).
- **Re-homing at the limits** bounds that drift and resets it twice daily.
- A **fixed PWM** (not proportional speed) is used so `time × speed` is linear.
- There is **no hard stall detection** (that's what the pot used to give). A jam is
  eventually caught when the panel is next commanded to an end and the estimate is
  re-referenced by the limit switch.

---

## In simulation
The simulators use a **perfect motor**. In [`src/hal_sim.cpp`](../src/hal_sim.cpp),
`motorHomeAndCalibrate()` is a no-op, `driveToAngle()` sets the angle to the target
instantly, and `readPanelAngle()` returns it. So `motor_control.cpp` is the **only**
place the real angle-to-movement conversion happens; the sims stub it out on purpose.

## Quick reference
```
speed (deg/s)  =  (PANEL_ANGLE_MAX - PANEL_ANGLE_MIN) / boot_sweep_seconds
move time      =  |target - estimatedAngle| / speed          // run motor this long
estimate       =  snapped to ±30° whenever a limit switch trips
```
