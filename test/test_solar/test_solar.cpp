// Unit tests for the NOAA solar position algorithm.
// Run with: pio test -e native
//
// All tests use Tempe AZ coordinates (33.4255°N, -111.94°W).
// Expected values are derived from orbital mechanics; tolerances
// account for EoT approximations in the NOAA algorithm.

#include <unity.h>
#include "solar_position.h"
#include "config.h"

void setUp()    {}
void tearDown() {}

// ── Test 1: Summer solstice solar noon ────────────────────────────────────────
// 2026-06-21 ~19:30 UTC — sun is nearly overhead in Arizona.
// Expected: elevation ≈ 80°, azimuth ≈ 180°, panel ≈ 0°.
void test_summer_solstice_noon() {
    SolarAngles sa = calculateSolarPositionRaw(2026, 6, 21, 19.5,
                                               LATITUDE, LONGITUDE);
    TEST_ASSERT_TRUE(sa.aboveHorizon);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, 80.0f, sa.elevation);
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 180.0f, sa.azimuth);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, 0.0f, sa.panelAngle);
}

// ── Test 2: Winter solstice solar noon ────────────────────────────────────────
// 2026-12-21 ~19:26 UTC — sun is at its lowest noon elevation for Tempe.
// Expected: elevation ≈ 33°.
void test_winter_solstice_noon() {
    SolarAngles sa = calculateSolarPositionRaw(2026, 12, 21, 19.43,
                                               LATITUDE, LONGITUDE);
    TEST_ASSERT_TRUE(sa.aboveHorizon);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, 33.1f, sa.elevation);
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 180.0f, sa.azimuth);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, 0.0f, sa.panelAngle);
}

// ── Test 3: Spring equinox solar noon ────────────────────────────────────────
// 2026-03-20 ~19:35 UTC. Declination ≈ 0°, so elevation = 90° - lat ≈ 56.6°.
void test_spring_equinox_noon() {
    SolarAngles sa = calculateSolarPositionRaw(2026, 3, 20, 19.58,
                                               LATITUDE, LONGITUDE);
    TEST_ASSERT_TRUE(sa.aboveHorizon);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, 56.6f, sa.elevation);
    TEST_ASSERT_FLOAT_WITHIN(15.0f, 180.0f, sa.azimuth);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, 0.0f, sa.panelAngle);
}

// ── Test 4: Morning — panel clamps to east mechanical limit ──────────────────
// 2026-06-21 13:00 UTC = 6:00 AM local. Sun is low in the east; the tracker
// formula would request ~-83° but the clamp stops it at PANEL_ANGLE_MIN.
void test_morning_panel_at_east_limit() {
    SolarAngles sa = calculateSolarPositionRaw(2026, 6, 21, 13.0,
                                               LATITUDE, LONGITUDE);
    TEST_ASSERT_TRUE(sa.elevation > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, PANEL_ANGLE_MIN, sa.panelAngle);
}

// ── Test 5: Mid-afternoon — panel tilts west ──────────────────────────────────
// 2026-06-21 22:00 UTC = 3:00 PM local. Sun well into western sky.
// Panel should be positive (tilted west) and elevation should be high.
void test_afternoon_panel_west() {
    SolarAngles sa = calculateSolarPositionRaw(2026, 6, 21, 22.0,
                                               LATITUDE, LONGITUDE);
    TEST_ASSERT_TRUE(sa.aboveHorizon);
    TEST_ASSERT_TRUE(sa.elevation > 40.0f);
    TEST_ASSERT_TRUE(sa.panelAngle > 0.0f);
}

// ── Test 6: Night — sun below horizon ────────────────────────────────────────
// 2026-06-21 04:00 UTC = previous evening local. Sun is well below horizon.
void test_night_below_horizon() {
    SolarAngles sa = calculateSolarPositionRaw(2026, 6, 21, 4.0,
                                               LATITUDE, LONGITUDE);
    TEST_ASSERT_FALSE(sa.aboveHorizon);
    TEST_ASSERT_TRUE(sa.elevation < 0.0f);
}

// ── Test 7: Sign convention (morning < 0, afternoon > 0) ─────────────────────
// Panel angle must track east (negative) in morning and west (positive) in
// afternoon — this is the fundamental behaviour of a N-S horizontal tracker.
void test_panel_sign_convention() {
    // 8:00 AM local = 15:00 UTC — sun east of south
    SolarAngles morning = calculateSolarPositionRaw(2026, 6, 21, 15.0,
                                                    LATITUDE, LONGITUDE);
    // 4:00 PM local = 23:00 UTC — sun west of south
    SolarAngles afternoon = calculateSolarPositionRaw(2026, 6, 21, 23.0,
                                                      LATITUDE, LONGITUDE);
    TEST_ASSERT_TRUE(morning.panelAngle < 0.0f);
    TEST_ASSERT_TRUE(afternoon.panelAngle > 0.0f);
}

// ── Test 8: Mechanical limits never exceeded ──────────────────────────────────
// For every hour of a full simulated day the panel angle must stay within
// [PANEL_ANGLE_MIN, PANEL_ANGLE_MAX]. Tests the clamp logic in the algorithm.
void test_panel_within_mechanical_limits() {
    for (int h = 0; h < 24; h++) {
        SolarAngles sa = calculateSolarPositionRaw(2026, 6, 21, (double)h,
                                                   LATITUDE, LONGITUDE);
        TEST_ASSERT_TRUE(sa.panelAngle >= PANEL_ANGLE_MIN);
        TEST_ASSERT_TRUE(sa.panelAngle <= PANEL_ANGLE_MAX);
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_summer_solstice_noon);
    RUN_TEST(test_winter_solstice_noon);
    RUN_TEST(test_spring_equinox_noon);
    RUN_TEST(test_morning_panel_at_east_limit);
    RUN_TEST(test_afternoon_panel_west);
    RUN_TEST(test_night_below_horizon);
    RUN_TEST(test_panel_sign_convention);
    RUN_TEST(test_panel_within_mechanical_limits);
    return UNITY_END();
}
