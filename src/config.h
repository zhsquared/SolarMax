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

// ── Anemometer (3-cup, reed-switch output) ───────────────────────────────────
#define PIN_ANEMOMETER   35   // Interrupt-capable digital input
#define ANEM_MPH_PER_HZ  1.49f  // mph per pulse/sec — adjust to your model
#define ANEM_SAMPLE_MS   3000UL  // Averaging window in ms

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
#define MOTOR_DEADBAND_DEG 1.0f   // Stop motor when within ±1° of target
#define DRIVE_TIMEOUT_MS  30000UL // Abort move if it takes longer than this (fault)

// ── Operating Thresholds ──────────────────────────────────────────────────────
#define WIND_STOW_MPH     20.0f   // Enter stow mode above this speed
#define WIND_RESUME_MPH   15.0f   // Resume tracking when wind drops below this
#define SUN_MIN_ELEV_DEG   5.0f   // Below this elevation = night mode
#define TRACKING_INTERVAL_MS  300000UL   // Recompute sun position every 5 minutes
