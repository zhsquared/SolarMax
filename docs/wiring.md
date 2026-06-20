# SolarMax — Wiring Reference

All pin assignments are defined in [include/config.h](../include/config.h).

📊 **Visual diagrams (colored):**
- 🔢 **[breadboard_placement.md](breadboard_placement.md)** — the column-by-column placement
  map. Single source of truth for the breadboard; use it to build in Fritzing or to check the
  diagram.
- 🔌 **[wiring_breadboard.pdf](wiring_breadboard.pdf)** — **build from this.** Full breadboard
  layout, every wire drawn, parts sit under their ESP32 pin, all grounds land on the blue (−)
  rails, and the motor/12 V supply are clearly marked off-board. Source: [wiring_breadboard.tex](wiring_breadboard.tex).
- 📐 **[wiring_diagram.pdf](wiring_diagram.pdf)** — schematic overview (power distribution,
  signal map, voltage-divider details). Good for understanding *what connects to what*.
  Source: [wiring_diagram.tex](wiring_diagram.tex).

Rebuild either with `pdflatex <file>.tex`. Use the diagrams with the tables below (tables = the
authoritative per-pin detail).

## Datasheet verification status

Checked against the PDFs in [../datasheets/](../datasheets/) on 2026-06-19:

| Part | Datasheet | Result |
|------|-----------|--------|
| ESP32 pin map (input-only 34–39, ADC1, no strapping pins used) | `esp32_datasheet_en.pdf` | ✅ confirmed |
| BTS7960 pinout + **3.3 V logic** + 6–27 V motor supply | `BTS7960 Motor Driver.pdf` | ✅ confirmed (resolved the level-shifter question) |
| DS3231 I2C address 0x68 | `DS3231.pdf` | ✅ confirmed |
| Potentiometer (P160 rotary, 3-terminal) | `p160.pdf` | ✅ confirmed |
| **Anemometer = QS-FS powered sensor, not a reed switch** | `C2192-002_datasheet.pdf` | ⚠️ **wiring corrected — see anemometer section** |
| LDR / photocell | `SEN-09088.pdf` | ⚠️ image-only PDF, not machine-readable; wiring is standard 2-terminal photocell |
| Solar panel (Renogy 50 W) | `50D-SS-Datasheet.pdf` | ℹ️ this is the tracked panel, not control wiring |

**Motor:** 5882-50ZY identified from the model number (stall ~15–19 A, max 200 kg·cm). **Confirm
the gear-ratio/RPM variant ordered** — only the high-ratio (low-RPM) versions meet the 20 N·m
wind-holding requirement; the fast 440 RPM version is ~10× too weak.

**Limit switches:** REIFENG mechanical endstop selected (SPDT C/NO/NC, 1 A 125 VAC). Generic
3D-printer part with no formal datasheet — specs read directly from the switch body.

---

## Components List

| # | Component | Qty | Datasheet | Notes |
|---|-----------|-----|-----------|-------|
| 1 | ESP32 DevKit V1 (WROOM-32) | 1 | `esp32_datasheet_en.pdf`, `esp-dev-kits-en-master-esp32.pdf` | 3.3 V logic |
| 2 | BTS7960 43A H-Bridge module | 1 | `BTS7960 Motor Driver.pdf` | Motor driver, 3.3–5 V logic, 6–27 V motor supply |
| 3 | DC worm gear motor, 12 V (5882-50ZY) | 1 | vendor spec (model identified) | Brushed, self-locking; stall ~15–19 A; **confirm gear-ratio variant — needs ~200 kg·cm / 20 N·m for wind** |
| 4 | P160 16 mm rotary potentiometer | 1 | `p160.pdf` | Use **linear (B) taper**; 10 kΩ; 260° travel |
| 5 | DS3231 RTC module | 1 | `DS3231.pdf` | I2C 0x68, CR2032 backup |
| 6 | REIFENG mechanical endstop (SPDT microswitch on PCB, C/NO/NC, 1 A 125 VAC, 1 M cable) | 2 | generic 3D-printer part (no formal datasheet) | East & west end-stops; use C + NO contacts |
| 7 | QS-FS wind speed sensor (C2192-002) | 1 | `C2192-002_datasheet.pdf` | **Powered 5–24 V sensor, NOT a reed switch — see anemometer section** |
| 8 | LDR / photocell (SparkFun SEN-09088) | 2 | `SEN-09088.pdf` | Diagnostic only; 2-terminal, no polarity |
| 9 | 10 kΩ resistor | 2 | — | LDR voltage divider pull-downs |
| 10 | 10 kΩ + 20 kΩ resistor | 1 ea | — | Anemometer 5 V→3.3 V signal divider (see note) |
| 11 | 12 V 20 A power supply (e.g. ALITOVE 240 W) | 1 | — | Motor rail; 20 A covers the ~19 A stall surge |
| 12 | LM2596 buck converter (12 V→5 V, adj., 3 A) | 1 | — | Makes the 5 V rail; **set to 5.0 V before connecting ESP32** |
| 13 | 12 AWG wire | — | — | Motor power path only (supply↔driver↔motor) |
| 14 | 18–22 AWG wire | — | — | All logic/signal/ESP32 wiring |
| 15 | Schottky diode (1N5822 3 A, or 1N5817) | 1 | — | In buck 5 V output → ESP 5V pin (stripe toward ESP); lets USB stay safely pluggable for programming |
| — | Renogy 50 W solar panel (RNG-50D-SS) | 1 | `50D-SS-Datasheet.pdf` | The tracked panel (Voc 21.8 V, Isc 3.1 A); not wired to the controller |

