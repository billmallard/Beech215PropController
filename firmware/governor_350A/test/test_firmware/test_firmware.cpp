/**
 * Unit tests for governor_350A firmware logic (firmware v3.0, panel pot).
 *
 * Strategy: mock all Arduino hardware APIs, then #include the .ino directly
 * so the firmware is compiled into this native translation unit.
 * No hardware required — runs under PlatformIO's native/Unity test runner.
 *
 * Functions under test:
 *   magPulseISR()      — ringing reject, dropout restart
 *   calculateRPM()     — pulse-interval to RPM conversion + rolling average
 *   magSignalLost()    — magneto timeout detection
 *   checkMagSignal()   — relays off + averaging reset when the signal is lost
 *   potAdcToRPM()      — pot divider reading to commanded RPM
 *   updatePot()        — filtering, hysteresis, open-circuit detect/recover
 *   updateRamp()       — setpoint ramping toward target
 *   runControlLoop()   — standbys, relay direction/duty decisions
 *   setup() / loop()   — boot into AUTO, end-to-end standby and resume
 */

// Pull in mock headers before the firmware so all Arduino symbols resolve.
#include "Arduino.h"
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "avr/wdt.h"

// Include the firmware itself as a C++ translation unit.
#include "../../governor_350A.ino"

#include <unity.h>

// Interval between magneto pulses for a given RPM (3 pulses per rev).
#define US_PER_PULSE(rpm) ((uint32_t)(60000000.0 / ((rpm) * PULSES_PER_REV)))

// ADC count the pot divider produces for a given pot resistance in ohms.
static uint16_t adcForOhms(float ohms) {
    return (uint16_t)(1023.0f * ohms / (ohms + 10000.0f) + 0.5f);
}

// =============================================================================
// Test fixture — reset all firmware global state before each test
// =============================================================================

void setUp(void) {
    mock_millis              = 0;
    mock_micros              = 0;
    memset(mock_pin, LOW, sizeof(mock_pin));
    memset(mock_adc, 0, sizeof(mock_adc));
    mock_adc_reads           = 0;
    mock_wire_ack            = false;
    mock_wire_transmissions  = 0;

    currentMode              = MODE_MANUAL;
    relayState               = RELAY_OFF;
    holdReason               = HOLD_NONE;

    rampedSetpoint           = 2300.0f;
    targetSetpoint           = 2300.0f;
    lastRampTime             = 0;
    reseedRamp               = false;

    potFilt16                = 0;
    potSeeded                = false;
    potOpen                  = false;
    potEdge_ms               = 0;
    lastPotRead              = 0;

    lastPulseTime_us         = 0;
    pulseInterval_us         = 0;
    newPulse                 = false;

    memset(rpmBuffer, 0, sizeof(rpmBuffer));
    rpmBufIdx                = 0;
    rpmBufFull               = false;

    actualRPM                = 0.0f;
    lastMagPulse_ms          = 0;

    lastControlRun           = 0;
    lastDisplayUpdate        = 0;
    displayOK                = false;
}

void tearDown(void) {}

// Advance mock time and deliver n magneto pulses through the real ISR and
// updateRPM(), exactly as the hardware interrupt + main loop would.
static void feedPulses(int n, uint32_t interval_us) {
    for (int i = 0; i < n; i++) {
        mock_micros += interval_us;
        mock_millis  = mock_micros / 1000;
        magPulseISR();
        if (newPulse) updateRPM();
    }
}

// Advance mock time with no pulses at all.
static void advanceMs(uint32_t ms) {
    mock_micros += ms * 1000UL;
    mock_millis  = mock_micros / 1000;
}


// =============================================================================
// magPulseISR() tests
// =============================================================================

void test_isr_first_pulse_reports_no_interval(void) {
    mock_micros = 50000;
    magPulseISR();
    TEST_ASSERT_FALSE(newPulse);
    TEST_ASSERT_EQUAL_UINT32(50000, lastPulseTime_us);
}

void test_isr_reports_interval_between_pulses(void) {
    mock_micros = 50000;  magPulseISR();
    mock_micros = 58696;  magPulseISR();
    TEST_ASSERT_TRUE(newPulse);
    TEST_ASSERT_EQUAL_UINT32(8696, pulseInterval_us);
}

void test_isr_rejects_ringing_edges(void) {
    mock_micros = 50000;  magPulseISR();           // real firing
    mock_micros = 50300;  magPulseISR();           // ringing, 0.3 ms later
    mock_micros = 51100;  magPulseISR();           // ringing, 1.1 ms later
    mock_micros = 51999;  magPulseISR();           // just inside the 2 ms blanking
    TEST_ASSERT_FALSE(newPulse);
    TEST_ASSERT_EQUAL_UINT32(50000, lastPulseTime_us);   // reference did not move

    mock_micros = 58696;  magPulseISR();           // next real firing
    TEST_ASSERT_TRUE(newPulse);
    TEST_ASSERT_EQUAL_UINT32(8696, pulseInterval_us);    // measured from the real edge
}

