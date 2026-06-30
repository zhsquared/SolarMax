#pragma once

// ── Location (Tempe, AZ — update for actual install site) ────────────────────
#define LATITUDE          33.4255f    // Decimal degrees North
#define LONGITUDE       -111.9400f    // Decimal degrees (negative = West)
#define TIMEZONE_OFFSET      -7       // Arizona = UTC-7, no DST

// ── WiFi / NTP ────────────────────────────────────────────────────────────────
#define WIFI_SSID         "YOUR_SSID"
#define WIFI_PASS         "YOUR_PASSWORD"
#define NTP_SERVER1       "pool.ntp.org"
#define NTP_SERVER2       "time.nist.gov"
#define WIFI_TIMEOUT_MS   10000UL
#define NTP_TIMEOUT_MS     8000UL
#define DAILY_NTP_RESYNC_H    3       // Re-sync NTP at this UTC hour (3 AM UTC = 8 PM local)

// ── BTS7960 Motor Driver Pins ─────────────────────────────────────────────────
// RPWM → CW  (panel tilts west)
// LPWM → CCW (panel tilts east)
#define PIN_MOTOR_RPWM   25
#define PIN_MOTOR_LPWM   26
#define PIN_MOTOR_REN    27   // Enable right half-bridge (set HIGH)
#define PIN_MOTOR_LEN    14   // Enable left  half-bridge (set HIGH)

// ── Potentiometer (shaft angle feedback) ─────────────────────────────────────
// ADC1 pins only — safe to read while WiFi is active
#define PIN_POT          34   // ADC1_CH6, input-only GPIO
#define POT_SAMPLES       8   // Readings averaged per call

// Run runCalibration() to find your values, then update these:
#define POT_ADC_MIN     300   // 12-bit ADC count when panel is at PANEL_ANGLE_MIN (-30°)
#define POT_ADC_MAX    3800   // 12-bit ADC count when panel is at PANEL_ANGLE_MAX (+30°)

// ── Limit Switches ────────────────────────────────────────────────────────────
#define PIN_LIMIT_CW     32   // Trips at full west travel
#define PIN_LIMIT_CCW    33   // Trips at full east travel
#define LIMIT_ACTIVE   HIGH   // Change to LOW if using normally-closed switches

// ── Anemometer (Adafruit 1733 — analog 0.4–2.0 V output) ─────────────────────
// The 1733 is a powered ANALOG anemometer (see docs/wiring.md):
//   • Power 7–24 V  -> wire to the 12 V rail. It will NOT run on the 5 V rail.
//   • Signal 0.4–2.0 V -> GPIO35 ADC directly. NO divider (2.0 V < 3.3 V ADC max).
// Linear map: 0.40 V = 0 m/s, 2.00 V = 32.4 m/s; output clamps to 0 below 0.40 V.
// Read with analogReadMilliVolts() so the ESP32's per-chip ADC calibration is used.
#define PIN_ANEMOMETER     35        // ADC1_CH7, input-only GPIO
#define ANEM_V_OFFSET_MV   400.0f    // Sensor output (mV) at 0 wind     (0.40 V)
#define ANEM_V_FS_MV       2000.0f   // Sensor output (mV) at full scale (2.00 V)
#define ANEM_MS_FS         32.4f     // Wind speed (m/s) at ANEM_V_FS_MV
#define ANEM_MPH_PER_MS    2.23694f  // m/s -> mph conversion
#define ANEM_ADC_SAMPLES   16        // ADC reads averaged per measurement
#define ANEM_SAMPLE_MS     3000UL    // Minimum interval between recomputes (ms)
// TODO (verify on the bench, then optionally enable): a healthy 1733 idles near
//   0.40 V even in dead calm, so a reading pinned near 0 mV means it lost 12 V power
//   or the signal wire is open. Once you confirm the real idle voltage, consider
//   failing SAFE — treat mv < ~150 mV as a fault and report high wind so the panel
//   stows. Left out for now so a genuinely calm day isn't misread as a fault.

// ── LDR Sensors (diagnostic only — not used for primary tracking) ─────────────
#define PIN_LDR_EAST     36   // ADC1_CH0, input-only GPIO
#define PIN_LDR_WEST     39   // ADC1_CH3, input-only GPIO
#define LDR_FLAG_THRESH  300  // ADC units mismatch that triggers a diagnostic warning

// ── DS3231 RTC (I2C) ─────────────────────────────────────────────────────────
#define RTC_SDA          21
#define RTC_SCL          22

// ── LEDC PWM channels for BTS7960 ────────────────────────────────────────────
#define PWM_CHANNEL_R     0
#define PWM_CHANNEL_L     1
#define PWM_FREQ_HZ    1000
#define PWM_RESOLUTION    8   // 8-bit: duty cycle 0–255

// ── Motor Speed Limits ────────────────────────────────────────────────────────
#define MOTOR_PWM_MIN    55   // Minimum duty to overcome stiction (tune to your motor)
#define MOTOR_PWM_MAX   200   // Maximum duty — caps speed to protect worm gear

// ── Panel Geometry ────────────────────────────────────────────────────────────
#define PANEL_ANGLE_MIN  -30.0f   // Degrees — east mechanical limit
#define PANEL_ANGLE_MAX   30.0f   // Degrees — west mechanical limit
#define PANEL_STOW_ANGLE   0.0f   // Flat/horizontal stow position for high wind

// ── Rotation-Axis Orientation (set per roof) ─────────────────────────────────
// The tracker's rotation axis as physically installed. Flat & level N-S = 0/0
// (original behavior). On a sloped roof, set how far the axis tilts above
// horizontal and the compass direction its raised end faces.
#define AXIS_TILT_DEG      0.0f   // Degrees above horizontal (0 = flat roof)
#define AXIS_AZIMUTH_DEG   0.0f   // 0=North, 90=East, 180=South, 270=West
#define MOTOR_DEADBAND_DEG 1.0f   // Stop motor when within ±1° of target
#define DRIVE_TIMEOUT_MS  30000UL // Abort move if it takes longer than this (fault)

// ── Operating Thresholds ──────────────────────────────────────────────────────
#define WIND_STOW_MPH     20.0f   // Enter stow mode above this speed
#define WIND_RESUME_MPH   15.0f   // Resume tracking when wind drops below this
#define SUN_MIN_ELEV_DEG   5.0f   // Below this elevation = night mode
#define TRACKING_INTERVAL_MS  300000UL   // Recompute sun position every 5 minutes
