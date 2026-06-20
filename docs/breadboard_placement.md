# SolarMax — Breadboard Placement Map

This is the **single source of truth** for the breadboard layout. Both the diagram
([wiring_breadboard.pdf](wiring_breadboard.pdf)) and any Fritzing build are made from
this table, so wire endpoints can't drift.

## Breadboard basics (read once)
- A breadboard has **4 long power rails**: two on top, two on bottom. Each rail is one
  continuous metal strip — every hole in it is the same electrical point.
  - **Top red rail = +5 V**, **Top blue rail = GND**
  - **Bottom red rail = +3.3 V**, **Bottom blue rail = GND**
- We **link the two blue (GND) rails** with a jumper at each end → that is the "common
  ground." Every GND wire just plugs into the nearest blue rail.
- The middle has **numbered columns**. Holes in the same column (within one half) are
  connected vertically. We place each part so its pins sit in known columns, then run
  jumpers from those columns to the rails or to ESP32 pins.
- **Columns below refer to the diagram's grid** (1 = far left). They are relative
  positions to keep wiring unambiguous, not specific to one breadboard brand.

## Power rails — who feeds them
| Rail | Fed by | Notes |
|------|--------|-------|
| +5 V (top red) | Buck OUT+ **through a Schottky diode** → ESP32 5V pin | buck makes 5 V from 12 V; the diode lets the USB port stay pluggable for programming with no conflict (USB = programming only) |
| GND (top blue) | Buck OUT−, ESP32 GND pin | linked to bottom GND |
| +3.3 V (bottom red) | ESP32 3V3 pin | ESP32 regulates its own 3.3 V |
| GND (bottom blue) | linked to top GND at both ends | common ground |

## Connection map (every wire)
"col" = grid column the pin sits in. "→" = the wire from that pin.

### ESP32 (straddles the center gap)
| ESP pin | col | wire → |
|---------|-----|--------|
| 5V (VIN) | 4 (top) | → +5 V rail |
| GND | 5 (top) | → GND rail |
| 3V3 | 6 (bottom) | → +3.3 V rail |
| GPIO25 | 20 (top) | → BTS7960 RPWM |
| GPIO26 | 21 (top) | → BTS7960 LPWM |
| GPIO27 | 22 (top) | → BTS7960 R_EN |
| GPIO14 | 23 (top) | → BTS7960 L_EN |
| GPIO21 | 2 (bottom) | → RTC SDA |
| GPIO22 | 3 (bottom) | → RTC SCL |
| GPIO34 | 8 (bottom) | → Pot wiper |
| GPIO32 | 11 (bottom) | → Limit CW NO |
| GPIO33 | 13 (bottom) | → Limit CCW NO |
| GPIO36 | 15 (bottom) | → LDR-east node |
| GPIO39 | 19 (bottom) | → LDR-west node |
| GPIO35 | 23 (bottom) | → anemometer divider node |

### Buck converter (top-left)
| pin | col | wire → |
|-----|-----|--------|
| OUT+ | 2 | → **Schottky diode** → +5 V rail (diode lets USB coexist) |
| OUT− | 3 | → GND rail |
| IN+ / IN− | off-board | → 12 V PSU (heavy wire) |

### BTS7960 (top-right; logic on board, power off-board)
| pin | col | wire → |
|-----|-----|--------|
| RPWM | 20 | → ESP GPIO25 |
| LPWM | 21 | → ESP GPIO26 |
| R_EN | 22 | → ESP GPIO27 |
| L_EN | 23 | → ESP GPIO14 |
| VCC | 20 | → +5 V rail |
| GND | 21 | → GND rail |
| B+ / B− | off-board | → 12 V PSU (heavy) |
| M+ / M− | off-board | → motor (heavy) |

### DS3231 RTC (bottom)
| pin | col | wire → |
|-----|-----|--------|
| SDA | 2 | → ESP GPIO21 |
| SCL | 3 | → ESP GPIO22 |
| VCC | 4 | → +3.3 V rail |
| GND | 5 | → GND rail |

### Potentiometer (bottom)
| pin | col | wire → |
|-----|-----|--------|
| 1 (end) | 7 | → +3.3 V rail |
| W (wiper) | 8 | → ESP GPIO34 |
| 3 (end) | 9 | → GND rail |

### Limit switches (bottom)
| switch | pin | col | wire → |
|--------|-----|-----|--------|
| CW | C | 10 | → +3.3 V rail |
| CW | NO | 11 | → ESP GPIO32 |
| CCW | C | 12 | → +3.3 V rail |
| CCW | NO | 13 | → ESP GPIO33 |

### LDR dividers  (3.3 V — LDR — node — 10 kΩ — GND; node → ADC)
| part | cols | wire → |
|------|------|--------|
| LDR east | 14–15 | col14 → +3.3 V rail; node col15 → ESP GPIO36 |
| 10 kΩ east | 15–16 | col16 → GND rail |
| LDR west | 18–19 | col18 → +3.3 V rail; node col19 → ESP GPIO39 |
| 10 kΩ west | 19–20 | col20 → GND rail |

### Anemometer + divider (bottom-right; S — 10 kΩ — node — 20 kΩ — GND)
| part | cols | wire → |
|------|------|--------|
| Anem 5V (power) | 26 | → +5 V rail (clear channel right of ESP) |
| Anem GND | 27 | → GND rail |
| Anem S (signal) | 24 | → 10 kΩ |
| 10 kΩ | 23–24 | node col23 → ESP GPIO35 |
| 20 kΩ | 22–23 | col22 → GND rail |

## Off-board (NOT on the breadboard)
The motor draws ~15–19 A at stall — **never through a breadboard**. These use heavy wire
and the BTS7960 screw terminals directly:
- 12 V PSU (+) → BTS7960 **B+** and buck **IN+**
- 12 V PSU (−) → BTS7960 **B−** and buck **IN−**
- BTS7960 **M+ / M−** → motor leads

## Fritzing parts availability
| Component | In Fritzing? |
|-----------|--------------|
| Breadboard, resistors, LDR/photoresistor, potentiometer, DC motor, basic microswitch | ✅ Built-in (CORE bin) |
| ESP32 DevKit V1 | ⚠️ Import community part (search "DOIT ESP32 DevKit V1 fritzing", `.fzpz`) |
| BTS7960 / IBT-2 driver | ⚠️ Community import |
| DS3231 RTC module | ⚠️ Community import |
| LM2596 buck converter | ⚠️ Community import |
| QS-FS anemometer | ❌ No part — use a generic 3-pin header/sensor and label it |

Import a `.fzpz` via **File → Open** (it lands in the MINE bin). For the anemometer, a generic
3-pin connector stands in for power / GND / signal.

## Building it in Fritzing (option B)
1. Drop a breadboard, an ESP32 DevKit, and the modules from the parts bin.
2. Place each part so its pins land in the columns above.
3. Wire by the "wire →" column. Fritzing snaps wires to holes, so endpoints can't float.
4. Color each wire to match the legend (red 5 V, orange 3.3 V, blue GND, etc.).
5. Keep the motor/PSU/heavy wiring OFF the breadboard (use a separate "schematic" note).