void test_isr_accepts_pulse_at_max_legitimate_rate(void) {
    // 3500 RPM x 3 pulses/rev -> 5714 us. Must never be rejected as ringing.
    TEST_ASSERT_TRUE(MIN_PULSE_US < US_PER_PULSE(3500));
    mock_micros = 50000;                       magPulseISR();
    mock_micros = 50000 + US_PER_PULSE(3500);  magPulseISR();
    TEST_ASSERT_TRUE(newPulse);
}

void test_isr_gap_restarts_timing_without_interval(void) {
    mock_micros = 50000;    magPulseISR();
    mock_micros = 1550000;  magPulseISR();     // 1.5 s dropout
    TEST_ASSERT_FALSE(newPulse);               // the gap is not an RPM reading
    TEST_ASSERT_EQUAL_UINT32(1550000, lastPulseTime_us);

    mock_micros = 1558696;  magPulseISR();
    TEST_ASSERT_TRUE(newPulse);
    TEST_ASSERT_EQUAL_UINT32(8696, pulseInterval_us);
}


// =============================================================================
// calculateRPM() tests
// =============================================================================

void test_rpm_zero_when_no_pulse_interval(void) {
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, calculateRPM(0));
}

void test_rpm_zero_when_mag_signal_lost(void) {
    // 8696 us interval would be ~2300 RPM with 3 PPR, but signal is stale.
    mock_millis      = 5000;
    lastMagPulse_ms  = 0;  // 5 s ago — well past 2 s timeout
    TEST_ASSERT_EQUAL_FLOAT(0.0f, calculateRPM(8696));
}

void test_rpm_correct_for_2300rpm_3ppr(void) {
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 2300.0f, calculateRPM(8696));
}

void test_rpm_correct_for_2650rpm_3ppr(void) {
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 2650.0f, calculateRPM(7547));
}

void test_rpm_correct_for_1800rpm_3ppr(void) {
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 1800.0f, calculateRPM(11111));
}

void test_rpm_rolling_average_smooths_spikes(void) {
    mock_millis     = 1000;
    lastMagPulse_ms = 999;
    for (uint8_t i = 0; i < RPM_AVG_SAMPLES; i++) calculateRPM(8696);
    // One spike (4000 RPM equivalent) among 8 samples must be dampened.
    float rpm = calculateRPM(5000);
    TEST_ASSERT_TRUE(rpm < 3000.0f);
    TEST_ASSERT_TRUE(rpm > 2000.0f);
}


// =============================================================================
// magSignalLost() / checkMagSignal() tests
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

void test_check_mag_leaves_things_alone_while_signal_fresh(void) {
    currentMode     = MODE_AUTO;
    mock_millis     = 1000;
    lastMagPulse_ms = 999;
    rpmBufFull      = true;
    actualRPM       = 2300.0f;
    relayState      = RELAY_INC;
    mock_pin[PIN_INC_RELAY] = HIGH;

    checkMagSignal();

    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
    TEST_ASSERT_EQUAL(HIGH, mock_pin[PIN_INC_RELAY]);
    TEST_ASSERT_TRUE(rpmBufFull);
}

void test_check_mag_drops_relays_within_timeout_in_any_mode(void) {
    ControlMode modes[2] = { MODE_AUTO, MODE_MANUAL };
    for (int m = 0; m < 2; m++) {
        currentMode     = modes[m];
        lastMagPulse_ms = 0;
        mock_millis     = MAG_TIMEOUT_MS + 1;
        rpmBufFull      = true;
        rpmBufIdx       = 5;
        actualRPM       = 2300.0f;
        relayState      = RELAY_DEC;
        mock_pin[PIN_DEC_RELAY] = HIGH;
        mock_pin[PIN_DEC_LED]   = HIGH;

        checkMagSignal();

        TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
        TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_INC_RELAY]);
        TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_DEC_RELAY]);
        TEST_ASSERT_EQUAL(modes[m], currentMode);     // standby never changes mode
        TEST_ASSERT_FALSE(rpmBufFull);                // averaging must refill
        TEST_ASSERT_EQUAL_FLOAT(0.0f, actualRPM);
        TEST_ASSERT_TRUE(reseedRamp);
    }
}


// =============================================================================
// potAdcToRPM() tests
// =============================================================================

void test_pot_full_cw_is_max_rpm(void) {
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MAX, potAdcToRPM(0.0f));
}

void test_pot_full_ccw_is_min_rpm(void) {
    // 10k pot against the 10k pull-up -> half scale.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, (float)POT_RPM_MIN, potAdcToRPM(511.5f));
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, potAdcToRPM((float)adcForOhms(10000)));
}

void test_pot_midpoint_is_mid_rpm(void) {
    // 5k (half rotation of a linear pot) -> adc 341 -> exactly half way.
    float mid = (POT_RPM_MAX + POT_RPM_MIN) / 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(2.0f, mid, potAdcToRPM((float)adcForOhms(5000)));
}

void test_pot_is_linear_in_resistance(void) {
    // Quarter and three-quarter rotation land on quarter / three-quarter RPM.
    float span = (float)(POT_RPM_MAX - POT_RPM_MIN);
    TEST_ASSERT_FLOAT_WITHIN(4.0f, POT_RPM_MAX - 0.25f * span, potAdcToRPM((float)adcForOhms(2500)));
    TEST_ASSERT_FLOAT_WITHIN(4.0f, POT_RPM_MAX - 0.75f * span, potAdcToRPM((float)adcForOhms(7500)));
}

