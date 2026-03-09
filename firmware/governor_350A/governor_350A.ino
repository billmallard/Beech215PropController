/**
 * Beechcraft Bonanza Electric Prop — Digital Governor Controller
 * Replacement firmware for Airborne Electronics 350A (STC SA2700WE)
 *
 * Platform:  Arduino Nano (ATmega328P, 5V, 16MHz)
 * Libraries: Adafruit SSD1306, Adafruit GFX
 *
 * ── CONFIGURATION ───────────────────────────────────────────────────────────
 * Set PULSES_PER_REV before flashing. All other parameters have safe defaults
 * and can be tuned via EEPROM or serial console.
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * MIT License — see repository LICENSE file
 * Open source — https://github.com/[your-repo]/bonanza-prop-governor
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <avr/wdt.h>

// ── CRITICAL CONFIGURATION — SET BEFORE FLASHING ─────────────────────────────
#define PULSES_PER_REV      3       // 6-cyl Continental (IO-470, IO-520, E-185/E-225)
                                    // Use 2 for 4-cylinder engines

// ── RPM PRESETS ───────────────────────────────────────────────────────────────
#define PRESET_MAX          2650    // Full fine pitch / takeoff power
#define PRESET_CRUISE_CLIMB 2450    // Best-power climb
#define PRESET_CRUISE       2300    // Normal cruise (default on AUTO engage)
#define PRESET_ECONOMY      2200    // Long-range economy
// Preset 5 = full coarse (emergency drag reduction) — no RPM target

// ── CONTROL TUNING ────────────────────────────────────────────────────────────
#define DEADBAND_RPM        25      // No correction within ±25 RPM of setpoint
#define PROP_ZONE_RPM       75      // Proportional slowdown begins within ±75 RPM
#define MIN_DUTY_PCT        25      // Minimum motor duty in proportional zone (%)
#define CONTROL_INTERVAL_MS 100     // Control loop runs every 100ms

// ── SAFETY LIMITS ─────────────────────────────────────────────────────────────
#define MAG_TIMEOUT_MS      2000    // Alarm if no magneto pulse for 2 seconds
#define MAX_RELAY_MS        8000    // Safety cutoff: max relay on-time without RPM change (ms)
#define RPM_AVG_SAMPLES     8       // Rolling average for RPM noise filtering

// ── HARDWARE PINS ─────────────────────────────────────────────────────────────
#define PIN_RPM_INPUT       2       // 6N137 output → INT0 interrupt
#define PIN_ENC_A           4       // Rotary encoder channel A
#define PIN_ENC_B           5       // Rotary encoder channel B
#define PIN_ENC_SW          3       // Encoder pushbutton (INT1)
#define PIN_INC_RELAY       6       // ULN2003A IN1 → INC RPM relay
#define PIN_DEC_RELAY       7       // ULN2003A IN2 → DEC RPM relay
#define PIN_INC_LED         8       // INC activity indicator (red LED)
#define PIN_DEC_LED         9       // DEC activity indicator (green LED)
#define PIN_BTN_MAX         10      // Preset 1 (MAX 2650)
#define PIN_BTN_CLIMB       11      // Preset 2 (CRUISE CLIMB 2450)
#define PIN_BTN_CRUISE      12      // Preset 3 (CRUISE 2300)
#define PIN_BTN_ECO         A1      // Preset 4 (ECONOMY 2200)
#define PIN_BTN_LOW         13      // Preset 5 (EMERGENCY COARSE) — LED pin, use via header
#define PIN_POT_WIPER       A0      // Panel potentiometer wiper (continuous control)

// ── DISPLAY ───────────────────────────────────────────────────────────────────
#define OLED_I2C_ADDR       0x3C    // Change to 0x3D if display is blank
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64

// ── EEPROM ADDRESSES ──────────────────────────────────────────────────────────
#define EE_MAGIC            0x00    // 1 byte — magic value 0xAE confirms valid EEPROM
#define EE_PRESET1          0x01    // 2 bytes — uint16
#define EE_PRESET2          0x03    // 2 bytes
#define EE_PRESET3          0x05    // 2 bytes
#define EE_PRESET4          0x07    // 2 bytes
#define EE_DEADBAND         0x09    // 1 byte
#define EE_PROP_ZONE        0x0A    // 1 byte
#define EE_PPR              0x0B    // 1 byte — pulses per rev
#define EE_LAST_PRESET      0x0C    // 1 byte — index 0-4
#define EEPROM_MAGIC_VAL    0xAE

// ── SYSTEM STATE ──────────────────────────────────────────────────────────────
enum ControlMode { MODE_MANUAL, MODE_AUTO, MODE_ALARM };
enum RelayState  { RELAY_OFF, RELAY_INC, RELAY_DEC };

ControlMode currentMode  = MODE_MANUAL;
RelayState  relayState   = RELAY_OFF;
String      alarmMessage = "";

// ── RPM MEASUREMENT ───────────────────────────────────────────────────────────
volatile uint32_t lastPulseTime_us = 0;
volatile uint32_t pulseInterval_us = 0;
volatile bool     newPulse         = false;

// Rolling average buffer for noise rejection
uint32_t rpmBuffer[RPM_AVG_SAMPLES];
uint8_t  rpmBufIdx = 0;
bool     rpmBufFull = false;

float actualRPM   = 0.0;
float setpointRPM = PRESET_CRUISE;

// ── PRESET CONFIGURATION ──────────────────────────────────────────────────────
struct Preset {
  const char* name;
  uint16_t    rpm;        // 0 = full coarse (emergency)
  bool        fullCoarse; // true = drive to limit, no RPM target
};

Preset presets[5] = {
  { "MAX",       PRESET_MAX,          false },
  { "CLB",       PRESET_CRUISE_CLIMB, false },
  { "CRUISE",    PRESET_CRUISE,       false },
  { "ECO",       PRESET_ECONOMY,      false },
  { "LOW/EMER",  0,                   true  },
};

uint8_t activePreset = 2;   // Default: CRUISE
int16_t encoderTrim  = 0;   // Fine adjustment in RPM

// ── RELAY TIMING ──────────────────────────────────────────────────────────────
uint32_t relayOnTime_ms   = 0;   // When current relay was energized
uint32_t lastControlRun   = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t lastMagPulse_ms  = 0;   // Last time we saw a magneto pulse (ms)

// ── DISPLAY ───────────────────────────────────────────────────────────────────
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayOK = false;

// ── ENCODER ───────────────────────────────────────────────────────────────────
volatile int8_t encoderDelta = 0;
uint8_t lastEncA = 0;


// ═══════════════════════════════════════════════════════════════════════════════
// INTERRUPT SERVICE ROUTINES
// ═══════════════════════════════════════════════════════════════════════════════

// Magneto pulse interrupt — fires on each falling edge from 6N137 output
ISR(INT0_vect) {
  uint32_t now = micros();
  if (lastPulseTime_us > 0) {
    pulseInterval_us = now - lastPulseTime_us;
  }
  lastPulseTime_us = now;
  newPulse = true;
}

// Rotary encoder — polled in main loop (not interrupt-driven)
// This avoids missed steps from contact bounce at slow rotation speeds.


// ═══════════════════════════════════════════════════════════════════════════════
// RPM MEASUREMENT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Convert current pulse interval to RPM using rolling average.
 * RPM = 60,000,000 µs/min ÷ (interval_µs × pulses_per_rev)
 */
