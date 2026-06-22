# SolarMax — Parts & Purchase Links

Buy-list only. Component pinouts and wiring live in [wiring.md](wiring.md) /
[wiring_diagram.pdf](wiring_diagram.pdf) — this file is **only** where to buy each part.
Printable version: [links_to_parts.pdf](links_to_parts.pdf).

| # | Part | Qty | Buy |
|---|------|-----|-----|
| 1 | ESP32 DevKit V1 (WROOM-32) | 1 | _needs link_ |
| 2 | BTS7960 43A H-Bridge module | 1 | _needs link_ |
| 3 | DC worm gear motor, 12 V (5882-50ZY) | 1 | _needs link_ |
| 4 | P160 16 mm rotary potentiometer | 1 | _needs link_ |
| 5 | DS3231 RTC module | 1 | [Amazon (B07Q7NZTQS)](https://www.amazon.com/dp/B07Q7NZTQS) |
| 6 | Limit switch — REIFENG endstop | 2 | [Amazon (B07PCN6T6F)](https://www.amazon.com/dp/B07PCN6T6F) |
| 7 | Anemometer ⚠️ | 1 | [Adafruit (1733)](https://www.adafruit.com/product/1733) |
| 8 | LDR / photocell | 2 | _needs link_ |
| 9 | 10 kΩ resistor | 2 | _needs link_ |
| 10 | 10 kΩ + 20 kΩ resistor (anemometer divider) | 1 ea | _needs link_ |
| 11 | 12 V 20 A power supply | 1 | [Amazon (B078RTV41D)](https://www.amazon.com/dp/B078RTV41D) |
| 12 | Buck converter (12 V→5 V) | 1 | [Amazon (B0D45KCDCX)](https://www.amazon.com/dp/B0D45KCDCX) |
| 13 | 12 AWG wire | 1 | [Amazon (B0D97CJYY7)](https://www.amazon.com/dp/B0D97CJYY7) |
| 14 | 18–22 AWG wire | 1 | _needs link_ |
| 15 | Schottky diode (1N5822) | 1 | [Amazon (B0CKSGFQ5T)](https://www.amazon.com/dp/B0CKSGFQ5T) |
| 16 | Lever wire connectors | 1 | [Amazon (B09CKDWK4Q)](https://www.amazon.com/dp/B09CKDWK4Q) |
| 17 | E-stop button (Baomain) | 1 | [Amazon (B00NTT91Y0)](https://www.amazon.com/dp/B00NTT91Y0) |
| 18 | Main switch (battery disconnect) | 1 | [Amazon (B0CXWTKDJB)](https://www.amazon.com/dp/B0CXWTKDJB) |
| — | Renogy 50 W solar panel (RNG-50D-SS) | 1 | _needs link_ |

> ⚠️ **Anemometer:** the linked Adafruit 1733 is an **analog (0.4–2.0 V)** sensor, but the
> firmware counts **pulses**. Pick the matching variant or update `config.h` before ordering.
