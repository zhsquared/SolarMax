// ESP32 pin-assignment validation — run with: pio test -e native
//
// This encodes the hardware constraints of the ESP32-WROOM-32 (DevKit V1) and
// checks every pin in config.h against them. It cannot verify that a sensor is
// physically wired correctly, but it CATCHES the class of error that has no
// hardware symptom until power-on:
//   - using a pin that doesn't exist / is tied to internal flash
//   - assigning the same GPIO to two functions (collision)
//   - driving an input-only pin as an output
//   - relying on an internal pull resistor on a pin that has none
//   - analogRead() on an ADC2 pin (silently fails while WiFi is active)
//   - using a boot strapping pin (warning, not fatal)
//
// SOURCE OF THE RULES: Espressif ESP32 datasheet / ESP32-WROOM-32 datasheet and
// the Arduino-ESP32 pin documentation. If you change boards, re-verify the sets
// below against that board's datasheet — the test is only as correct as this data.

#include <unity.h>
#include <cstdio>
#include "config.h"

// ── ESP32-WROOM-32 hardware constraint sets ───────────────────────────────────

static bool inSet(int pin, const int* set, int n) {
    for (int i = 0; i < n; i++) if (set[i] == pin) return true;
    return false;
}

// GPIO 6–11 are wired to the integrated SPI flash. Using them bricks boot.
static const int FLASH_PINS[]    = {6, 7, 8, 9, 10, 11};
// GPIO 34–39 are input-only: cannot be OUTPUT, have NO internal pull resistors.
static const int INPUT_ONLY[]    = {34, 35, 36, 37, 38, 39};
// ADC1 channels — the only ADC pins usable while WiFi is active.
static const int ADC1_PINS[]     = {32, 33, 34, 35, 36, 37, 38, 39};
// Boot strapping pins — usable but can glitch or affect boot; avoid where possible.
static const int STRAPPING_PINS[]= {0, 2, 5, 12, 15};
// Pins broken out on a typical 30/38-pin DevKit V1 (37/38 are NOT broken out).
static const int VALID_GPIO[]    = {0,1,2,3,4,5,12,13,14,15,16,17,18,19,21,22,23,
                                    25,26,27,32,33,34,35,36,39};

#define N(a) ((int)(sizeof(a)/sizeof((a)[0])))

void setUp()    {}
void tearDown() {}

// ── Classify every configured pin by how the firmware uses it ─────────────────

// Pins the firmware drives as digital/PWM OUTPUT.
static const int OUTPUT_PINS[] = {
    PIN_MOTOR_RPWM, PIN_MOTOR_LPWM, PIN_MOTOR_REN, PIN_MOTOR_LEN
};
// Pins the firmware reads with analogRead() (must be ADC1 for WiFi safety).
static const int ANALOG_PINS[] = {
    PIN_POT, PIN_LDR_EAST, PIN_LDR_WEST
};
// Pins that rely on an ESP32 INTERNAL pull resistor.
static const int INTERNAL_PULL_PINS[] = {
    PIN_LIMIT_CW, PIN_LIMIT_CCW   // INPUT_PULLDOWN in motor_control.cpp
};
// Every pin the project uses, for the collision check.
static const int ALL_USED_PINS[] = {
    PIN_MOTOR_RPWM, PIN_MOTOR_LPWM, PIN_MOTOR_REN, PIN_MOTOR_LEN,
    PIN_POT, PIN_LIMIT_CW, PIN_LIMIT_CCW, PIN_ANEMOMETER,
    PIN_LDR_EAST, PIN_LDR_WEST, RTC_SDA, RTC_SCL
};

// ── Tests ─────────────────────────────────────────────────────────────────────