float calculateRPM() {
  if (pulseInterval_us == 0) return 0.0;

  // Check for stale data (engine stopped or signal lost)
  if ((millis() - lastMagPulse_ms) > MAG_TIMEOUT_MS) return 0.0;

  // Add to rolling average buffer
  rpmBuffer[rpmBufIdx % RPM_AVG_SAMPLES] = pulseInterval_us;
  rpmBufIdx++;
  if (rpmBufIdx >= RPM_AVG_SAMPLES) rpmBufFull = true;

  uint8_t count = rpmBufFull ? RPM_AVG_SAMPLES : rpmBufIdx;
  uint32_t total = 0;
  for (uint8_t i = 0; i < count; i++) {
    total += rpmBuffer[i % RPM_AVG_SAMPLES];
  }
  uint32_t avgInterval = total / count;

  if (avgInterval == 0) return 0.0;
  return 60000000.0f / ((float)avgInterval * PULSES_PER_REV);
}

/**
 * Check if the magneto signal has been lost.
 * Returns true if no pulse seen recently.
 */
bool magSignalLost() {
  return (millis() - lastMagPulse_ms) > MAG_TIMEOUT_MS;
}

/**
 * Update the RPM reading. Call from main loop when newPulse is true.
 */
void updateRPM() {
  if (!newPulse) return;
  newPulse = false;
  lastMagPulse_ms = millis();
  actualRPM = calculateRPM();
}


