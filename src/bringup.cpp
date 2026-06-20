#ifdef BRINGUP
// Hardware bring-up tool — flash to a real ESP32 to validate each peripheral
// one at a time, BEFORE the full system is assembled.
//
//   pio run -e bringup --target upload && pio device monitor
//
// Open Serial Monitor at 115200, then press a number to run a test:
//   1  I2C scan        — confirms DS3231 RTC wiring (expect device at 0x68)
//   2  Potentiometer   — live ADC + mapped angle (turn the pot, watch it move)
//   3  Limit switches  — live state of CW/CCW switches (press each to confirm)
//   4  Anemometer      — pulse count + computed mph over 5 s windows (spin it)
//   m  reprint this menu
//
// Each test runs until you press 'm' or another menu key. These are RAW reads
// (no project logic) so they isolate wiring from firmware behavior.

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

static void printMenu() {
    Serial.println("\n========= SolarMax Hardware Bring-Up =========");
    Serial.println(" 1  I2C scan        (DS3231 RTC — expect 0x68)");
    Serial.println(" 2  Potentiometer   (GPIO 34 ADC + angle)");
    Serial.println(" 3  Limit switches  (GPIO 32 CW / GPIO 33 CCW)");
    Serial.println(" 4  Anemometer      (GPIO 35 pulse count -> mph)");
    Serial.println(" m  reprint this menu");
    Serial.println("=============================================");
    Serial.print("Select test: ");
}

// Returns true if the user pressed a key (and leaves it consumed).
static bool keyPressed(char& c) {
    if (Serial.available()) { c = Serial.read(); return true; }
    c = 0;
    return false;
}

// ── Test 1: I2C scan ──────────────────────────────────────────────────────────
static void testI2CScan() {
    Serial.println("\n[I2C] Scanning bus on SDA=21 SCL=22 ... (press m to stop)");
    Wire.begin(RTC_SDA, RTC_SCL);
    while (true) {
        char c; if (keyPressed(c)) return;
        int found = 0;
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                Serial.printf("  found device at 0x%02X%s\n", addr,
                              addr == 0x68 ? "  <- DS3231 RTC" :
                              addr == 0x57 ? "  <- onboard EEPROM" : "");
                found++;
            }
        }
        if (found == 0)
            Serial.println("  no I2C devices — check SDA/SCL/VCC/GND and 3.3V power");
        else
            Serial.printf("  %d device(s) found\n", found);
        Serial.println("  ---");
        delay(1500);
    }
}

// ── Test 2: Potentiometer ─────────────────────────────────────────────────────
static void testPot() {
    Serial.println("\n[POT] Live read on GPIO 34. Turn the pot. (press m to stop)");
    analogReadResolution(12);
    while (true) {
        char c; if (keyPressed(c)) return;
        int raw = 0;
        for (int i = 0; i < 8; i++) { raw += analogRead(PIN_POT); delay(2); }
        raw /= 8;
        float angle = (float)(raw - POT_ADC_MIN) / (float)(POT_ADC_MAX - POT_ADC_MIN)
                      * (PANEL_ANGLE_MAX - PANEL_ANGLE_MIN) + PANEL_ANGLE_MIN;
        Serial.printf("  ADC=%4d  ->  angle=%6.1f deg   (cal range %d..%d)\n",
                      raw, angle, POT_ADC_MIN, POT_ADC_MAX);
        delay(250);
    }
}

// ── Test 3: Limit switches ────────────────────────────────────────────────────
static void testLimits() {
    Serial.println("\n[LIMITS] Live state. Press each switch. (press m to stop)");
    Serial.printf("  LIMIT_ACTIVE = %s\n", LIMIT_ACTIVE == HIGH ? "HIGH" : "LOW");
    pinMode(PIN_LIMIT_CW,  INPUT_PULLDOWN);
    pinMode(PIN_LIMIT_CCW, INPUT_PULLDOWN);
    while (true) {
        char c; if (keyPressed(c)) return;
        bool cw  = digitalRead(PIN_LIMIT_CW)  == LIMIT_ACTIVE;
        bool ccw = digitalRead(PIN_LIMIT_CCW) == LIMIT_ACTIVE;
        Serial.printf("  CW(32)=%s   CCW(33)=%s\n",
                      cw ? "TRIPPED" : "open   ",
                      ccw ? "TRIPPED" : "open");
        delay(250);
    }
}

// ── Test 4: Anemometer ────────────────────────────────────────────────────────
static volatile uint32_t _pulses = 0;
static void IRAM_ATTR anemISR() { _pulses++; }

static void testAnemometer() {
    Serial.println("\n[ANEM] Counting pulses on GPIO 35 over 5 s windows.");
    Serial.println("       Spin the cups. If it counts with no wind, the external");
    Serial.println("       pull-up is missing/floating. (press m to stop)");
    pinMode(PIN_ANEMOMETER, INPUT_PULLUP);   // no-op on GPIO 35 — needs ext. pull-up
    attachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER), anemISR, RISING);
    while (true) {
        char c; if (keyPressed(c)) { detachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER)); return; }
        _pulses = 0;
        uint32_t start = millis();
        while (millis() - start < 5000) {
            char k; if (keyPressed(k)) { detachInterrupt(digitalPinToInterrupt(PIN_ANEMOMETER)); return; }
            delay(10);
        }
        uint32_t count = _pulses;
        float hz  = count / 5.0f;
        float mph = hz * ANEM_MPH_PER_HZ;
        Serial.printf("  %lu pulses / 5s  =  %.2f Hz  ->  %.1f mph\n",
                      (unsigned long)count, hz, mph);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    printMenu();
}

void loop() {
    char c;
    if (!keyPressed(c)) { delay(20); return; }
    switch (c) {
        case '1': testI2CScan();     printMenu(); break;
        case '2': testPot();         printMenu(); break;
        case '3': testLimits();      printMenu(); break;
        case '4': testAnemometer();  printMenu(); break;
        case 'm': case 'M': printMenu(); break;
        case '\n': case '\r': break;
        default: Serial.printf("\nUnknown key '%c'\n", c); printMenu(); break;
    }
}

#endif // BRINGUP