> **Common ground:** ESP32 GND, BTS7960 logic GND, anemometer ground, and the 12 V supply GND must all be tied together.

---

## Power — two rails

A single 12 V 20 A supply feeds the motor directly and an LM2596 buck converter that
produces the 5 V logic rail.

```
12 V 20 A supply ─┬─ B+/B− ─> BTS7960 ─> motor (M+/M−)     [12 AWG, ~19 A]
                  └─ buck ─[Schottky diode]─> 5 V rail ─┬─ ESP32 5V pin
                                                        ├─ BTS7960 VCC
                                                        └─ anemometer power
USB port = programming only (the diode lets you plug it in anytime)
ESP32 3.3 V ─> RTC, pot, LDRs, limit switches             [18–22 AWG]
ALL grounds tied together (12 V supply, buck, ESP32, driver, sensors)
```

| From | To | Wire | Notes |
|------|----|------|-------|
| 12 V supply (+) | BTS7960 B+ | 12 AWG | Motor power rail |
| 12 V supply (−) | BTS7960 B− | 12 AWG | Common GND |
| 12 V supply (+/−) | LM2596 IN+ / IN− | 18–22 AWG | Buck converter input |
| Buck OUT+ | **Schottky diode** → ESP32 5V pin, BTS7960 VCC, anemometer power | 18–22 AWG | 5 V logic rail; diode lets USB coexist |
| Buck OUT− | ESP32 GND (common) | 18–22 AWG | |
| ESP32 3.3 V | Potentiometer pin 1 | 22 AWG | Pot reference voltage |
| ESP32 3.3 V | DS3231 VCC | 22 AWG | RTC power (3.3 V — see RTC safety note) |
| ESP32 GND | All peripheral GNDs | 22 AWG | Common GND — must be shared |

> ⚠️ If using an adjustable buck, **set it to 5.0 V before wiring it to the ESP32**.
> ⚠️ Do **not** feed 12 V into the ESP32 VIN; power it from the regulated 5 V rail.
> 🔌 **Programming:** the ESP32 is powered through its **5V pin** from the buck — the USB port
> is for flashing code only. A **Schottky diode** in the buck's 5 V output means you can leave
> the whole system powered and plug a USB cable into the computer anytime, with any cable, with
> no conflict and no risk to the USB port (the diode blocks any backfeed toward the computer).

---

## BTS7960 Motor Driver → ESP32

| BTS7960 Pin | ESP32 GPIO | Config symbol | Description |
|-------------|------------|---------------|-------------|
| RPWM | GPIO 25 | `PIN_MOTOR_RPWM` | PWM — drives motor CW (panel tilts west) |
| LPWM | GPIO 26 | `PIN_MOTOR_LPWM` | PWM — drives motor CCW (panel tilts east) |
| R_EN | GPIO 27 | `PIN_MOTOR_REN` | Enable right half-bridge (driven HIGH) |
| L_EN | GPIO 14 | `PIN_MOTOR_LEN` | Enable left half-bridge (driven HIGH) |
| VCC | ESP32 5 V | — | Logic supply for the module |
| GND | ESP32 GND | — | Logic ground |
| R_IS | — | — | Current-sense output, not connected |
| L_IS | — | — | Current-sense output, not connected |

