/**
 * Unit tests for governor_350A firmware logic.
 *
 * Strategy: mock all Arduino hardware APIs, then #include the .ino directly
 * so the pure logic functions are compiled into this native translation unit.
 * No hardware required — runs under PlatformIO's native/Unity test runner.
 *
 * Functions under test:
 *   calculateRPM()     — pulse-interval to RPM conversion + rolling average
 *   magSignalLost()    — magneto timeout detection
 *   updateRamp()       — setpoint ramping toward target
 *   runControlLoop()   — proportional relay direction/duty decisions
 */

// Pull in mock headers before the firmware so all Arduino symbols resolve.
#include "Arduino.h"
#include "Wire.h"
#include "EEPROM.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "avr/wdt.h"

// Include the firmware itself as a C++ translation unit.
// Forward-declare setAlarm so setRelay() can see it — Arduino IDE auto-generates
// these declarations from .ino files, but standard C++ compilers do not.
void setAlarm(const char* msg);
#include "../../governor_350A.ino"

#include <unity.h>

// =============================================================================
// Test fixture — reset all firmware global state before each test
// =============================================================================

void setUp(void) {
    mock_millis              = 0;
    mock_micros              = 0;
    memset(mock_pin, LOW, sizeof(mock_pin));

    currentMode              = MODE_MANUAL;
    relayState               = RELAY_OFF;
    alarmMessage             = "";

    activePreset             = 2;   // CRUISE
    lastValidPreset          = 2;

    rampedSetpoint           = (float)PRESET_CRUISE;
    targetSetpoint           = (float)PRESET_CRUISE;
    lastRampTime             = 0;

    lastPulseTime_us         = 0;
    pulseInterval_us         = 0;
    newPulse                 = false;

    memset(rpmBuffer, 0, sizeof(rpmBuffer));
    rpmBufIdx                = 0;
    rpmBufFull               = false;

    actualRPM                = 0.0f;
    lastMagPulse_ms          = 0;

    relayOnTime_ms           = 0;
    lastControlRun           = 0;
    lastDisplayUpdate        = 0;
    displayOK                = false;
}

void tearDown(void) {}


// =============================================================================
// calculateRPM() tests
// =============================================================================

void test_rpm_zero_when_no_pulse_interval(void) {
    pulseInterval_us = 0;
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, calculateRPM());
}

void test_rpm_zero_when_mag_signal_lost(void) {
    // 8696 µs interval would be ~2300 RPM with 3 PPR, but signal is stale.
    pulseInterval_us = 8696;
    mock_millis      = 5000;
    lastMagPulse_ms  = 0;  // 5 s ago — well past 2 s timeout
    TEST_ASSERT_EQUAL_FLOAT(0.0f, calculateRPM());
}

void test_rpm_correct_for_2300rpm_3ppr(void) {
    // Expected interval: 60,000,000 / (2300 * 3) = 8695.65 µs
    pulseInterval_us = 8696;
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 2300.0f, calculateRPM());
}

void test_rpm_correct_for_2650rpm_3ppr(void) {
    // Expected interval: 60,000,000 / (2650 * 3) = 7547.17 µs
    pulseInterval_us = 7547;
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 2650.0f, calculateRPM());
}

void test_rpm_correct_for_2200rpm_3ppr(void) {
    // Expected interval: 60,000,000 / (2200 * 3) = 9090.9 µs
    pulseInterval_us = 9091;
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 2200.0f, calculateRPM());
}

void test_rpm_rolling_average_smooths_spikes(void) {
    // Fill the averaging buffer with a steady 2300 RPM interval.
    mock_millis     = 1000;
    lastMagPulse_ms = 999;
    for (uint8_t i = 0; i < RPM_AVG_SAMPLES; i++) {
        pulseInterval_us = 8696;
        calculateRPM();
    }
    // Now inject a single spike (4000 RPM equivalent), then read.
    pulseInterval_us = 5000;
    float rpm = calculateRPM();
    // With 8 samples (7 at 8696 + 1 at 5000), average interval ~8321 µs → ~2401 RPM.
    // The key assertion: spike is dampened — result is nowhere near 4000 RPM.
    TEST_ASSERT_TRUE(rpm < 3000.0f);
    TEST_ASSERT_TRUE(rpm > 2000.0f);
}


