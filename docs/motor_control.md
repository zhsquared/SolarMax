# Motor Position Control — turning a target angle into movement

Stage 2 of [the tracking math](sun_tracking_math.md) gives a **target panel angle**
in degrees. This doc covers what happens next: **how that number becomes actual
motor movement**, using the potentiometer as position feedback.

**Where the code is:** [`src/motor_control.cpp`](../src/motor_control.cpp) (real
firmware — excluded from simulation builds by `#ifndef SIMULATE`). It is called
once per control cycle from [`src/main.cpp`](../src/main.cpp): `driveToAngle(targetAngle)`.
All the tuning constants are in [`include/config.h`](../include/config.h).

This is a **closed-loop position controller**: it drives the motor until the panel
angle *measured by the potentiometer* matches the commanded angle.

```
target angle ──► clamp to ±30° ──►┌───────────── loop every 50 ms ─────────────┐
                                   │ read pot → angle → error = target−current  │
                                   │ |error| ≤ 1°?  ── yes ──► STOP (done)       │
                                   │      │ no                                   │
                                   │ pick direction (CW/CCW), check limit switch │
                                   │ speed = f(|error|) ──► PWM to H-bridge       │
                                   └──────────────────────────────────────────── ┘
```

---

## 1. Position feedback: potentiometer → angle
A 10 kΩ linear potentiometer is mechanically coupled to the panel's rotation
shaft, so its wiper voltage is proportional to the panel angle. The ESP32 reads
that voltage on `PIN_POT` with its **12-bit ADC** (`analogRead` → an integer
**0–4095**), averaged over `POT_SAMPLES = 8` reads to cut noise (`readPotRaw()`).

The count is mapped to degrees by a straight line (`adcToAngle()`), fixed by two
calibrated endpoints:

```
angle = (adc − POT_ADC_MIN) / (POT_ADC_MAX − POT_ADC_MIN) × (ANGLE_MAX − ANGLE_MIN) + ANGLE_MIN
```

With the values in `config.h` — `POT_ADC_MIN = 300` (at −30°), `POT_ADC_MAX = 3800`
(at +30°), `ANGLE_MIN/MAX = ∓30°`:

```
angle = (adc − 300) / 3500 × 60 − 30      [degrees]
```

- **Resolution:** 60° / 3500 counts ≈ **0.017° per ADC count** (≈ 58 counts per
  degree) — far finer than the ±1° deadband, so the ADC is not the limiting factor.
- **Examples:** adc 300 → −30°; adc 2050 → 0°; adc 3800 → +30°.

### Calibration (how POT_ADC_MIN / MAX are found)
These two numbers depend on how the pot is mounted, so they're measured, not
guessed. `runCalibration()` (same file) drives the panel by hand to each
mechanical limit, reads the raw ADC at each end, and prints the two `#define`
lines to paste into `config.h`. (The `bringup` build also prints live pot counts.)

---

## 2. Error and the deadband
Each loop:

```
current = readPanelAngle()        // pot → degrees, from §1
error   = target − current        // degrees, signed
```

If `|error| ≤ MOTOR_DEADBAND_DEG` (**1°**) the motor **stops and the move is done**.
The deadband is essential: without it the controller would forever twitch back and
forth across the exact target (the panel and gear can't hold an infinitely precise
position). 1° ≈ 58 ADC counts, comfortably above the pot's noise floor.

---

## 3. Direction, and the mechanical limits
The **sign of the error** picks the direction:

| error | meaning | drive |
|-------|---------|-------|
| `> 0` | panel must increase angle → tilt **west** | `driveCW()` |
| `< 0` | panel must decrease angle → tilt **east** | `driveCCW()` |

Before driving, the matching **limit switch** is checked
(`PIN_LIMIT_CW` / `PIN_LIMIT_CCW`, active = `LIMIT_ACTIVE`). If the panel is already
against that hard stop, the move aborts with a fault instead of straining the motor.
(These are a hardware backstop; the ±30° software clamp in `driveToAngle()` should
stop it first.)

---

## 4. Speed: error → PWM duty (proportional)
Speed is **proportional to how far the panel still has to move**, so it moves
briskly when far and eases in near the target. `calcSpeed()`:

```
duty = clamp( |error| × 6 + MOTOR_PWM_MIN,  MOTOR_PWM_MIN,  MOTOR_PWM_MAX )
     = clamp( |error| × 6 + 55,             55,             200 )        [0–255]
```

- **`MOTOR_PWM_MIN = 55`** — the floor duty needed to overcome stiction (below it
  the motor buzzes but doesn't turn).
- **gain `6`** — duty counts per degree of error.
- **`MOTOR_PWM_MAX = 200`** — caps speed to protect the worm gear.

Worked values: 1° error → 61; 10° → 115; **≥ 24.2° → capped at 200**. So the
response is proportional within ~24° and saturated beyond that; near the target it
bottoms out at 55 for a gentle final approach.

This is a **proportional-speed, bang-bang-direction** controller (P-control on
speed magnitude, on/off on direction, with a deadband) — deliberately simpler than
a full PID because the load is slow and self-holding (a worm gear won't back-drive).

---

## 5. PWM → the motor (BTS7960 H-bridge)
The duty is written to the BTS7960 driver over two PWM channels using the ESP32
LEDC peripheral (`motorInit()` / `driveCW/CCW()`):

- **`PWM_FREQ_HZ = 1000`**, **`PWM_RESOLUTION = 8`** → duty cycle is an 8-bit value
  **0–255** at 1 kHz.
- `driveCW(speed)` → `RPWM = speed`, `LPWM = 0`; `driveCCW(speed)` → the reverse.
  One side PWM-ed, the other at 0, sets rotation direction.
- `R_EN` / `L_EN` are held HIGH to enable both half-bridges.
- **Stop** = both channels 0 (coast). **Brake** = both to 255 briefly, then stop.

---

## 6. The loop, and safety
`driveToAngle()` repeats §2–§5 every **50 ms** until the deadband is met. Two ways
it aborts and returns a fault instead of spinning forever:

- **Limit switch** tripped in the drive direction (§3).
- **Timeout:** if the move isn't done within `DRIVE_TIMEOUT_MS` (**30 s**) — the
  telltale sign the potentiometer isn't reading (broken wiper/wire), so the loop
  would never see the panel reach target. It stops and logs `check pot wiring`.

Because tracking only nudges the panel a fraction of a degree at a time (the
control cycle runs often), most calls finish in one or two iterations.

---

## In simulation
The interactive simulators don't model any of this — they use a **perfect motor**.
In [`src/hal_sim.cpp`](../src/hal_sim.cpp), `driveToAngle()` just sets the stored
angle to the target and `readPanelAngle()` returns it. The FTXUI and 3D sims then
*ease* the displayed angle toward the target purely for smooth animation. So this
file (`motor_control.cpp`) is the **only** place the real angle-to-movement
conversion happens; the sims stub it out on purpose.

## Quick reference — the two conversion formulas
```
current angle  =  (adc − 300) / 3500 × 60 − 30                     // pot → degrees
motor duty     =  clamp(|target − current| × 6 + 55, 55, 200)      // error → PWM (0–255)
```