> **3.3 V logic confirmed by datasheet.** The BTS7960 module datasheet states **"Control Input Level: 3.3~5V"** — the ESP32's 3.3 V GPIO drives RPWM/LPWM/R_EN/L_EN directly, no level shifter needed. Motor supply (B+/B−) range is **6–27 VDC**, VCC is +5 V. (Verified against `datasheets/BTS7960 Motor Driver.pdf`.)

---

## BTS7960 → Motor & Power Terminals

Screw-terminal block labels on the IBT-2 module (verified against datasheet/vendor docs):

| Terminal | Connection | Notes |
|----------|------------|-------|
| M+ | Motor wire A | Swap A/B if the panel drives the wrong direction |
| M− | Motor wire B | |
| B+ | 12 V supply (+) | Motor power rail, 6–27 V |
| B− | 12 V supply (−) | Common ground |

> **Power sizing:** the 5882-50ZY draws **~15–19 A at stall** (startup inrush or a jam),
> though normal slow tracking moves draw far less. The BTS7960 (43 A) handles this easily,
> but the **12 V supply/battery needs ≥ 15–20 A surge capability** — a 10 A supply may brown
> out on stall. Fuse ~10–15 A on B+; 18 AWG wire is near its limit at stall current.

---

## Potentiometer (Shaft Angle Feedback)

| Pot pin | Connection | Notes |
|---------|------------|-------|
| Pin 1 (end) | ESP32 3.3 V | Reference high |
| Pin 2 (wiper) | ESP32 GPIO 34 | `PIN_POT` — ADC1_CH6 |
| Pin 3 (end) | ESP32 GND | Reference low |

> Use ADC1 pins only (GPIO 32–39). ADC2 is disabled while WiFi is active.
> Run `runCalibration()` on first assembly to find `POT_ADC_MIN` / `POT_ADC_MAX`.

---

## Limit Switches — REIFENG mechanical endstop (SPDT, C/NO/NC)

One switch at each end of travel. The board has an SPDT microswitch (terminals
**C / NO / NC**, rated 1 A 125 VAC) on a small PCB with mounting holes and a 3-wire
cable (red/black/white). Firmware uses `INPUT_PULLDOWN` + `LIMIT_ACTIVE HIGH`, so the
switch must connect the GPIO to **3.3 V** when pressed — use the **C and NO** contacts.

| Switch | ESP32 GPIO | Config symbol | Triggers when... |
|--------|------------|---------------|-----------------|
| CW (west) limit | GPIO 32 | `PIN_LIMIT_CW` | Panel reaches full west travel |
| CCW (east) limit | GPIO 33 | `PIN_LIMIT_CCW` | Panel reaches full east travel |

| Switch contact | Connect to |
|----------------|------------|
| C (common) | ESP32 3.3 V |
| NO (normally open) | ESP32 GPIO 32 (CW) or 33 (CCW) |
| NC (normally closed) | not connected |

