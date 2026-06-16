#ifndef SIMULATE   // entire file excluded when running simulation

#include "motor_control.h"
#include "config.h"
#include <Arduino.h>

// ── Internal helpers ──────────────────────────────────────────────────────────

static int readPotRaw() {
    long sum = 0;
    for (int i = 0; i < POT_SAMPLES; i++) {
        sum += analogRead(PIN_POT);
        delay(2);
    }
    return (int)(sum / POT_SAMPLES);
}

static float adcToAngle(int adc) {
    return (float)(adc - POT_ADC_MIN) / (float)(POT_ADC_MAX - POT_ADC_MIN)
           * (PANEL_ANGLE_MAX - PANEL_ANGLE_MIN) + PANEL_ANGLE_MIN;
}

// Proportional speed: faster when far away, slow near target, never below MIN.
static uint8_t calcSpeed(float absError) {
    int spd = (int)(absError * 6.0f) + MOTOR_PWM_MIN;
    return (uint8_t)constrain(spd, MOTOR_PWM_MIN, MOTOR_PWM_MAX);
}

static void driveCW(uint8_t speed) {
    ledcWrite(PWM_CHANNEL_L, 0);
    ledcWrite(PWM_CHANNEL_R, speed);
}

static void driveCCW(uint8_t speed) {
    ledcWrite(PWM_CHANNEL_R, 0);
    ledcWrite(PWM_CHANNEL_L, speed);
}

// ── Public API ────────────────────────────────────────────────────────────────

void motorInit() {
    ledcSetup(PWM_CHANNEL_R, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_L, PWM_FREQ_HZ, PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_RPWM, PWM_CHANNEL_R);
    ledcAttachPin(PIN_MOTOR_LPWM, PWM_CHANNEL_L);

    pinMode(PIN_MOTOR_REN, OUTPUT);
    pinMode(PIN_MOTOR_LEN, OUTPUT);
    digitalWrite(PIN_MOTOR_REN, HIGH);
    digitalWrite(PIN_MOTOR_LEN, HIGH);

    pinMode(PIN_LIMIT_CW,  INPUT_PULLDOWN);
    pinMode(PIN_LIMIT_CCW, INPUT_PULLDOWN);

    analogReadResolution(12);   // 12-bit ADC = 0–4095
    motorStop();
}

float readPanelAngle() {
    return adcToAngle(readPotRaw());
}

void motorStop() {
    ledcWrite(PWM_CHANNEL_R, 0);
    ledcWrite(PWM_CHANNEL_L, 0);
}

void motorBrake() {
    ledcWrite(PWM_CHANNEL_R, 255);
    ledcWrite(PWM_CHANNEL_L, 255);
    delay(80);
    motorStop();
}

bool driveToAngle(float targetDeg) {
    targetDeg = constrain(targetDeg, PANEL_ANGLE_MIN, PANEL_ANGLE_MAX);
    unsigned long startMs = millis();

    while (true) {
        if (millis() - startMs > DRIVE_TIMEOUT_MS) {
            motorStop();
            Serial.println("[MOTOR] ERROR: move timed out — check pot wiring");
            return false;
        }

        float current = readPanelAngle();
        float error   = targetDeg - current;

        if (fabsf(error) <= MOTOR_DEADBAND_DEG) {
            motorStop();
            return true;
        }

        if (error > 0) {
            // Tilt toward west (CW)
            if (digitalRead(PIN_LIMIT_CW) == LIMIT_ACTIVE) {
                motorStop();
                Serial.println("[MOTOR] CW limit switch tripped");
                return false;
            }
            driveCW(calcSpeed(fabsf(error)));
        } else {
            // Tilt toward east (CCW)
            if (digitalRead(PIN_LIMIT_CCW) == LIMIT_ACTIVE) {
                motorStop();
                Serial.println("[MOTOR] CCW limit switch tripped");
                return false;
            }
            driveCCW(calcSpeed(fabsf(error)));
        }
        delay(50);
    }
}

void runCalibration() {
    Serial.println("=== POTENTIOMETER CALIBRATION ===");
    Serial.println("1. Manually move panel to EAST limit (-30 deg).");
    Serial.println("   Then send any character over Serial...");
    while (!Serial.available()) delay(100);
    Serial.read();
    int minRaw = readPotRaw();
    Serial.print("   East ADC reading: "); Serial.println(minRaw);

    Serial.println("2. Manually move panel to WEST limit (+30 deg).");
    Serial.println("   Then send any character over Serial...");
    while (!Serial.available()) delay(100);
    Serial.read();
    int maxRaw = readPotRaw();
    Serial.print("   West ADC reading: "); Serial.println(maxRaw);

    Serial.println("=== Update POT_ADC_MIN and POT_ADC_MAX in config.h ===");
    Serial.printf("   #define POT_ADC_MIN  %d\n", minRaw);
    Serial.printf("   #define POT_ADC_MAX  %d\n", maxRaw);
}

#endif // SIMULATE