void test_pot_clamps_beyond_both_ends(void) {
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MAX, potAdcToRPM(-5.0f));
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, potAdcToRPM((float)adcForOhms(12000))); // worn pot
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, potAdcToRPM(900.0f));
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, potAdcToRPM(1023.0f));
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, potAdcToRPM(2000.0f));
}

void test_pot_mapping_is_monotonic_and_in_range(void) {
    float prev = potAdcToRPM(0.0f);
    for (int adc = 1; adc <= 1023; adc++) {
        float rpm = potAdcToRPM((float)adc);
        TEST_ASSERT_TRUE(rpm <= prev);                       // CW -> CCW never raises RPM
        TEST_ASSERT_TRUE(rpm >= (float)POT_RPM_MIN);
        TEST_ASSERT_TRUE(rpm <= (float)POT_RPM_MAX);
        prev = rpm;
    }
}

void test_pot_open_threshold_sits_between_worn_pot_and_open_wire(void) {
    TEST_ASSERT_TRUE(adcForOhms(12000) < POT_OPEN_ADC);   // worn 10k pot is still valid
    TEST_ASSERT_TRUE(adcForOhms(20000) >= POT_OPEN_ADC);  // clearly open
    TEST_ASSERT_TRUE(POT_OPEN_ADC < 1000);                // open wire (~1023) trips with margin
}


// =============================================================================
// updatePot() tests
// =============================================================================

// Feed the same reading for a while, one call every POT_INTERVAL_MS.
static void holdPot(uint16_t adc, uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += POT_INTERVAL_MS) {
        mock_millis += POT_INTERVAL_MS;
        updatePot(adc);
    }
}

void test_pot_first_reading_seeds_target(void) {
    targetSetpoint = 1234.0f;
    updatePot(adcForOhms(5000));
    TEST_ASSERT_TRUE(potSeeded);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, (POT_RPM_MAX + POT_RPM_MIN) / 2.0f, targetSetpoint);
}

void test_pot_target_follows_knob(void) {
    updatePot(adcForOhms(5000));
    holdPot(adcForOhms(1000), 1000);     // turn the knob toward CW
    float span = (float)(POT_RPM_MAX - POT_RPM_MIN);
    TEST_ASSERT_FLOAT_WITHIN(POT_HYST_RPM + 2.0f, POT_RPM_MAX - 0.1f * span, targetSetpoint);
}

void test_pot_reaches_both_end_stops_exactly(void) {
    updatePot(adcForOhms(5000));
    holdPot(0, 1000);
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MAX, targetSetpoint);
    holdPot(adcForOhms(10000), 1000);
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, targetSetpoint);
}

void test_pot_stationary_knob_does_not_jitter_target(void) {
    // Worst case for ADC noise is near the CCW end (largest RPM per count).
    uint16_t centre[3] = { adcForOhms(1000), adcForOhms(5000), adcForOhms(9000) };
    for (int c = 0; c < 3; c++) {
        potSeeded = false;
        updatePot(centre[c]);
        holdPot(centre[c], 500);
        float settled = targetSetpoint;
        for (int i = 0; i < 500; i++) {          // +/-1 count of noise for 10 s
            mock_millis += POT_INTERVAL_MS;
            updatePot(centre[c] + ((i % 3) - 1));
            TEST_ASSERT_EQUAL_FLOAT(settled, targetSetpoint);
        }
    }
}

void test_pot_open_wire_trips_only_after_persisting(void) {
    updatePot(adcForOhms(5000));
    float before = targetSetpoint;

    holdPot(1023, POT_OPEN_MS - POT_INTERVAL_MS);     // not long enough yet
    TEST_ASSERT_FALSE(potOpen);
    TEST_ASSERT_EQUAL_FLOAT(before, targetSetpoint);  // target never follows an open reading

    holdPot(1023, 2 * POT_INTERVAL_MS);
    TEST_ASSERT_TRUE(potOpen);
    TEST_ASSERT_EQUAL_FLOAT(before, targetSetpoint);
}

void test_pot_brief_wiper_lift_is_ignored(void) {
    updatePot(adcForOhms(5000));
    float before = targetSetpoint;
    for (int i = 0; i < 20; i++) {
        holdPot(1023, 100);                           // 100 ms open
        holdPot(adcForOhms(5000), 100);               // 100 ms good
        TEST_ASSERT_FALSE(potOpen);
        TEST_ASSERT_EQUAL_FLOAT(before, targetSetpoint);
    }
}

void test_pot_open_recovers_when_reading_valid_again(void) {
    updatePot(adcForOhms(5000));
    holdPot(1023, 1000);
    TEST_ASSERT_TRUE(potOpen);

    holdPot(adcForOhms(2500), POT_OPEN_MS - POT_INTERVAL_MS);
    TEST_ASSERT_TRUE(potOpen);                        // recovery is debounced too
    holdPot(adcForOhms(2500), 500);
    TEST_ASSERT_FALSE(potOpen);
    float span = (float)(POT_RPM_MAX - POT_RPM_MIN);
    TEST_ASSERT_FLOAT_WITHIN(POT_HYST_RPM + 4.0f, POT_RPM_MAX - 0.25f * span, targetSetpoint);
}

