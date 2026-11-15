/**
 * Beechcraft Bonanza Electric Prop — Digital Governor Controller
 * Replacement firmware for Airborne Electronics 350A (STC SA2700WE)
 *
 * Firmware v3.0 — for hardware Rev 3.3 and later
 *
 * Platform:  Arduino Nano (ATmega328P, 5V, 16MHz)
 * Libraries: Adafruit SSD1306, Adafruit GFX (the display itself is optional)
 *
 * Panel interface: the ORIGINAL 350A panel control and nothing else — a 10k
 * linear potentiometer wired as a two-terminal rheostat across connector
 * pins E and F. Full clockwise (near 0 ohm) = take-off RPM, full
 * counter-clockwise (10k) = lowest RPM. No switch, no presets, no panel unit.
 *
 * Behaviour, deliberately matching the original box:
 *   - The aircraft prop switch powers the governor only in AUTO, so the
 *     firmware starts governing by itself after power-up.
 *   - Without a magneto RPM signal it takes no action (relays off) and
 *     resumes on its own when the signal returns.
 *
 * ── CONFIGURATION ───────────────────────────────────────────────────────────
 * Set PULSES_PER_REV, POT_RPM_MAX and POT_RPM_MIN before flashing.
 * All other parameters have safe defaults.
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * MIT License — see repository LICENSE file
 * https://github.com/billmallard/Beech215PropController
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/wdt.h>

// ── CRITICAL: SET BEFORE FLASHING ────────────────────────────────────────────
#define PULSES_PER_REV      3       // 6-cyl Continental (IO-470, IO-520, E-185/E-225)
                                    // Use 2 for 4-cylinder engines

// ── RPM RANGE OF THE PANEL KNOB ───────────────────────────────────────────────
// SET PER AIRCRAFT / ENGINE. These defaults are PLACEHOLDERS pending Bill's
// decision — do not fly them without confirming against the engine's limits.
// The knob is linear between the two ends.
#define POT_RPM_MAX         2650    // Knob full clockwise (pot ~0 ohm): take-off RPM
#define POT_RPM_MIN         1800    // Knob full counter-clockwise (pot 10k): lowest RPM

// ── CONTROL TUNING ────────────────────────────────────────────────────────────
#define DEADBAND_RPM        25      // No correction within +/-25 RPM of setpoint
#define PROP_ZONE_RPM       75      // Proportional slowdown begins within +/-75 RPM
#define MIN_DUTY_PCT        25      // Minimum motor duty in proportional zone (%)
#define CONTROL_INTERVAL_MS 100     // Control loop runs every 100ms

// ── SETPOINT RAMPING ──────────────────────────────────────────────────────────
// The knob sets a TARGET. The setpoint the control loop actually tracks walks
// toward that target at this rate rather than jumping. This prevents a sudden
// large RPM error from commanding full-duty relay drive the moment the knob
// is turned (or the moment the governor is switched on).
//
// 100 RPM/sec: a full-range knob sweep (850 RPM) takes ~8.5 seconds
// Increase for snappier transitions; decrease for more gradual.
#define RAMP_RATE_RPM_PER_SEC  100

// ── SAFETY LIMITS ─────────────────────────────────────────────────────────────
#define MAG_TIMEOUT_MS      2000    // Relays off if no valid magneto pulse for 2 seconds
#define RPM_MIN_VALID       400     // A reading below this is not governed (relays off)
// There is deliberately no upper RPM limit. A high reading is governed like any
// other: the governor commands a decrease, which is what the original does and
// what a real overspeed needs.
#define RPM_AVG_SAMPLES     8       // Rolling average depth for noise rejection

// ── MAGNETO PULSE QUALIFICATION ───────────────────────────────────────────────
// A magneto primary rings when the points open, so one firing can produce
// several edges. Edges closer than MIN_PULSE_US to the last accepted pulse are
// ignored. Must stay well below the shortest real period. Even at 3500 RPM,
// far beyond any redline:
//   3500 RPM x 3 pulses/rev = 175 Hz = 5714 us.   (2 pulses/rev: 8571 us)
#define MIN_PULSE_US        2000UL
// A gap longer than this is not an RPM measurement (signal dropout, engine
// start). The pulse after such a gap only restarts timing; it produces no
// interval. 200 ms = 100 RPM at 3 pulses/rev — far below the 400 RPM floor.
#define MAX_PULSE_US        200000UL

// ── PANEL POTENTIOMETER ───────────────────────────────────────────────────────
// Rev 3.3 hardware: J1-F is ground. J1-E has a 10k 1% pull-up to +5V and feeds
// A7 through 10k + 100nF. The ADC reference is the same +5V, so the reading is
// ratiometric (independent of the exact supply voltage):
//     adc = 1023 x Rpot / (Rpot + 10k)        Rpot = 10k x adc / (1023 - adc)
//     Rpot 0..10k  ->  adc 0..~512.   Open wire -> ~1023.
// A shorted pot or shorted wire reads 0, which cannot be told apart from full
// clockwise and is treated as full clockwise — the same as the original 350A.
#define POT_OPEN_ADC        630     // Above this the pot is open (~16k; a worn 10k pot can read ~12k)
#define POT_OPEN_MS         300     // Open/valid must persist this long before the state changes
#define POT_INTERVAL_MS     20      // Pot is read every 20ms
#define POT_SAMPLES         4       // ADC readings averaged per read (~0.5ms total)
#define POT_HYST_RPM        5       // Target only moves when the knob has moved at least this far

// ── HARDWARE PINS ─────────────────────────────────────────────────────────────
#define PIN_RPM_INPUT   2   // 6N137 output -> INT0 (hardware interrupt)
#define PIN_INC_RELAY   6   // NPN base (via 1k) -> PNP high-side switch -> INC RPM relay (fine pitch)
#define PIN_DEC_RELAY   7   // NPN base (via 1k) -> PNP high-side switch -> DEC RPM relay (coarse pitch)
#define PIN_INC_LED     8   // Red LED — INC activity indicator
#define PIN_DEC_LED     9   // Green LED — DEC activity indicator
#define PIN_POT         A7  // Panel pot sense (analog-only pin)
// Relay output readback — HARDWARE PROVISION ONLY on Rev 3.3 (33k/10k dividers).
// Nothing reads these yet; stuck-output monitoring is a follow-up item.
#define PIN_INC_SENSE   A3  // INC_RELAY output / 4.3
#define PIN_DEC_SENSE   A6  // DEC_RELAY output / 4.3 (analog-only pin)
// A4 = SDA, A5 = SCL — optional OLED on the 4-pin service header (Wire library)
// D3, D4, D5, D10, D11 were the rotary switch on Rev 3.2 and earlier. They are
// unconnected on Rev 3.3 and are left as inputs with pull-ups.

// ── DISPLAY (OPTIONAL) ────────────────────────────────────────────────────────
// The governor runs identically with no display attached. The display is
// probed once at boot; if it does not answer, no I2C traffic is generated.
#define OLED_I2C_ADDR   0x3C    // Change to 0x3D if a fitted display is blank at boot
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// ── SYSTEM STATE ──────────────────────────────────────────────────────────────
enum ControlMode { MODE_MANUAL, MODE_AUTO };
enum RelayState  { RELAY_OFF, RELAY_INC, RELAY_DEC };

// Why the governor is in AUTO but not driving. Nothing here latches: the
// relays are held off and governing resumes by itself when the cause clears.
// The firmware has no latched alarm of any kind, because the aircraft has
// no way to reset one except the prop switch, and the original has none.
enum HoldReason  { HOLD_NONE, HOLD_NO_MAG, HOLD_RPM_LOW, HOLD_POT_OPEN };

ControlMode currentMode  = MODE_MANUAL;   // setup() ends in MODE_AUTO
RelayState  relayState   = RELAY_OFF;
HoldReason  holdReason   = HOLD_NO_MAG;   // Nothing is known about RPM at power-up

// ── SETPOINT RAMP STATE ───────────────────────────────────────────────────────
float    targetSetpoint = (float)POT_RPM_MIN; // Where the knob is pointing
float    rampedSetpoint = (float)POT_RPM_MIN; // What the control loop tracks
uint32_t lastRampTime   = 0;
// True whenever the governor is not actively governing. The first control
// pass that governs again starts the ramp from the RPM the prop is actually
// turning, so switching on (or recovering) never produces a step.
bool     reseedRamp     = true;

// ── PANEL POT STATE ───────────────────────────────────────────────────────────
uint16_t potFilt16   = 0;      // Filtered ADC reading x16
bool     potSeeded   = false;  // False until the first valid reading
bool     potOpen     = false;  // True while the pot circuit reads open
uint32_t potEdge_ms  = 0;      // When the open/valid reading last agreed with potOpen
uint32_t lastPotRead = 0;

// ── RPM MEASUREMENT ───────────────────────────────────────────────────────────
volatile uint32_t lastPulseTime_us = 0;
volatile uint32_t pulseInterval_us = 0;
volatile bool     newPulse         = false;

uint32_t rpmBuffer[RPM_AVG_SAMPLES];
uint8_t  rpmBufIdx  = 0;
bool     rpmBufFull = false;

float    actualRPM       = 0.0;
uint32_t lastMagPulse_ms = 0;

// ── RELAY TIMING ──────────────────────────────────────────────────────────────
uint32_t lastControlRun    = 0;
uint32_t lastDisplayUpdate = 0;

// ── DISPLAY ───────────────────────────────────────────────────────────────────
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayOK = false;


// =============================================================================
// RPM MEASUREMENT
// =============================================================================

// Magneto pulse interrupt — falling edge on INT0 (pin 2).
//   - An edge inside MIN_PULSE_US of the last accepted pulse is ringing: ignored
//     completely (the timing reference does not move).
//   - An edge after a gap longer than MAX_PULSE_US restarts timing without
//     reporting an interval, so a dropout or an engine start can never be
//     averaged in as a bogus low RPM.
void magPulseISR() {
  uint32_t now = micros();
  if (lastPulseTime_us > 0) {
    uint32_t dt = now - lastPulseTime_us;
    if (dt < MIN_PULSE_US) return;
    if (dt <= MAX_PULSE_US) {
      pulseInterval_us = dt;
      newPulse = true;
    }
  }
  lastPulseTime_us = now;
}

// Rolling average of the last RPM_AVG_SAMPLES pulse intervals.
float calculateRPM(uint32_t interval_us) {
  if (interval_us == 0) return 0.0;
  if ((millis() - lastMagPulse_ms) > MAG_TIMEOUT_MS) return 0.0;

  rpmBuffer[rpmBufIdx % RPM_AVG_SAMPLES] = interval_us;
  rpmBufIdx++;
  if (rpmBufIdx >= RPM_AVG_SAMPLES) rpmBufFull = true;

  uint8_t  count = rpmBufFull ? RPM_AVG_SAMPLES : rpmBufIdx;
  uint32_t total = 0;
  for (uint8_t i = 0; i < count; i++) total += rpmBuffer[i % RPM_AVG_SAMPLES];
  uint32_t avg = total / count;

  return (avg > 0) ? (60000000.0f / ((float)avg * PULSES_PER_REV)) : 0.0f;
}

void updateRPM() {
  if (!newPulse) return;
  // The interval is 4 bytes and the ISR can fire mid-read on an 8-bit MCU,
  // so take the copy with interrupts held off.
  noInterrupts();
  uint32_t interval = pulseInterval_us;
  newPulse = false;
  interrupts();

  lastMagPulse_ms = millis();
  actualRPM = calculateRPM(interval);
}

bool magSignalLost() {
  return (millis() - lastMagPulse_ms) > MAG_TIMEOUT_MS;
}

// RPM is only trusted once the averaging buffer has been refilled with fresh
// intervals since the signal was last lost (or since power-up).
bool rpmValid() {
  return rpmBufFull && !magSignalLost();
}


// =============================================================================
// RELAY CONTROL
// =============================================================================

void relayOff() {
  digitalWrite(PIN_INC_RELAY, LOW);
  digitalWrite(PIN_DEC_RELAY, LOW);
  digitalWrite(PIN_INC_LED,   LOW);
  digitalWrite(PIN_DEC_LED,   LOW);
  relayState = RELAY_OFF;
}


// Time-proportioned relay drive within each 100ms control window.
// duty = 0.0 (off) to 1.0 (fully on throughout window).
// NOT audio-frequency PWM — relay contacts are rated for this.
void setRelay(RelayState direction, float duty) {
  if (direction == RELAY_OFF) { relayOff(); return; }

  // There is deliberately no limit on how long one direction may be
  // commanded. The original 350A holds its output until the selected RPM is
  // reached, and at a pitch stop the aircraft's limit switch opens the relay
  // coil circuit. A timeout here would stop governing every time the prop
  // rested on a stop (low power on approach, for one). The 8 s timeout that
  // stood here was removed 2026-11-15.

  duty = constrain(duty, 0.0f, 1.0f);
  uint32_t pos       = millis() % CONTROL_INTERVAL_MS;
  uint32_t onMs      = (uint32_t)(duty * CONTROL_INTERVAL_MS);
  bool     shouldBeOn = (pos < onMs);

  if (direction == RELAY_INC) {
    digitalWrite(PIN_INC_RELAY, shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_DEC_RELAY, LOW);
    digitalWrite(PIN_INC_LED,   shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_DEC_LED,   LOW);
  } else {
    digitalWrite(PIN_DEC_RELAY, shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_INC_RELAY, LOW);
    digitalWrite(PIN_DEC_LED,   shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_INC_LED,   LOW);
  }

  relayState = direction;
}


// =============================================================================
// MAGNETO SIGNAL SUPERVISION
// =============================================================================

// Called every loop iteration. Like the original 350A, the governor takes no
// action without an RPM signal: both relays are de-energized within
// MAG_TIMEOUT_MS of the last valid pulse, in every mode. This is a standby,
// not an alarm — when pulses return the averaging buffer refills and
// governing resumes on its own.
void checkMagSignal() {
  if (!magSignalLost()) return;
  relayOff();
  rpmBufIdx  = 0;       // Old intervals must not be averaged with new ones
  rpmBufFull = false;
  actualRPM  = 0.0;
  reseedRamp = true;
  if (currentMode == MODE_AUTO) holdReason = HOLD_NO_MAG;
}


// =============================================================================
// PANEL POTENTIOMETER
// =============================================================================

// Convert an ADC reading of the pot divider into the commanded RPM.
// Linear in pot RESISTANCE (so linear in knob rotation for a linear-taper
// pot), not linear in ADC counts. Clamped at both ends.
float potAdcToRPM(float adc) {
  if (adc < 0.0f) adc = 0.0f;
  float frac = (adc >= 1023.0f) ? 1.0f : (adc / (1023.0f - adc));   // = Rpot / 10k
  if (frac > 1.0f) frac = 1.0f;
  return (float)POT_RPM_MAX - (float)(POT_RPM_MAX - POT_RPM_MIN) * frac;
}

// Average a few ADC conversions (about 0.1ms each).
uint16_t readPotAdc() {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < POT_SAMPLES; i++) sum += analogRead(PIN_POT);
  return sum / POT_SAMPLES;
}

// Process one averaged pot reading. Called every POT_INTERVAL_MS.
void updatePot(uint16_t adc) {
  uint32_t now     = millis();
  bool     nowOpen = (adc >= POT_OPEN_ADC);

  // Open-circuit detection, debounced both ways. A wiper that lifts for a
  // moment on a worn track neither trips the fault nor moves the target.
  if (nowOpen == potOpen) {
    potEdge_ms = now;
  } else if ((now - potEdge_ms) >= POT_OPEN_MS) {
    potOpen    = nowOpen;
    potEdge_ms = now;
  }
  if (nowOpen) return;      // Never let an open reading move the target

  // Low-pass filter (time constant ~80ms), kept x16 for resolution
  int16_t in = (int16_t)(adc << 4);
  if (!potSeeded) {
    potFilt16 = (uint16_t)in;
  } else {
    int16_t diff = in - (int16_t)potFilt16;
    int16_t step = diff / 4;
    if (step == 0 && diff != 0) step = (diff > 0) ? 1 : -1;   // always converge fully
    potFilt16 = (uint16_t)((int16_t)potFilt16 + step);
  }

  float rpm = potAdcToRPM(potFilt16 / 16.0f);

  // Hysteresis: a stationary knob must not make the target wander. The two
  // ends are always reachable exactly.
  if (!potSeeded || fabs(rpm - targetSetpoint) >= POT_HYST_RPM ||
      rpm >= (float)POT_RPM_MAX || rpm <= (float)POT_RPM_MIN) {
    targetSetpoint = rpm;
  }
  potSeeded = true;
}


// =============================================================================
// SETPOINT RAMPING
// =============================================================================

// Called every loop iteration. Walks rampedSetpoint toward targetSetpoint
// at RAMP_RATE_RPM_PER_SEC. The control loop always tracks rampedSetpoint,
// so knob movements feel smooth rather than lurching to full motor duty.
void updateRamp() {
  uint32_t now     = millis();
  float    elapsed = (now - lastRampTime) / 1000.0f; // seconds
  lastRampTime = now;

  float maxStep = RAMP_RATE_RPM_PER_SEC * elapsed;
  float diff    = targetSetpoint - rampedSetpoint;

  if (fabs(diff) <= maxStep) {
    rampedSetpoint = targetSetpoint; // Close enough — snap to target
  } else {
    rampedSetpoint += (diff > 0) ? maxStep : -maxStep;
  }
}


// =============================================================================
// CONTROL ALGORITHM
// =============================================================================

// Runs every 100ms. Tracks rampedSetpoint so knob movements are smooth.
void runControlLoop() {
  if (currentMode != MODE_AUTO) { relayOff(); reseedRamp = true; return; }

  // Standby conditions (none latched): take no action, resume when they clear.
  HoldReason why = HOLD_NONE;
  if (!rpmValid())  why = HOLD_NO_MAG;
  else if (actualRPM < RPM_MIN_VALID) why = HOLD_RPM_LOW;
  else if (potOpen) why = HOLD_POT_OPEN;
  if (why != HOLD_NONE) {
    relayOff();
    holdReason = why;
    // A low-RPM hold leaves the ramp alone, so that governing resumes at
    // once against the knob setting: straight to INC (fine pitch) as an
    // engine being restarted in flight comes up through RPM_MIN_VALID.
    // The other standbys restart the ramp from the actual RPM.
    if (why != HOLD_RPM_LOW) reseedRamp = true;
    return;
  }
  holdReason = HOLD_NONE;

  // First governing pass after power-up, a standby or MANUAL: start
  // the ramp from the RPM the prop is turning right now. The setpoint then
  // walks to the knob position at RAMP_RATE_RPM_PER_SEC.
  if (reseedRamp) {
    reseedRamp     = false;
    rampedSetpoint = actualRPM;
    lastRampTime   = millis();
  }

  // The ramp only limits how fast the governor approaches the knob setting.
  // It must never make the governor push the RPM AWAY from the knob setting,
  // which the original (it has no ramp) cannot do. So the tracked setpoint is
  // kept between the actual RPM and the knob setting:
  //  - RPM has moved past the ramp toward the knob setting (throttle advanced,
  //    engine starting or restarting): the ramp carries on from the actual
  //    RPM, and no relay is driven until the RPM stops short.
  //  - RPM is beyond the knob setting, on the far side from the ramp: govern
  //    straight against the knob setting.
  rampedSetpoint = constrain(rampedSetpoint,
                             min(actualRPM, targetSetpoint),
                             max(actualRPM, targetSetpoint));

  // Proportional control against the ramped (smooth) setpoint
  float error = rampedSetpoint - actualRPM;

  if (fabs(error) < DEADBAND_RPM) {
    relayOff();
    return;
  }

  float duty;
  if (fabs(error) >= PROP_ZONE_RPM) {
    duty = 1.0;
  } else {
    duty = fabs(error) / PROP_ZONE_RPM;
    duty = max(duty, (float)(MIN_DUTY_PCT) / 100.0f);
  }

  setRelay((error > 0) ? RELAY_INC : RELAY_DEC, duty);
}


// =============================================================================
// DISPLAY (optional service display)
// =============================================================================

const char* modeName() {
  return currentMode == MODE_AUTO ? "AUTO" : "MAN";
}

void updateDisplay() {
  if (!displayOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  char buf[14];

  // Row 1 (large): the setpoint being tracked + mode indicator
  display.setTextSize(2);
  display.setCursor(0, 0);
  snprintf(buf, sizeof(buf), "%4d", (int)rampedSetpoint);
  display.print(buf);
  display.setTextSize(1);
  display.setCursor(52, 7);
  display.print(F("RPM"));
  display.setCursor(SCREEN_WIDTH - 24, 4);
  display.print(modeName());

  // Row 2 (small): where the knob is pointing, with ramp direction
  display.setCursor(0, 18);
  display.print(F("KNOB:"));
  if (potOpen) {
    display.print(F(" OPEN"));
  } else {
    snprintf(buf, sizeof(buf), "%5d RPM", (int)targetSetpoint);
    display.print(buf);
    float diff = targetSetpoint - rampedSetpoint;
    if (fabs(diff) > 5.0) {
      display.print(diff > 0 ? F(" >") : F(" <"));
    }
  }

  display.drawLine(0, 28, SCREEN_WIDTH, 28, SSD1306_WHITE);

  // Row 3: ACT RPM
  display.setCursor(0, 36);
  display.print(F("ACT:"));
  if (actualRPM < 10 || magSignalLost()) {
    display.print(F("  ---- RPM"));
  } else {
    snprintf(buf, sizeof(buf), "%5d RPM", (int)actualRPM);
    display.print(buf);
  }

  // Row 4: status bar
  display.setCursor(0, 56);
  if (currentMode != MODE_AUTO) {
    display.print(F("MANUAL"));
  } else if (holdReason == HOLD_NO_MAG) {
    display.print(F("NO MAG SIGNAL"));
  } else if (holdReason == HOLD_RPM_LOW) {
    display.print(F("RPM LOW"));
  } else if (holdReason == HOLD_POT_OPEN) {
    display.print(F("POT OPEN"));
  } else {
    float error = rampedSetpoint - actualRPM;
    if (fabs(error) < DEADBAND_RPM) {
      // HOLD: show a stability bar
      display.print(F("HOLD"));
      display.drawLine(28, 60, SCREEN_WIDTH - 2, 60, SSD1306_WHITE);
    } else {
      // Activity bar: width proportional to duty
      float duty = min(fabs(error) / PROP_ZONE_RPM, 1.0f);
      uint8_t barW = (uint8_t)(duty * (SCREEN_WIDTH - 22));
      display.fillRect(22, 56, barW, 8, SSD1306_WHITE);
      // Label inverts over the filled bar
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, 56);
      display.print(error > 0 ? F("INC") : F("DEC"));
      display.setTextColor(SSD1306_WHITE);
    }
  }

  display.display();

#ifdef WIRE_HAS_TIMEOUT
  // A display that stops answering mid-flight must not keep stalling the loop.
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
    displayOK = false;
    Serial.println(F("OLED timeout - display disabled"));
  }
#endif
}


// =============================================================================
// SERIAL DEBUG CONSOLE
// =============================================================================

void printStatus() {
  Serial.print(F("Mode="));    Serial.print(modeName());
  Serial.print(F(" Hold="));
  Serial.print(holdReason == HOLD_NO_MAG    ? F("NO_MAG")
             : holdReason == HOLD_RPM_LOW   ? F("RPM_LOW")
             : holdReason == HOLD_POT_OPEN  ? F("POT_OPEN") : F("-"));
  Serial.print(F(" PotADC=")); Serial.print(potFilt16 >> 4);
  Serial.print(F(" Target=")); Serial.print((int)targetSetpoint);
  Serial.print(F(" Ramped=")); Serial.print((int)rampedSetpoint);
  Serial.print(F(" ACT="));    Serial.print((int)actualRPM);
  Serial.print(F(" Err="));    Serial.print((int)(rampedSetpoint - actualRPM));
  Serial.print(F(" Relay="));
  Serial.println(relayState == RELAY_INC ? F("INC") : relayState == RELAY_DEC ? F("DEC") : F("OFF"));
}

// Bench/service console. Nothing here is needed in the aircraft.
void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 's': printStatus(); break;
    case 'a': currentMode = MODE_AUTO; reseedRamp = true; Serial.println(F("-> AUTO")); break;
    case 'm': currentMode = MODE_MANUAL; relayOff(); Serial.println(F("-> MANUAL")); break;
    case '?':
      Serial.println(F("Commands: s=status a=auto m=manual(relays off) ?=help"));
      break;
  }
}


// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println(F("350A Digital Governor v3.0"));
  Serial.print(F("PULSES_PER_REV=")); Serial.println(PULSES_PER_REV);
  Serial.print(F("Knob range=")); Serial.print(POT_RPM_MIN);
  Serial.print(F("-")); Serial.print(POT_RPM_MAX); Serial.println(F(" RPM"));
  Serial.print(F("RAMP_RATE=")); Serial.print(RAMP_RATE_RPM_PER_SEC); Serial.println(F(" RPM/sec"));

  // Output pins — relay channels drive NPN+PNP high-side switches; HIGH = relay on
  pinMode(PIN_INC_RELAY, OUTPUT); digitalWrite(PIN_INC_RELAY, LOW);
  pinMode(PIN_DEC_RELAY, OUTPUT); digitalWrite(PIN_DEC_RELAY, LOW);
  pinMode(PIN_INC_LED,   OUTPUT); digitalWrite(PIN_INC_LED,   LOW);
  pinMode(PIN_DEC_LED,   OUTPUT); digitalWrite(PIN_DEC_LED,   LOW);

  // RPM input with internal pull-up (board also has a 4.7k pull-up)
  pinMode(PIN_RPM_INPUT, INPUT_PULLUP);

  // Former rotary-switch pins: unconnected on Rev 3.3 — don't leave them floating
  pinMode(3, INPUT_PULLUP);  pinMode(4, INPUT_PULLUP);  pinMode(5, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP); pinMode(11, INPUT_PULLUP);

  // Magneto pulse interrupt — falling edge on INT0 (pin 2)
  EIFR = (1 << INTF0);
  attachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT), magPulseISR, FALLING);

  for (uint8_t i = 0; i < RPM_AVG_SAMPLES; i++) rpmBuffer[i] = 0;
  rpmBufIdx  = 0;
  rpmBufFull = false;
  lastMagPulse_ms = millis();

  // Optional OLED. Probe the address first: with nothing on the service
  // header the display code is never called and the I2C bus stays silent.
  Wire.begin();
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(5000, true);   // a stuck bus can never hang the governor
#endif
  Wire.beginTransmission(OLED_I2C_ADDR);
  displayOK = (Wire.endTransmission() == 0) &&
              display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!displayOK) {
    Serial.println(F("No OLED (optional) - running without display"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);  display.println(F("350A Governor v3.0"));
    display.setCursor(0, 12); display.println(F("Panel pot control"));
    display.setCursor(0, 30); display.println(F("Initializing..."));
    display.display();
    delay(600);
  }

  // Read the knob once so the target is right from the first control pass.
  // If the pot circuit is already open, start in the POT OPEN standby.
  uint16_t adc = readPotAdc();
  potSeeded  = false;
  potOpen    = (adc >= POT_OPEN_ADC);
  potEdge_ms = millis();
  updatePot(adc);
  rampedSetpoint = targetSetpoint;

  lastRampTime   = millis();
  lastControlRun = millis();
  lastPotRead    = millis();
  relayOff();

  // The aircraft prop switch only powers this box in AUTO, so power-up means
  // "govern". Nothing is driven until a valid magneto signal is present, and
  // the ramp then starts from the actual RPM.
  currentMode = MODE_AUTO;
  holdReason  = HOLD_NO_MAG;
  reseedRamp  = true;

  // Hardware watchdog — 250ms timeout
  // wdt_reset() must be called at least every 250ms or MCU resets and
  // relays de-energize. This is the last-resort firmware hang protection.
  wdt_enable(WDTO_250MS);

  Serial.print(F("Boot: knob -> "));
  if (potOpen) Serial.println(F("POT OPEN"));
  else { Serial.print((int)targetSetpoint); Serial.println(F(" RPM")); }
  Serial.println(F("AUTO. Type ? for commands."));
}


// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  wdt_reset(); // Must be first — resets 250ms watchdog

  uint32_t now = millis();

  // Update RPM from ISR data
  if (newPulse) updateRPM();

  // No magneto signal -> relays off within MAG_TIMEOUT_MS, every pass
  checkMagSignal();

  // Read the panel knob at 50Hz
  if ((now - lastPotRead) >= POT_INTERVAL_MS) {
    lastPotRead = now;
    updatePot(readPotAdc());
  }

  // Update setpoint ramp (runs every loop, very lightweight)
  updateRamp();

  // Serial debug console
  handleSerial();

  // Control loop at 10Hz
  if ((now - lastControlRun) >= CONTROL_INTERVAL_MS) {
    lastControlRun = now;
    runControlLoop();
  }

  // Display at 5Hz
  if ((now - lastDisplayUpdate) >= 200) {
    lastDisplayUpdate = now;
    updateDisplay();
  }

  // Serial status in AUTO
  static uint32_t lastStatus = 0;
  if (currentMode == MODE_AUTO && (now - lastStatus) >= 2000) {
    lastStatus = now;
    printStatus();
  }
}