// ═══════════════════════════════════════════════════════════════════════════════
// RELAY CONTROL
// ═══════════════════════════════════════════════════════════════════════════════

void relayOff() {
  digitalWrite(PIN_INC_RELAY, LOW);
  digitalWrite(PIN_DEC_RELAY, LOW);
  digitalWrite(PIN_INC_LED, LOW);
  digitalWrite(PIN_DEC_LED, LOW);
  relayState   = RELAY_OFF;
  relayOnTime_ms = 0;
}

/**
 * Drive a relay with proportional duty cycle.
 * duty: 0.0–1.0 (fraction of CONTROL_INTERVAL_MS that relay is on)
 * direction: RELAY_INC or RELAY_DEC
 *
 * Implementation: time-proportioned within each 100ms control window.
 * Relays are NOT PWM'd at audio frequency (contact life concern).
 * Instead: relay on for (duty × 100ms), then off for remainder.
 */
void setRelay(RelayState direction, float duty) {
  // Safety: enforce maximum continuous run time
  if (relayState == direction && relayOnTime_ms > 0) {
    if ((millis() - relayOnTime_ms) > MAX_RELAY_MS) {
      relayOff();
      setAlarm("RELAY TIMEOUT");
      return;
    }
  }

  if (direction == RELAY_OFF) {
    relayOff();
    return;
  }

  // Clamp duty to sane range
  duty = constrain(duty, 0.0f, 1.0f);

  // Calculate on-time within the current 100ms cycle
  uint32_t cyclePosition = millis() % CONTROL_INTERVAL_MS;
  uint32_t onDuration    = (uint32_t)(duty * CONTROL_INTERVAL_MS);

  bool shouldBeOn = (cyclePosition < onDuration);

  if (direction == RELAY_INC) {
    digitalWrite(PIN_INC_RELAY, shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_DEC_RELAY, LOW);
    digitalWrite(PIN_INC_LED, shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_DEC_LED, LOW);
  } else {
    digitalWrite(PIN_DEC_RELAY, shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_INC_RELAY, LOW);
    digitalWrite(PIN_DEC_LED, shouldBeOn ? HIGH : LOW);
    digitalWrite(PIN_INC_LED, LOW);
  }

  if (relayState != direction) {
    relayOnTime_ms = millis(); // Reset timer when direction changes
  }
  relayState = direction;
}


// ═══════════════════════════════════════════════════════════════════════════════
// CONTROL ALGORITHM
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Get the effective setpoint RPM from active preset + encoder trim.
 * Returns 0 if active preset is the full-coarse (emergency) preset.
 */
float getEffectiveSetpoint() {
  if (presets[activePreset].fullCoarse) return 0;
  return (float)(presets[activePreset].rpm) + (float)encoderTrim;
}