void test_pot_worn_12k_pot_is_min_rpm_not_a_fault(void) {
    updatePot(adcForOhms(5000));
    holdPot(adcForOhms(12000), 2000);
    TEST_ASSERT_FALSE(potOpen);
    TEST_ASSERT_EQUAL_FLOAT((float)POT_RPM_MIN, targetSetpoint);
}

void test_pot_read_averages_the_pot_pin(void) {
    mock_adc[PIN_POT - A0] = 400;
    TEST_ASSERT_EQUAL_UINT16(400, readPotAdc());
    TEST_ASSERT_EQUAL_UINT32(POT_SAMPLES, mock_adc_reads);
}


// =============================================================================
// updateRamp() tests
// =============================================================================

void test_ramp_moves_up_toward_higher_target(void) {
    rampedSetpoint = 2200.0f;
    targetSetpoint = 2300.0f;
    lastRampTime   = 0;
    mock_millis    = 500;        // 0.5 s → max step = 50 RPM
    updateRamp();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2250.0f, rampedSetpoint);
}

void test_ramp_moves_down_toward_lower_target(void) {
    rampedSetpoint = 2300.0f;
    targetSetpoint = 2200.0f;
    lastRampTime   = 0;
    mock_millis    = 500;        // 0.5 s → max step = 50 RPM
    updateRamp();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2250.0f, rampedSetpoint);
}

void test_ramp_snaps_when_within_step(void) {
    rampedSetpoint = 2295.0f;
    targetSetpoint = 2300.0f;   // only 5 RPM away
    lastRampTime   = 0;
    mock_millis    = 1000;      // 1 s → max step = 100 RPM > 5 RPM diff
    updateRamp();
    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
}

void test_ramp_does_not_overshoot(void) {
    rampedSetpoint = 2299.0f;
    targetSetpoint = 2300.0f;
    lastRampTime   = 0;
    mock_millis    = 5000;      // huge elapsed time — must snap, not overshoot
    updateRamp();
    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
}

void test_ramp_full_knob_sweep_is_rate_limited(void) {
    // Knob slammed from CCW to CW: after 1 s the setpoint has moved 100 RPM.
    rampedSetpoint = (float)POT_RPM_MIN;
    targetSetpoint = (float)POT_RPM_MAX;
    lastRampTime   = 0;
    for (mock_millis = 10; mock_millis <= 1000; mock_millis += 10) updateRamp();
    TEST_ASSERT_FLOAT_WITHIN(2.0f, POT_RPM_MIN + 100.0f, rampedSetpoint);
}


// =============================================================================
// runControlLoop() tests
// =============================================================================

// Helper: an already-governing AUTO state (valid RPM, pot good, ramp seeded).
static void enterAutoMode(float setpoint, float actual) {
    currentMode      = MODE_AUTO;
    mock_millis      = 1000;
    lastMagPulse_ms  = 999;    // mag signal fresh
    rpmBufFull       = true;   // averaging buffer filled
    potOpen          = false;
    reseedRamp       = false;
    holdReason       = HOLD_NONE;
    rampedSetpoint   = setpoint;
    targetSetpoint   = setpoint;
    actualRPM        = actual;
    relayState       = RELAY_OFF;
}

void test_control_relay_inc_when_below_setpoint(void) {
    enterAutoMode(2300.0f, 2100.0f);
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
}

void test_control_relay_dec_when_above_setpoint(void) {
    enterAutoMode(2300.0f, 2500.0f);
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
    currentMode = MODE_MANUAL;
    relayState  = RELAY_INC;
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_TRUE(reseedRamp);     // returning to AUTO must not step
}

void test_control_mag_loss_is_standby_not_alarm(void) {
    enterAutoMode(2300.0f, 2100.0f);
    relayState = RELAY_INC;
    mock_pin[PIN_INC_RELAY] = HIGH;
    mock_millis     = 5000;
    lastMagPulse_ms = 0;              // signal lost 5 s ago
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);       // not latched
    TEST_ASSERT_EQUAL(HOLD_NO_MAG, holdReason);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_INC_RELAY]);
}

void test_control_no_drive_until_rpm_average_refilled(void) {
    enterAutoMode(2300.0f, 2100.0f);
    rpmBufFull = false;               // only a few pulses since the signal came back
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_EQUAL(HOLD_NO_MAG, holdReason);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
}

void test_control_pot_open_holds_relays_off(void) {
    enterAutoMode(2300.0f, 2100.0f);
    relayState = RELAY_INC;
    mock_pin[PIN_INC_RELAY] = HIGH;
    potOpen = true;
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);       // not latched
    TEST_ASSERT_EQUAL(HOLD_POT_OPEN, holdReason);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_INC_RELAY]);
}