> **Identify the wires** (red/black/white order isn't guaranteed) with a multimeter in
> continuity mode, or with the bring-up tool:
> 1. Lever **released** → the two wires that beep are **C and NC**.
> 2. Lever **pressed** → the two wires that beep are **C and NO**.
> 3. The wire common to both is **C** (→ 3.3 V); the other from step 2 is **NO** (→ GPIO).
>
> Not weather-sealed — mount inside the electronics enclosure for a permanent outdoor install.
> (To wire C+NC instead, change `LIMIT_ACTIVE` to `LOW` and the pinMode to `INPUT_PULLUP`.)

---

## Anemometer (Wind Speed) — QS-FS, part C2192-002

> ⚠️ **This is NOT a passive reed switch.** Per `datasheets/C2192-002_datasheet.pdf`, the
> QS-FS is a **powered electromagnetic-induction sensor** that needs its own supply and
> outputs an actively-driven signal. This changes the wiring substantially from the earlier
> reed-switch assumption.

**Specs:** Supply 5–24 VDC · output options (variant-dependent): **Pulse (5 V)**, 4–20 mA, 0–5 V/1–5 V, or RS485 · range 0–60 m/s · starting threshold ≤ 0.8 m/s · IP55.

**3-wire connection (Pulse / current / voltage variants):**

| Function | Wire color (2 color schemes) | Connect to |
|----------|------------------------------|------------|
| Power | red **or** brown | ESP32 5 V (VIN) — sensor needs 5–24 V, use 5 V |
| Ground | blue **or** black | ESP32 GND (common) |
| Signal | yellow **or** blue | GPIO 35 (`PIN_ANEMOMETER`) **via level shift — see below** |

> ⚠️ **Two things to resolve before wiring this:**
>
> 1. **Confirm which output variant you bought.** The current firmware counts digital pulses
>    (`RISING`-edge interrupt), so it only works with the **Pulse (5 V)** variant. If you have
>    the 4–20 mA, 0–5 V, or RS485 version, the firmware and wiring must change (ADC + sense
>    resistor, or a MAX485/UART transceiver). The "-002" suffix likely encodes the variant —
>    verify with the supplier.
>
> 2. **5 V signal must be dropped to 3.3 V.** The pulse output is **5 V**, but GPIO 35's
>    absolute max is ~3.6 V — driving it at 5 V can damage the pin. Use a **voltage divider**
>    on the signal line: signal → **10 kΩ** → GPIO 35, and GPIO 35 → **20 kΩ** → GND
>    (gives 5 V × 20/30 ≈ 3.3 V). A logic-level shifter also works. (No pull-up resistor is
>    needed anymore — the sensor actively drives the line; the old reed-switch pull-up note is
>    obsolete. If bench testing shows the output is open-collector rather than push-pull, add a
>    pull-up to 5 V *before* the divider.)
>
> 3. **Recalibrate `ANEM_MPH_PER_HZ`.** The default `1.49` was for a SparkFun reed-switch
>    anemometer and is wrong for the QS-FS. Get the pulse-frequency-to-wind-speed relation from
>    the QS-FS manual/supplier and update `config.h`.

---

## LDR Sensors (Diagnostic)

Voltage divider: 3.3 V → LDR → ADC pin → 10 kΩ to GND. Used only to cross-check the astronomical algorithm — not required for tracking.

| LDR | ESP32 GPIO | Config symbol | Position |
|-----|------------|---------------|----------|
| East LDR | GPIO 36 | `PIN_LDR_EAST` | Faces east (morning sun) |
| West LDR | GPIO 39 | `PIN_LDR_WEST` | Faces west (afternoon sun) |

| Node | Connection |
|------|------------|
| LDR top | ESP32 3.3 V |
| LDR bottom / ADC junction | ESP32 GPIO 36 or 39 |
| 10 kΩ resistor (top) | ADC junction |
| 10 kΩ resistor (bottom) | ESP32 GND |

---

## DS3231 RTC Module (I2C)

| DS3231 pin | ESP32 GPIO | Config symbol | Notes |
|------------|------------|---------------|-------|
| VCC | **3.3 V** | — | Power from 3.3 V — see safety note below |
| GND | GND | — | |
| SDA | GPIO 21 | `RTC_SDA` | I2C data, address 0x68 |
| SCL | GPIO 22 | `RTC_SCL` | I2C clock |
| SQW / INT | — | — | Not connected |
| 32K | — | — | Not connected |

> **Power from 3.3 V, not 5 V (safety).** The common ZS-042 DS3231 board has an onboard battery-charging circuit (diode + 200 Ω) intended for a rechargeable LIR2032. If you power VCC at 5 V while using a **non-rechargeable CR2032**, it pushes ~5 mA into a cell that cannot take a charge — risk of overheating/leakage. Powering from 3.3 V keeps the diode reverse-biased and prevents charging. (If you must run 5 V, fit a LIR2032 or remove the charging resistor.)
>
> The DS3231 chip's I2C address is **0x68**; the onboard EEPROM is 0x57 (or 0x50–0x57). The bring-up I2C scan should report 0x68.
>
> Install a CR2032 coin cell so the RTC keeps time through power loss.
> The module includes onboard I2C pull-up resistors (typically 4.7 kΩ) — do not add external pull-ups.

---

## ESP32 GPIO Summary

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 14 | Motor L_EN | Output | BTS7960 left enable |
| 21 | I2C SDA | Bidirectional | DS3231 RTC |
| 22 | I2C SCL | Output | DS3231 RTC |
| 25 | Motor RPWM | Output | PWM CW |
| 26 | Motor LPWM | Output | PWM CCW |
| 27 | Motor R_EN | Output | BTS7960 right enable |
| 32 | Limit CW | Input (pull-down) | West end-stop |
| 33 | Limit CCW | Input (pull-down) | East end-stop |
| 34 | Potentiometer | ADC1 input | Shaft angle feedback |
| 35 | Anemometer | Input-only | QS-FS pulse — **5 V signal, must divide to 3.3 V** (10 k/20 k) |
| 36 | LDR East | ADC1 input | Diagnostic |
| 39 | LDR West | ADC1 input | Diagnostic |