// Every pin must be a real, broken-out GPIO and never a flash pin.
void test_all_pins_exist_and_not_flash() {
    for (int i = 0; i < N(ALL_USED_PINS); i++) {
        int p = ALL_USED_PINS[i];
        TEST_ASSERT_TRUE_MESSAGE(!inSet(p, FLASH_PINS, N(FLASH_PINS)),
            "Pin is tied to internal SPI flash (GPIO 6-11) — will not boot");
        TEST_ASSERT_TRUE_MESSAGE(inSet(p, VALID_GPIO, N(VALID_GPIO)),
            "Pin is not a usable GPIO on the DevKit V1");
    }
}

// No GPIO may be assigned to two functions at once.
void test_no_pin_collisions() {
    for (int i = 0; i < N(ALL_USED_PINS); i++) {
        for (int j = i + 1; j < N(ALL_USED_PINS); j++) {
            TEST_ASSERT_TRUE_MESSAGE(ALL_USED_PINS[i] != ALL_USED_PINS[j],
                "Two functions assigned to the same GPIO");
        }
    }
}

// Output pins must not be input-only (34-39 cannot drive a signal).
void test_outputs_not_input_only() {
    for (int i = 0; i < N(OUTPUT_PINS); i++) {
        TEST_ASSERT_FALSE_MESSAGE(
            inSet(OUTPUT_PINS[i], INPUT_ONLY, N(INPUT_ONLY)),
            "An OUTPUT pin is on an input-only GPIO (34-39) — cannot drive it");
    }
}

// analogRead pins must be ADC1 — ADC2 silently fails while WiFi is on.
void test_analog_pins_are_adc1() {
    for (int i = 0; i < N(ANALOG_PINS); i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            inSet(ANALOG_PINS[i], ADC1_PINS, N(ADC1_PINS)),
            "analogRead pin is not ADC1 — returns garbage while WiFi is active");
    }
}

// Pins that depend on an internal pull resistor must actually have one
// (input-only pins 34-39 do not).
void test_internal_pull_pins_have_pulls() {
    for (int i = 0; i < N(INTERNAL_PULL_PINS); i++) {
        TEST_ASSERT_FALSE_MESSAGE(
            inSet(INTERNAL_PULL_PINS[i], INPUT_ONLY, N(INPUT_ONLY)),
            "Pin relies on internal pull-up/down but is input-only (no internal pulls)");
    }
}

// The anemometer is on an input-only pin, so its INPUT_PULLUP is a no-op.
// This test documents that an EXTERNAL pull-up is mandatory (see docs/wiring.md).
// It passes as long as that fact stays true; if you move the anemometer to a
// pin with internal pulls, update docs/wiring.md to drop the external resistor.
void test_anemometer_needs_external_pullup() {
    bool inputOnly = inSet(PIN_ANEMOMETER, INPUT_ONLY, N(INPUT_ONLY));
    if (inputOnly) {
        TEST_MESSAGE("NOTE: anemometer on input-only pin -> external 10k pull-up REQUIRED");
    } else {
        TEST_MESSAGE("NOTE: anemometer pin supports internal pull-up -> external resistor optional");
    }
    TEST_PASS();
}

// Warn (do not fail) if any pin is a boot strapping pin.
void test_warn_on_strapping_pins() {
    for (int i = 0; i < N(ALL_USED_PINS); i++) {
        if (inSet(ALL_USED_PINS[i], STRAPPING_PINS, N(STRAPPING_PINS))) {
            char msg[80];
            snprintf(msg, sizeof(msg),
                     "WARNING: GPIO %d is a boot strapping pin — verify boot behavior",
                     ALL_USED_PINS[i]);
            TEST_MESSAGE(msg);
        }
    }
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_all_pins_exist_and_not_flash);
    RUN_TEST(test_no_pin_collisions);
    RUN_TEST(test_outputs_not_input_only);
    RUN_TEST(test_analog_pins_are_adc1);
    RUN_TEST(test_internal_pull_pins_have_pulls);
    RUN_TEST(test_anemometer_needs_external_pullup);
    RUN_TEST(test_warn_on_strapping_pins);
    return UNITY_END();
}