void test_control_pot_open_recovery_resumes_without_step(void) {
    enterAutoMode(2300.0f, 2100.0f);
    potOpen = true;
    runControlLoop();                 // standby
    potOpen = false;                  // pot is back; knob still asks for 2300
    runControlLoop();                 // first governing pass
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
    TEST_ASSERT_EQUAL_FLOAT(2100.0f, rampedSetpoint);    // ramp restarts at actual RPM
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);            // so nothing lurches

    mock_millis += 1000;              // ramp walks toward the knob at 100 RPM/s
    updateRamp();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 2200.0f, rampedSetpoint);
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
}

void test_control_first_governing_pass_seeds_ramp_at_actual_rpm(void) {
    enterAutoMode(2650.0f, 1900.0f);  // knob at take-off, prop turning 1900
    reseedRamp = true;                // e.g. just powered up
    runControlLoop();
    TEST_ASSERT_FALSE(reseedRamp);
    TEST_ASSERT_EQUAL_FLOAT(1900.0f, rampedSetpoint);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
}

void test_control_no_upper_rpm_limit_keeps_commanding_decrease(void) {
    // Like the original: however high the reading, the answer is DEC. A real
    // overspeed is exactly when the governor must not go quiet.
    float readings[3] = { 3501.0f, 4000.0f, 6000.0f };
    for (int i = 0; i < 3; i++) {
        enterAutoMode(2300.0f, readings[i]);
        runControlLoop();
        TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
        TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
        TEST_ASSERT_EQUAL(RELAY_DEC, relayState);
    }
}

void test_control_rpm_below_minimum_is_standby(void) {
    enterAutoMode(2300.0f, 200.0f);   // < RPM_MIN_VALID (non-zero)
    relayState = RELAY_INC;
    mock_pin[PIN_INC_RELAY] = HIGH;
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_EQUAL(HOLD_RPM_LOW, holdReason);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_INC_RELAY]);
}

void test_control_rpm_minimum_itself_is_governed(void) {
    enterAutoMode(2300.0f, (float)RPM_MIN_VALID - 1.0f);
    runControlLoop();
    TEST_ASSERT_EQUAL(HOLD_RPM_LOW, holdReason);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    enterAutoMode(2300.0f, (float)RPM_MIN_VALID);
    runControlLoop();
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
}

void test_control_low_rpm_hold_resumes_by_itself_against_the_knob(void) {
    // Engine failure in flight: the reading sits below the minimum for 5 s.
    // No drive, nothing latched, and the ramp is not restarted.
    enterAutoMode(2300.0f, 300.0f);
    for (int i = 0; i < 50; i++) {
        mock_millis += 100; lastMagPulse_ms = mock_millis;
        runControlLoop();
        TEST_ASSERT_EQUAL(HOLD_RPM_LOW, holdReason);
        TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    }
    TEST_ASSERT_FALSE(reseedRamp);
    // Restart: as the RPM comes up through the minimum the very next pass
    // commands INC -- toward fine pitch -- with the ramp still at the knob.
    actualRPM = 450.0f;
    mock_millis += 100; lastMagPulse_ms = mock_millis;
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
}

void test_control_relay_drive_has_no_time_limit(void) {
    // Like the original 350A: the output is held until the selected RPM is
    // reached, however long that takes. With the prop on a pitch stop the
    // aircraft's limit switch stops the motor; the governor must not give up
    // and must still be governing when the RPM finally crosses the setpoint.
    enterAutoMode(2300.0f, 2100.0f);
    mock_millis = 1000;
    for (int i = 0; i < 600; i++) {            // 60 seconds of INC demand
        lastMagPulse_ms = mock_millis;
        runControlLoop();
        TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
        TEST_ASSERT_EQUAL(RELAY_INC, relayState);
        mock_millis += CONTROL_INTERVAL_MS;
    }
    actualRPM = 2500.0f;                       // e.g. power back on: now too fast
    lastMagPulse_ms = mock_millis;
    runControlLoop();
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_EQUAL(RELAY_DEC, relayState);
}

void test_control_low_rpm_hold_keeps_a_pending_ramp_restart(void) {
    // Engine start with the switch already in AUTO: the first valid readings
    // are below the range. The restart of the ramp that the no-signal standby
    // asked for must survive the low-RPM hold, so governing still begins from
    // the actual RPM once the engine is running.
    enterAutoMode(2300.0f, 300.0f);
    reseedRamp = true;
    runControlLoop();
    TEST_ASSERT_EQUAL(HOLD_RPM_LOW, holdReason);
    TEST_ASSERT_TRUE(reseedRamp);
    actualRPM = 700.0f;
    mock_millis += 100; lastMagPulse_ms = mock_millis;
    runControlLoop();
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
    TEST_ASSERT_FALSE(reseedRamp);
    TEST_ASSERT_EQUAL_FLOAT(700.0f, rampedSetpoint);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
}

// ---- The ramp never makes the governor drive away from the knob setting ----

// One 100 ms control tick with the ramp running, as loop() does it.
static void rampTick(float rpmStep) {
    mock_millis += CONTROL_INTERVAL_MS;
    lastMagPulse_ms = mock_millis;
    actualRPM += rpmStep;
    updateRamp();
    runControlLoop();
}