/**
 * Main control loop — called every CONTROL_INTERVAL_MS (100ms).
 *
 * Logic:
 *   1. Check safety conditions first — bail out before any relay action
 *   2. If full-coarse preset: drive DEC until limit switch stops motor
 *   3. Calculate RPM error
 *   4. If |error| < DEADBAND: hold (no action)
 *   5. If |error| < PROP_ZONE: proportional — scale duty by error magnitude
 *   6. If |error| >= PROP_ZONE: full duty
 */
void runControlLoop() {
  if (currentMode != MODE_AUTO) {
    relayOff();
    return;
  }

  // ── Safety check 1: magneto signal ────────────────────────────────────────
  if (magSignalLost()) {
    relayOff();
    setAlarm("NO MAG SIGNAL");
    return;
  }

  // ── Safety check 2: RPM out of sane range ─────────────────────────────────
  if (actualRPM > 0 && (actualRPM < 400 || actualRPM > 3500)) {
    relayOff();
    setAlarm("RPM OUT OF RANGE");
    return;
  }

  // ── Full coarse / emergency preset ────────────────────────────────────────
  if (presets[activePreset].fullCoarse) {
    setRelay(RELAY_DEC, 1.0);
    return;
  }

  // ── Normal proportional control ───────────────────────────────────────────
  float target = getEffectiveSetpoint();
  float error  = target - actualRPM;

  // Deadband: do nothing
  if (fabs(error) < DEADBAND_RPM) {
    relayOff();
    return;
  }

  // Calculate duty cycle
  float duty;
  if (fabs(error) >= PROP_ZONE_RPM) {
    duty = 1.0;   // Full drive outside proportional zone
  } else {
    // Linear scale within proportional zone
    duty = fabs(error) / PROP_ZONE_RPM;
    // Apply minimum duty so motor actually moves (overcome static friction)
    float minDuty = MIN_DUTY_PCT / 100.0;
    duty = max(duty, minDuty);
  }

  // Command relay direction based on error sign
  if (error > 0) {
    setRelay(RELAY_INC, duty);   // RPM too low: increase RPM (fine pitch)
  } else {
    setRelay(RELAY_DEC, duty);   // RPM too high: decrease RPM (coarse pitch)
  }
}


// ═══════════════════════════════════════════════════════════════════════════════
// ALARM HANDLING
// ═══════════════════════════════════════════════════════════════════════════════

void setAlarm(const char* message) {
  relayOff();
  currentMode  = MODE_ALARM;
  alarmMessage = String(message);
  Serial.print("ALARM: ");
  Serial.println(message);
}

void clearAlarm() {
  if (currentMode == MODE_ALARM) {
    currentMode  = MODE_MANUAL;
    alarmMessage = "";
  }
}


// ═══════════════════════════════════════════════════════════════════════════════
// PANEL INPUTS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Debounced button read with internal pull-up.
 * Returns true on button press (falling edge — pin goes LOW when pressed).
 */
struct Button {
  uint8_t  pin;
  bool     lastState;
  uint32_t lastChange;
};

#define NUM_BTNS 5
Button buttons[NUM_BTNS] = {
  { PIN_BTN_MAX,   HIGH, 0 },
  { PIN_BTN_CLIMB, HIGH, 0 },
  { PIN_BTN_CRUISE,HIGH, 0 },
  { PIN_BTN_ECO,   HIGH, 0 },
  { PIN_BTN_LOW,   HIGH, 0 },
};

/**
 * Check all preset buttons. Returns 0-4 if a button was just pressed, -1 otherwise.
 */
int8_t readPresetButton() {
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    bool state = digitalRead(buttons[i].pin);
    if (state == LOW && buttons[i].lastState == HIGH &&
        (millis() - buttons[i].lastChange) > 50) {  // 50ms debounce
      buttons[i].lastChange = millis();
      buttons[i].lastState  = LOW;
      return (int8_t)i;
    }
    if (state == HIGH && buttons[i].lastState == LOW) {
      buttons[i].lastState = HIGH;
    }
  }
  return -1;
}

/**
 * Read rotary encoder using polling with software debounce.
 * Updates the encoderDelta counter (positive = CW, negative = CCW).
 */