// =============================================================================
// magSignalLost() tests
// =============================================================================

void test_mag_not_lost_when_fresh(void) {
    mock_millis     = 1000;
    lastMagPulse_ms = 999;  // 1 ms ago
    TEST_ASSERT_FALSE(magSignalLost());
}

void test_mag_not_lost_just_before_timeout(void) {
    mock_millis     = 1999;
    lastMagPulse_ms = 0;    // 1999 ms — just under 2000 ms timeout
    TEST_ASSERT_FALSE(magSignalLost());
}

void test_mag_lost_after_timeout(void) {
    mock_millis     = 2001;
    lastMagPulse_ms = 0;    // 2001 ms > MAG_TIMEOUT_MS (2000)
    TEST_ASSERT_TRUE(magSignalLost());
}


// =============================================================================
// updateRamp() tests
// =============================================================================

void test_ramp_moves_up_toward_higher_target(void) {
    activePreset   = 0;          // MAX — not fullCoarse
    rampedSetpoint = 2200.0f;
    targetSetpoint = 2300.0f;
    lastRampTime   = 0;
    mock_millis    = 500;        // 0.5 s → max step = 50 RPM

    updateRamp();

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2250.0f, rampedSetpoint);
}

void test_ramp_moves_down_toward_lower_target(void) {
    activePreset   = 0;
    rampedSetpoint = 2300.0f;
    targetSetpoint = 2200.0f;
    lastRampTime   = 0;
    mock_millis    = 500;        // 0.5 s → max step = 50 RPM

    updateRamp();

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2250.0f, rampedSetpoint);
}

void test_ramp_snaps_when_within_step(void) {
    activePreset   = 0;
    rampedSetpoint = 2295.0f;
    targetSetpoint = 2300.0f;   // only 5 RPM away
    lastRampTime   = 0;
    mock_millis    = 1000;      // 1 s → max step = 100 RPM > 5 RPM diff

    updateRamp();

    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
}

void test_ramp_does_not_overshoot(void) {
    activePreset   = 0;
    rampedSetpoint = 2299.0f;
    targetSetpoint = 2300.0f;
    lastRampTime   = 0;
    mock_millis    = 5000;      // huge elapsed time — must snap, not overshoot

    updateRamp();

    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
}

void test_ramp_bypassed_for_fullcoarse_preset(void) {
    activePreset   = 4;         // LOW/EMERGENCY — fullCoarse = true
    rampedSetpoint = 2200.0f;
    targetSetpoint = 0.0f;
    lastRampTime   = 0;
    mock_millis    = 1000;

    updateRamp();

    // rampedSetpoint must not change when fullCoarse is active.
    TEST_ASSERT_EQUAL_FLOAT(2200.0f, rampedSetpoint);
}


// =============================================================================
// runControlLoop() tests
// =============================================================================

// Helper: put the system into a valid AUTO state ready for control loop tests.
static void enterAutoMode(float setpoint, float actual) {
    currentMode      = MODE_AUTO;
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;    // mag signal fresh
    activePreset     = 2;      // CRUISE — not fullCoarse
    rampedSetpoint   = setpoint;
    actualRPM        = actual;
    relayOnTime_ms   = 0;
    relayState       = RELAY_OFF;
}

void test_control_relay_inc_when_below_setpoint(void) {
    enterAutoMode(2300.0f, 2100.0f);  // -200 RPM — outside deadband
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
}

void test_control_relay_dec_when_above_setpoint(void) {
    enterAutoMode(2300.0f, 2500.0f);  // +200 RPM — outside deadband
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_DEC, relayState);
}

void test_control_relay_off_within_deadband(void) {
    enterAutoMode(2300.0f, 2310.0f);  // +10 RPM — inside ±25 RPM deadband
    relayState = RELAY_INC;           // was driving; should cut off
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
}