void test_control_ramp_never_commands_decrease_while_rpm_rises_to_the_knob(void) {
    // Engine start / restart with the knob at take-off RPM. The ramp starts at
    // the actual RPM and climbs 100 RPM/s; the engine accelerates at 300 RPM/s.
    // The governor must not drive toward coarse pitch to hold the RPM back.
    enterAutoMode(2650.0f, 500.0f);
    reseedRamp   = true;
    lastRampTime = mock_millis;
    for (int i = 0; i < 40; i++) {                 // 4 s, 500 -> 1700 RPM
        rampTick(30.0f);
        TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
        TEST_ASSERT_EQUAL_FLOAT(actualRPM, rampedSetpoint);   // ramp rides the RPM
    }
    // RPM stops short of the knob setting: the ramp walks on and INC follows.
    bool inc = false;
    for (int i = 0; i < 10; i++) {
        rampTick(0.0f);
        TEST_ASSERT_TRUE(relayState != RELAY_DEC);
        if (relayState == RELAY_INC) inc = true;
    }
    TEST_ASSERT_TRUE(inc);
}

void test_control_ramp_never_commands_increase_while_rpm_falls_to_the_knob(void) {
    enterAutoMode(1800.0f, 2650.0f);
    reseedRamp   = true;
    lastRampTime = mock_millis;
    for (int i = 0; i < 20; i++) {                 // 2 s, 2650 -> 2050 RPM
        rampTick(-30.0f);
        TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
        TEST_ASSERT_EQUAL_FLOAT(actualRPM, rampedSetpoint);
    }
    bool dec = false;
    for (int i = 0; i < 10; i++) {
        rampTick(0.0f);
        TEST_ASSERT_TRUE(relayState != RELAY_INC);
        if (relayState == RELAY_DEC) dec = true;
    }
    TEST_ASSERT_TRUE(dec);
}

void test_control_rpm_beyond_the_knob_is_governed_against_the_knob(void) {
    // Ramp still climbing toward 2300, RPM already above it: govern against
    // the knob setting, not against the lagging ramp.
    enterAutoMode(2300.0f, 2400.0f);
    rampedSetpoint = 2000.0f;
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_DEC, relayState);
    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
    // Mirror image: ramp still coming down, RPM already below the knob.
    enterAutoMode(2300.0f, 2200.0f);
    rampedSetpoint = 2600.0f;
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
    TEST_ASSERT_EQUAL_FLOAT(2300.0f, rampedSetpoint);
}

void test_control_ramp_between_rpm_and_knob_is_left_alone(void) {
    // The ordinary case (a knob movement): the ramp lies between the RPM and
    // the knob setting and must be tracked exactly as before.
    enterAutoMode(2600.0f, 2300.0f);
    rampedSetpoint = 2400.0f;
    runControlLoop();
    TEST_ASSERT_EQUAL_FLOAT(2400.0f, rampedSetpoint);
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
    enterAutoMode(2000.0f, 2300.0f);
    rampedSetpoint = 2200.0f;
    runControlLoop();
    TEST_ASSERT_EQUAL_FLOAT(2200.0f, rampedSetpoint);
    TEST_ASSERT_EQUAL(RELAY_DEC, relayState);
}

void test_control_full_duty_outside_prop_zone(void) {
    // Error >= PROP_ZONE_RPM (75) → duty must be 1.0 → relay on the whole window.
    enterAutoMode(2300.0f, 2150.0f);  // -150 RPM error
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
    TEST_ASSERT_EQUAL(HIGH, mock_pin[PIN_INC_RELAY]);
    TEST_ASSERT_EQUAL(LOW,  mock_pin[PIN_DEC_RELAY]);
}

void test_control_proportional_duty_in_prop_zone(void) {
    // Error of 50 RPM is inside PROP_ZONE_RPM (75) but outside DEADBAND (25).
    enterAutoMode(2300.0f, 2250.0f);  // -50 RPM error
    runControlLoop();
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);
}


// =============================================================================
// setup() / loop() — boot behaviour and end-to-end standby / resume
// =============================================================================

// Run the real main loop for a while. pulse_us = 0 means no magneto signal.
// sawInc / sawDec record whether either relay output was ever driven HIGH.
// The pulse train keeps its phase across consecutive calls, so splitting a
// run into two calls does not drop a pulse.
static bool sawInc, sawDec;
static uint32_t nextPulse = 0;
static void runLoopFor(uint32_t ms, uint32_t pulse_us) {
    uint32_t end = mock_micros + ms * 1000UL;
    if (pulse_us == 0)       nextPulse = 0;
    else if (nextPulse == 0) nextPulse = mock_micros + pulse_us;
    while (mock_micros < end) {
        mock_micros += 500;                       // loop() pass every 0.5 ms
        mock_millis  = mock_micros / 1000;
        if (pulse_us && mock_micros >= nextPulse) { magPulseISR(); nextPulse += pulse_us; }
        loop();
        if (mock_pin[PIN_INC_RELAY] == HIGH) sawInc = true;
        if (mock_pin[PIN_DEC_RELAY] == HIGH) sawDec = true;
    }
}

static void boot(uint16_t potAdc) {
    mock_micros = 3000000;                        // some time after power-on reset
    mock_millis = 3000;
    mock_adc[PIN_POT - A0] = potAdc;
    sawInc = sawDec = false;
    nextPulse = 0;
    setup();
}