void updateEncoder() {
  static uint8_t lastEnc = 0b11;
  static int8_t  enc_val = 0;

  uint8_t a = digitalRead(PIN_ENC_A);
  uint8_t b = digitalRead(PIN_ENC_B);
  uint8_t cur = (a << 1) | b;

  if (cur != lastEnc) {
    // Gray code state machine
    static const int8_t ENC_STATES[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    enc_val += ENC_STATES[(lastEnc << 2) | cur];
    if (enc_val >= 4) {
      encoderDelta++;
      enc_val = 0;
    } else if (enc_val <= -4) {
      encoderDelta--;
      enc_val = 0;
    }
    lastEnc = cur;
  }
}

/**
 * Read and average the panel potentiometer.
 * Maps 0–1023 ADC range to PRESET_ECONOMY–PRESET_MAX RPM range.
 * Software linearization makes this behave like a linear taper regardless
 * of physical pot taper — fixes the "hypersensitive" feel of audio-taper pots.
 */
float readPotRPM() {
  // Average 64 samples for noise rejection
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 64; i++) {
    sum += analogRead(PIN_POT_WIPER);
  }
  float avg = sum / 64.0;

  // Map to RPM range
  float rpm = map((long)avg, 0, 1023, PRESET_ECONOMY, PRESET_MAX);
  return constrain(rpm, PRESET_ECONOMY, PRESET_MAX);
}


// ═══════════════════════════════════════════════════════════════════════════════
// DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════