void test_control_no_action_in_manual_mode(void) {
    enterAutoMode(2300.0f, 2100.0f);
    currentMode = MODE_MANUAL;        // override back to manual
    relayState  = RELAY_INC;
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
}

void test_control_alarm_triggers_on_mag_loss(void) {
    enterAutoMode(2300.0f, 2300.0f);
    mock_millis     = 5000;
    lastMagPulse_ms = 0;              // signal lost 5 s ago
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_ALARM, currentMode);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
}

void test_control_alarm_triggers_on_rpm_too_high(void) {
    enterAutoMode(2300.0f, 4000.0f);  // > 3500 RPM hard limit
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_ALARM, currentMode);
}

void test_control_alarm_triggers_on_rpm_too_low(void) {
    enterAutoMode(2300.0f, 200.0f);   // < 400 RPM hard limit (non-zero)
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_ALARM, currentMode);
}

void test_control_fullcoarse_drives_dec_relay(void) {
    enterAutoMode(0.0f, 2300.0f);
    activePreset = 4;                 // LOW/EMERGENCY — fullCoarse
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_DEC, relayState);
}

void test_control_full_duty_outside_prop_zone(void) {
    // Error >= PROP_ZONE_RPM (75) → duty must be 1.0 → relay stays on the
    // entire control window. With mock_millis % 100 == 0, shouldBeOn = true.
    enterAutoMode(2300.0f, 2150.0f);  // -150 RPM error
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
    // Verify the relay GPIO was actually driven HIGH.
    TEST_ASSERT_EQUAL(HIGH, mock_pin[PIN_INC_RELAY]);
}

void test_control_proportional_duty_in_prop_zone(void) {
    // Error of 50 RPM is inside PROP_ZONE_RPM (75) but outside DEADBAND (25).
    // Duty = 50/75 = 0.667 → onMs = 67. With pos = 0, shouldBeOn = true.
    enterAutoMode(2300.0f, 2250.0f);  // -50 RPM error
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
}


// =============================================================================
// Entry point
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    // calculateRPM
    RUN_TEST(test_rpm_zero_when_no_pulse_interval);
    RUN_TEST(test_rpm_zero_when_mag_signal_lost);
    RUN_TEST(test_rpm_correct_for_2300rpm_3ppr);
    RUN_TEST(test_rpm_correct_for_2650rpm_3ppr);
    RUN_TEST(test_rpm_correct_for_2200rpm_3ppr);
    RUN_TEST(test_rpm_rolling_average_smooths_spikes);

    // magSignalLost
    RUN_TEST(test_mag_not_lost_when_fresh);
    RUN_TEST(test_mag_not_lost_just_before_timeout);
    RUN_TEST(test_mag_lost_after_timeout);

    // updateRamp
    RUN_TEST(test_ramp_moves_up_toward_higher_target);
    RUN_TEST(test_ramp_moves_down_toward_lower_target);
    RUN_TEST(test_ramp_snaps_when_within_step);
    RUN_TEST(test_ramp_does_not_overshoot);
    RUN_TEST(test_ramp_bypassed_for_fullcoarse_preset);

    // runControlLoop
    RUN_TEST(test_control_relay_inc_when_below_setpoint);
    RUN_TEST(test_control_relay_dec_when_above_setpoint);
    RUN_TEST(test_control_relay_off_within_deadband);
    RUN_TEST(test_control_no_action_in_manual_mode);
    RUN_TEST(test_control_alarm_triggers_on_mag_loss);
    RUN_TEST(test_control_alarm_triggers_on_rpm_too_high);
    RUN_TEST(test_control_alarm_triggers_on_rpm_too_low);
    RUN_TEST(test_control_fullcoarse_drives_dec_relay);
    RUN_TEST(test_control_full_duty_outside_prop_zone);
    RUN_TEST(test_control_proportional_duty_in_prop_zone);

    return UNITY_END();
}
