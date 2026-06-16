# SolarMax

ESP32 firmware for a single-axis solar tracking system. Tracks the sun using astronomical calculations (NOAA Solar Calculator) rather than light sensors — no calibration needed after install.

## Hardware

- ESP32 DevKit V1
- BTS7960 43A H-bridge motor driver
- DC worm gear motor with potentiometer position feedback
- DS3231 RTC (I2C) + WiFi NTP for timekeeping
- 3-cup anemometer for wind stow protection

## Builds

| Environment | Command | Use |
|---|---|---|
| `esp32dev` | `pio run -e esp32dev` | Real hardware |
| `simulate` | `pio run -e simulate` | ESP32 without sensors — full state machine on Serial at 120× speed |
| `native` | `pio test -e native` | Unit tests on Mac, no ESP32 needed |

## Configuration

Edit [src/config.h](src/config.h) before flashing:

- `LATITUDE` / `LONGITUDE` — install site coordinates
- `WIFI_SSID` / `WIFI_PASS` — for NTP sync
- `POT_ADC_MIN` / `POT_ADC_MAX` — run `runCalibration()` on first assembly to find these

## State Machine

`INIT` → `TRACKING` → `STOW` (wind ≥ 20 mph) → `TRACKING` / `NIGHT` (sun < 5° elevation) → `TRACKING`

## Unit Tests

```bash
pio test -e native
```

Tests cover solar noon elevation for all three solstice/equinox cases, morning/afternoon panel sign convention, mechanical limit clamping, and night detection.