void updateDisplay() {
  if (!displayOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (currentMode == MODE_ALARM) {
    // ── Alarm screen ─────────────────────────────────────────────────────────
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("!! ALARM !!");

    display.setTextSize(1);
    display.setCursor(0, 22);
    display.println(alarmMessage);

    display.setCursor(0, 36);
    display.println("CYCLE AUTO SWITCH");
    display.setCursor(0, 46);
    display.println("TO CLEAR");

    // Flash border
    if ((millis() / 500) % 2 == 0) {
      display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    }

  } else {
    // ── Normal operating screen ───────────────────────────────────────────────

    // Line 1: Preset name + mode
    display.setTextSize(1);
    display.setCursor(0, 0);

    String presetLabel = String(presets[activePreset].name);
    if (encoderTrim != 0) {
      presetLabel += (encoderTrim > 0 ? " +" : " ");
      presetLabel += String(encoderTrim);
    }
    display.print(presetLabel);

    // Right-align mode indicator
    String modeStr = (currentMode == MODE_AUTO) ? "AUTO" : "MAN";
    display.setCursor(SCREEN_WIDTH - (modeStr.length() * 6), 0);
    display.print(modeStr);

    // Divider line
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // Line 2: Setpoint
    display.setCursor(0, 14);
    display.print("SET:");
    if (presets[activePreset].fullCoarse) {
      display.print(" COARSE");
    } else {
      int16_t sp = (int16_t)getEffectiveSetpoint();
      char buf[12];
      snprintf(buf, sizeof(buf), "%5d RPM", sp);
      display.print(buf);
    }

    // Line 3: Actual RPM
    display.setCursor(0, 26);
    display.print("ACT:");
    if (actualRPM < 10 || magSignalLost()) {
      display.print("  ---- RPM");
    } else {
      char buf[12];
      snprintf(buf, sizeof(buf), "%5d RPM", (int16_t)actualRPM);
      display.print(buf);
    }

    // Divider
    display.drawLine(0, 38, SCREEN_WIDTH, 38, SSD1306_WHITE);

    // Line 4: Status bar
    display.setCursor(0, 42);
    if (currentMode != MODE_AUTO) {
      display.print("MANUAL");
    } else if (magSignalLost()) {
      display.print("NO MAG SIG");
    } else if (presets[activePreset].fullCoarse) {
      display.print("COARSE >>>>");
    } else {
      float error = getEffectiveSetpoint() - actualRPM;
      if (fabs(error) < DEADBAND_RPM) {
        display.print("HOLD");
        // Draw stability indicator bar
        display.drawLine(36, 46, SCREEN_WIDTH - 2, 46, SSD1306_WHITE);
      } else if (error > 0) {
        display.print("INC ");
        // Draw duty cycle bar
        float duty = min(fabs(error) / PROP_ZONE_RPM, 1.0);
        uint8_t barLen = (uint8_t)(duty * (SCREEN_WIDTH - 36));
        display.fillRect(36, 42, barLen, 8, SSD1306_WHITE);
        // Arrow indicator
        for (uint8_t i = 0; i < 3; i++) {
          display.drawLine(SCREEN_WIDTH-10+i, 42, SCREEN_WIDTH-10+i, 50, SSD1306_WHITE);
        }
      } else {
        display.print("DEC ");
        float duty = min(fabs(error) / PROP_ZONE_RPM, 1.0);
        uint8_t barLen = (uint8_t)(duty * (SCREEN_WIDTH - 36));
        display.fillRect(36, 42, barLen, 8, SSD1306_WHITE);
      }
    }

    // Line 5: Error value (if in AUTO and significant error)
    if (currentMode == MODE_AUTO && !presets[activePreset].fullCoarse && !magSignalLost()) {
      float error = getEffectiveSetpoint() - actualRPM;
      if (fabs(error) >= DEADBAND_RPM) {
        char buf[10];
        snprintf(buf, sizeof(buf), "%+.0f", error);
        display.setCursor(SCREEN_WIDTH - strlen(buf)*6, 56);
        display.setTextSize(1);
        display.print(buf);
      }
    }
  }

  display.display();
}


// ═══════════════════════════════════════════════════════════════════════════════
// EEPROM PERSISTENCE
// ═══════════════════════════════════════════════════════════════════════════════

void loadSettings() {
  uint8_t magic = EEPROM.read(EE_MAGIC);
  if (magic != EEPROM_MAGIC_VAL) {
    // First boot or corrupt EEPROM — write defaults
    EEPROM.write(EE_MAGIC, EEPROM_MAGIC_VAL);
    EEPROM.put(EE_PRESET1, (uint16_t)PRESET_MAX);
    EEPROM.put(EE_PRESET2, (uint16_t)PRESET_CRUISE_CLIMB);
    EEPROM.put(EE_PRESET3, (uint16_t)PRESET_CRUISE);
    EEPROM.put(EE_PRESET4, (uint16_t)PRESET_ECONOMY);
    EEPROM.write(EE_DEADBAND,    DEADBAND_RPM);
    EEPROM.write(EE_PROP_ZONE,   PROP_ZONE_RPM);
    EEPROM.write(EE_PPR,         PULSES_PER_REV);
    EEPROM.write(EE_LAST_PRESET, 2);  // Default: CRUISE
    Serial.println("EEPROM initialized with defaults");
  } else {
    // Load saved settings
    uint16_t p1, p2, p3, p4;
    EEPROM.get(EE_PRESET1, p1); presets[0].rpm = p1;
    EEPROM.get(EE_PRESET2, p2); presets[1].rpm = p2;
    EEPROM.get(EE_PRESET3, p3); presets[2].rpm = p3;
    EEPROM.get(EE_PRESET4, p4); presets[3].rpm = p4;
    activePreset = EEPROM.read(EE_LAST_PRESET);
    if (activePreset >= NUM_BTNS) activePreset = 2;
    Serial.println("Settings loaded from EEPROM");
  }
}

void saveActivePreset() {
  EEPROM.write(EE_LAST_PRESET, activePreset);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SERIAL DEBUG
// ═══════════════════════════════════════════════════════════════════════════════

void printStatus() {
  Serial.print("Mode=");
  Serial.print(currentMode == MODE_AUTO ? "AUTO" : currentMode == MODE_ALARM ? "ALARM" : "MAN");
  Serial.print(" Preset=");
  Serial.print(presets[activePreset].name);
  Serial.print(" SET=");
  Serial.print((int)getEffectiveSetpoint());
  Serial.print(" ACT=");
  Serial.print((int)actualRPM);
  Serial.print(" ERR=");
  Serial.print((int)(getEffectiveSetpoint() - actualRPM));
  Serial.print(" Relay=");
  Serial.print(relayState == RELAY_INC ? "INC" : relayState == RELAY_DEC ? "DEC" : "OFF");
  Serial.print(" PulseInterval=");
  Serial.print(pulseInterval_us);
  Serial.println("µs");
}

void handleSerial() {
  if (!Serial.available()) return;
  char cmd = Serial.read();
  switch (cmd) {
    case 's': printStatus(); break;
    case 'a': currentMode = MODE_AUTO;   Serial.println("AUTO"); break;
    case 'm': currentMode = MODE_MANUAL; relayOff(); Serial.println("MANUAL"); break;
    case 'c': clearAlarm(); Serial.println("Alarm cleared"); break;
    case '1': activePreset = 0; Serial.println("Preset: MAX"); break;
    case '2': activePreset = 1; Serial.println("Preset: CLIMB"); break;
    case '3': activePreset = 2; Serial.println("Preset: CRUISE"); break;
    case '4': activePreset = 3; Serial.println("Preset: ECO"); break;
    case '5': activePreset = 4; Serial.println("Preset: LOW/COARSE"); break;
    case '+': encoderTrim += 10; Serial.print("Trim: "); Serial.println(encoderTrim); break;
    case '-': encoderTrim -= 10; Serial.print("Trim: "); Serial.println(encoderTrim); break;
    case '0': encoderTrim = 0; Serial.println("Trim reset"); break;
    case '?':
      Serial.println("Commands: s=status a=auto m=manual c=clear_alarm");
      Serial.println("  1-5=presets +/-=trim 0=reset_trim ?=help");
      break;
  }
}


// ═══════════════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println("350A Digital Governor v1.0");
  Serial.print("PULSES_PER_REV="); Serial.println(PULSES_PER_REV);

  // ── Output pins ──
  pinMode(PIN_INC_RELAY, OUTPUT);  digitalWrite(PIN_INC_RELAY, LOW);
  pinMode(PIN_DEC_RELAY, OUTPUT);  digitalWrite(PIN_DEC_RELAY, LOW);
  pinMode(PIN_INC_LED,   OUTPUT);  digitalWrite(PIN_INC_LED,   LOW);
  pinMode(PIN_DEC_LED,   OUTPUT);  digitalWrite(PIN_DEC_LED,   LOW);

  // ── Input pins with internal pull-ups ──
  pinMode(PIN_RPM_INPUT, INPUT_PULLUP);
  pinMode(PIN_ENC_A,     INPUT_PULLUP);
  pinMode(PIN_ENC_B,     INPUT_PULLUP);
  pinMode(PIN_ENC_SW,    INPUT_PULLUP);
  pinMode(PIN_BTN_MAX,   INPUT_PULLUP);
  pinMode(PIN_BTN_CLIMB, INPUT_PULLUP);
  pinMode(PIN_BTN_CRUISE,INPUT_PULLUP);
  pinMode(PIN_BTN_ECO,   INPUT_PULLUP);
  pinMode(PIN_BTN_LOW,   INPUT_PULLUP);

  // ── Magneto pulse interrupt — falling edge on INT0 (pin 2) ──
  EIFR = (1 << INTF0);   // Clear any pending interrupt
  attachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT), []() {
    uint32_t now = micros();
    if (lastPulseTime_us > 0) pulseInterval_us = now - lastPulseTime_us;
    lastPulseTime_us = now;
    newPulse = true;
  }, FALLING);

  // ── Initialize RPM buffer ──
  for (uint8_t i = 0; i < RPM_AVG_SAMPLES; i++) rpmBuffer[i] = 0;
  lastMagPulse_ms = millis();   // Prevent immediate "NO MAG SIGNAL" alarm at boot

  // ── OLED display ──
  displayOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!displayOK) {
    Serial.println("OLED not found — check I2C address and wiring");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("350A Governor v1.0");
    display.println("Initializing...");
    display.display();
    delay(1000);
  }

  // ── Load settings from EEPROM ──
  loadSettings();

  // ── Startup state: always MANUAL ──
  currentMode = MODE_MANUAL;
  relayOff();

  // ── Enable hardware watchdog (250ms) ──
  // Main loop must call wdt_reset() at least every 250ms or MCU resets
  wdt_enable(WDTO_250MS);

  Serial.println("Ready. Type ? for commands.");
}