void test_boot_goes_straight_to_auto_with_relays_off(void) {
    boot(adcForOhms(5000));
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_EQUAL(HOLD_NO_MAG, holdReason);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_INC_RELAY]);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_DEC_RELAY]);
    TEST_ASSERT_FALSE(potOpen);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, (POT_RPM_MAX + POT_RPM_MIN) / 2.0f, targetSetpoint);
}

void test_boot_with_open_pot_starts_in_pot_open(void) {
    boot(1023);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_TRUE(potOpen);
    runLoopFor(3000, US_PER_PULSE(2300));         // engine running, pot wire broken
    TEST_ASSERT_EQUAL(HOLD_POT_OPEN, holdReason);
    TEST_ASSERT_FALSE(sawInc);
    TEST_ASSERT_FALSE(sawDec);
}

void test_boot_without_display_generates_no_i2c_traffic(void) {
    mock_wire_ack = false;                        // nothing on the service header
    boot(adcForOhms(5000));
    TEST_ASSERT_FALSE(displayOK);
    uint32_t probes = mock_wire_transmissions;
    TEST_ASSERT_EQUAL_UINT32(1, probes);          // the single presence probe
    runLoopFor(2000, US_PER_PULSE(2300));
    TEST_ASSERT_EQUAL_UINT32(probes, mock_wire_transmissions);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
}

void test_boot_with_display_enables_it(void) {
    mock_wire_ack = true;
    boot(adcForOhms(5000));
    TEST_ASSERT_TRUE(displayOK);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
}

void test_loop_no_mag_signal_never_drives_and_never_alarms(void) {
    boot(0);                                      // knob at take-off RPM
    runLoopFor(10000, 0);                         // 10 s, engine not running
    TEST_ASSERT_FALSE(sawInc);
    TEST_ASSERT_FALSE(sawDec);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_EQUAL(HOLD_NO_MAG, holdReason);
}

void test_loop_governs_by_itself_once_signal_is_valid(void) {
    boot(0);                                      // knob full CW: 2650
    runLoopFor(60, US_PER_PULSE(2300));           // a handful of pulses: not yet trusted
    TEST_ASSERT_FALSE(sawInc);
    TEST_ASSERT_FALSE(sawDec);

    runLoopFor(300, US_PER_PULSE(2300));          // RPM now valid; ramp starts at 2300
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 2300.0f, rampedSetpoint);
    TEST_ASSERT_FALSE(sawDec);

    runLoopFor(2000, US_PER_PULSE(2300));         // setpoint has ramped ~200 RPM above actual
    TEST_ASSERT_TRUE(sawInc);                     // increase RPM, no serial command needed
    TEST_ASSERT_FALSE(sawDec);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
}

void test_loop_mag_dropout_goes_to_standby_then_resumes(void) {
    boot(0);
    runLoopFor(3000, US_PER_PULSE(2300));
    TEST_ASSERT_EQUAL(RELAY_INC, relayState);     // governing: asking for more RPM

    // Signal disappears. Relays must be off within MAG_TIMEOUT_MS (+1 loop pass).
    runLoopFor(MAG_TIMEOUT_MS + 5, 0);
    TEST_ASSERT_EQUAL(RELAY_OFF, relayState);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_INC_RELAY]);
    TEST_ASSERT_EQUAL(LOW, mock_pin[PIN_DEC_RELAY]);
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    sawInc = sawDec = false;
    runLoopFor(5000, 0);
    TEST_ASSERT_FALSE(sawInc);
    TEST_ASSERT_FALSE(sawDec);
    TEST_ASSERT_EQUAL(HOLD_NO_MAG, holdReason);

    // Signal returns at a different RPM. The dropout itself must not be read
    // as a low RPM, and the ramp restarts from the new actual RPM.
    runLoopFor(400, US_PER_PULSE(2000));
    TEST_ASSERT_EQUAL(MODE_AUTO, currentMode);
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 2000.0f, actualRPM);
    TEST_ASSERT_FLOAT_WITHIN(60.0f, 2000.0f, rampedSetpoint);
    runLoopFor(2000, US_PER_PULSE(2000));
    TEST_ASSERT_TRUE(sawInc);
}

void test_loop_ringing_on_every_pulse_does_not_corrupt_rpm(void) {
    boot(adcForOhms(5000));
    // Each firing is followed by two ringing edges 0.4 ms and 0.9 ms later.
    uint32_t period = US_PER_PULSE(2225);
    for (int i = 0; i < 300; i++) {
        mock_micros += period - 900; mock_millis = mock_micros / 1000; magPulseISR(); loop();
        mock_micros += 400;          mock_millis = mock_micros / 1000; magPulseISR(); loop();
        mock_micros += 500;          mock_millis = mock_micros / 1000; magPulseISR(); loop();
    }
    TEST_ASSERT_EQUAL(HOLD_NONE, holdReason);             // never read as a low RPM
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 2225.0f, actualRPM);
}