// ═══════════════════════════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void loop() {
  // ── Watchdog reset — MUST be first line in loop ──────────────────────────
  wdt_reset();

  uint32_t now = millis();

  // ── Update RPM from ISR data ──────────────────────────────────────────────
  if (newPulse) {
    updateRPM();
  }

  // ── Read encoder ─────────────────────────────────────────────────────────
  updateEncoder();
  if (encoderDelta != 0) {
    encoderTrim += encoderDelta * 10;   // 10 RPM per detent
    encoderTrim  = constrain(encoderTrim, -300, 300);
    encoderDelta = 0;
  }

  // ── Read preset buttons ───────────────────────────────────────────────────
  int8_t btn = readPresetButton();
  if (btn >= 0) {
    activePreset = (uint8_t)btn;
    encoderTrim  = 0;    // Reset trim when switching presets
    saveActivePreset();
    Serial.print("Preset selected: ");
    Serial.println(presets[activePreset].name);
  }

  // ── Serial console ────────────────────────────────────────────────────────
  handleSerial();

  // ── Control loop (runs every CONTROL_INTERVAL_MS = 100ms) ────────────────
  if ((now - lastControlRun) >= CONTROL_INTERVAL_MS) {
    lastControlRun = now;
    runControlLoop();
  }

  // ── Display update (runs every 200ms = 5Hz) ───────────────────────────────
  if ((now - lastDisplayUpdate) >= 200) {
    lastDisplayUpdate = now;
    updateDisplay();
  }

  // ── Serial status every 2 seconds in debug mode ──────────────────────────
  static uint32_t lastStatus = 0;
  if ((now - lastStatus) >= 2000) {
    lastStatus = now;
    if (currentMode == MODE_AUTO) {
      printStatus();
    }
  }

  // ── Mag signal loss check ─────────────────────────────────────────────────
  // Only alarm in AUTO mode — in MANUAL the mag signal isn't needed
  if (currentMode == MODE_AUTO && magSignalLost()) {
    setAlarm("NO MAG SIGNAL");
  }
}


// ═══════════════════════════════════════════════════════════════════════════════
// NOTE: AUTO MODE ENGAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════
//
// The AUTO/MANUAL switch in the aircraft is wired to the 12V supply on Pin A.
// When the switch is in AUTO, Pin A has 12V and the controller has power.
// When the switch is in MANUAL (or OFF), Pin A has no power and the controller
// is unpowered — relays de-energize by loss of power.
//
// This means the controller doesn't need to "detect" the AUTO switch position
// directly. Instead, implement AUTO engagement via the serial console command
// 'a' for bench testing, or add an optional AUTO engagement button on the panel
// unit connected to a Nano digital pin.
//
// For the simplest implementation matching the original 350A:
//   - Switch in AUTO position: power applied → controller boots in MANUAL mode
//   - Press any preset button to select setpoint
//   - Uncomment the auto-engage line below to auto-engage when power is first
//     applied and a valid RPM signal exists:
//
//   In setup(), after loadSettings():
//   // currentMode = MODE_AUTO;   // Uncomment to auto-engage on power-on
//
// For pilot safety, the default (commented out) keeps the system in MANUAL
// at power-on and requires explicit engagement via serial or button.
// The pilot should select a preset, verify the SET display, then engage.