// =============================================================================
// Entry point
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    // magPulseISR
    RUN_TEST(test_isr_first_pulse_reports_no_interval);
    RUN_TEST(test_isr_reports_interval_between_pulses);
    RUN_TEST(test_isr_rejects_ringing_edges);
    RUN_TEST(test_isr_accepts_pulse_at_max_legitimate_rate);
    RUN_TEST(test_isr_gap_restarts_timing_without_interval);

    // calculateRPM
    RUN_TEST(test_rpm_zero_when_no_pulse_interval);
    RUN_TEST(test_rpm_zero_when_mag_signal_lost);
    RUN_TEST(test_rpm_correct_for_2300rpm_3ppr);
    RUN_TEST(test_rpm_correct_for_2650rpm_3ppr);
    RUN_TEST(test_rpm_correct_for_1800rpm_3ppr);
    RUN_TEST(test_rpm_rolling_average_smooths_spikes);

    // magSignalLost / checkMagSignal
    RUN_TEST(test_mag_not_lost_when_fresh);
    RUN_TEST(test_mag_not_lost_just_before_timeout);
    RUN_TEST(test_mag_lost_after_timeout);
    RUN_TEST(test_check_mag_leaves_things_alone_while_signal_fresh);
    RUN_TEST(test_check_mag_drops_relays_within_timeout_in_any_mode);

    // potAdcToRPM
    RUN_TEST(test_pot_full_cw_is_max_rpm);
    RUN_TEST(test_pot_full_ccw_is_min_rpm);
    RUN_TEST(test_pot_midpoint_is_mid_rpm);
    RUN_TEST(test_pot_is_linear_in_resistance);
    RUN_TEST(test_pot_clamps_beyond_both_ends);
    RUN_TEST(test_pot_mapping_is_monotonic_and_in_range);
    RUN_TEST(test_pot_open_threshold_sits_between_worn_pot_and_open_wire);

    // updatePot
    RUN_TEST(test_pot_first_reading_seeds_target);
    RUN_TEST(test_pot_target_follows_knob);
    RUN_TEST(test_pot_reaches_both_end_stops_exactly);
    RUN_TEST(test_pot_stationary_knob_does_not_jitter_target);
    RUN_TEST(test_pot_open_wire_trips_only_after_persisting);
    RUN_TEST(test_pot_brief_wiper_lift_is_ignored);
    RUN_TEST(test_pot_open_recovers_when_reading_valid_again);
    RUN_TEST(test_pot_worn_12k_pot_is_min_rpm_not_a_fault);
    RUN_TEST(test_pot_read_averages_the_pot_pin);

    // updateRamp
    RUN_TEST(test_ramp_moves_up_toward_higher_target);
    RUN_TEST(test_ramp_moves_down_toward_lower_target);
    RUN_TEST(test_ramp_snaps_when_within_step);
    RUN_TEST(test_ramp_does_not_overshoot);
    RUN_TEST(test_ramp_full_knob_sweep_is_rate_limited);

    // runControlLoop
    RUN_TEST(test_control_relay_inc_when_below_setpoint);
    RUN_TEST(test_control_relay_dec_when_above_setpoint);
    RUN_TEST(test_control_relay_off_within_deadband);
    RUN_TEST(test_control_no_action_in_manual_mode);
    RUN_TEST(test_control_mag_loss_is_standby_not_alarm);
    RUN_TEST(test_control_no_drive_until_rpm_average_refilled);
    RUN_TEST(test_control_pot_open_holds_relays_off);
    RUN_TEST(test_control_pot_open_recovery_resumes_without_step);
    RUN_TEST(test_control_first_governing_pass_seeds_ramp_at_actual_rpm);
    RUN_TEST(test_control_no_upper_rpm_limit_keeps_commanding_decrease);
    RUN_TEST(test_control_rpm_below_minimum_is_standby);
    RUN_TEST(test_control_rpm_minimum_itself_is_governed);
    RUN_TEST(test_control_low_rpm_hold_resumes_by_itself_against_the_knob);
    RUN_TEST(test_control_relay_drive_has_no_time_limit);
    RUN_TEST(test_control_low_rpm_hold_keeps_a_pending_ramp_restart);
    RUN_TEST(test_control_ramp_never_commands_decrease_while_rpm_rises_to_the_knob);
    RUN_TEST(test_control_ramp_never_commands_increase_while_rpm_falls_to_the_knob);
    RUN_TEST(test_control_rpm_beyond_the_knob_is_governed_against_the_knob);
    RUN_TEST(test_control_ramp_between_rpm_and_knob_is_left_alone);
    RUN_TEST(test_control_full_duty_outside_prop_zone);
    RUN_TEST(test_control_proportional_duty_in_prop_zone);

    // setup / loop
    RUN_TEST(test_boot_goes_straight_to_auto_with_relays_off);
    RUN_TEST(test_boot_with_open_pot_starts_in_pot_open);
    RUN_TEST(test_boot_without_display_generates_no_i2c_traffic);
    RUN_TEST(test_boot_with_display_enables_it);
    RUN_TEST(test_loop_no_mag_signal_never_drives_and_never_alarms);
    RUN_TEST(test_loop_governs_by_itself_once_signal_is_valid);
    RUN_TEST(test_loop_mag_dropout_goes_to_standby_then_resumes);
    RUN_TEST(test_loop_ringing_on_every_pulse_does_not_corrupt_rpm);

    return UNITY_END();
}
