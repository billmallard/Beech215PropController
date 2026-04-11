/**
 * Beechcraft Bonanza Electric Prop — Digital Governor Controller
 * Replacement firmware for Airborne Electronics 350A (STC SA2700WE)
 *
 * Platform:  Arduino Nano (ATmega328P, 5V, 16MHz)
 * Libraries: Adafruit SSD1306, Adafruit GFX
 *
 * Panel interface: 5-position rotary switch (one position per RPM preset)
 *
 * ── CONFIGURATION ───────────────────────────────────────────────────────────
 * Set PULSES_PER_REV before flashing. All other parameters have safe defaults.
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * MIT License — see repository LICENSE file
 * https://github.com/billmallard/Beech215PropController
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <avr/wdt.h>

// ── CRITICAL: SET BEFORE FLASHING ────────────────────────────────────────────
#define PULSES_PER_REV      3       // 6-cyl Continental (IO-470, IO-520, E-185/E-225)
                                    // Use 2 for 4-cylinder engines

// ── RPM PRESETS ───────────────────────────────────────────────────────────────
#define PRESET_MAX          2650    // Position 1 — takeoff / full fine pitch
#define PRESET_CRUISE_CLIMB 2450    // Position 2 — best-power climb
#define PRESET_CRUISE       2300    // Position 3 — normal cruise
#define PRESET_ECONOMY      2200    // Position 4 — long-range economy
// Position 5 = EMERGENCY/LOW — drives to full coarse, no RPM target

// ── CONTROL TUNING ────────────────────────────────────────────────────────────
#define DEADBAND_RPM        25      // No correction within +/-25 RPM of setpoint
#define PROP_ZONE_RPM       75      // Proportional slowdown begins within +/-75 RPM
#define MIN_DUTY_PCT        25      // Minimum motor duty in proportional zone (%)
#define CONTROL_INTERVAL_MS 100     // Control loop runs every 100ms

// ── SETPOINT RAMPING ──────────────────────────────────────────────────────────
// When switching presets, the setpoint walks toward the new target at this
// rate rather than jumping instantly. This prevents a sudden large RPM error
// from commanding full-duty relay drive the moment you turn the knob.
//
// 100 RPM/sec: ECO->MAX (450 RPM step) takes ~4.5 seconds
// Increase for snappier transitions; decrease for more gradual.
#define RAMP_RATE_RPM_PER_SEC  100

// ── SAFETY LIMITS ─────────────────────────────────────────────────────────────
#define MAG_TIMEOUT_MS      2000    // Alarm if no magneto pulse for 2 seconds
#define MAX_RELAY_MS        8000    // Safety: max relay on-time without RPM movement
#define RPM_AVG_SAMPLES     8       // Rolling average depth for noise rejection

// ── SWITCH DEBOUNCE ───────────────────────────────────────────────────────────
// Rotary switches are "break before make" — briefly no pin is grounded as
// the wiper moves between positions. We ignore transitions shorter than this.
#define SWITCH_SETTLE_MS    80

// ── HARDWARE PINS ─────────────────────────────────────────────────────────────
#define PIN_RPM_INPUT   2   // 6N137 output -> INT0 (hardware interrupt)
#define PIN_SW_POS1     3   // Rotary switch position 1: MAX (2650)
#define PIN_SW_POS2     4   // Rotary switch position 2: CRUISE CLIMB (2450)
#define PIN_SW_POS3     5   // Rotary switch position 3: CRUISE (2300)
#define PIN_INC_RELAY   6   // ULN2003A IN1 -> INC RPM relay (fine pitch)
#define PIN_DEC_RELAY   7   // ULN2003A IN2 -> DEC RPM relay (coarse pitch)
#define PIN_INC_LED     8   // Red LED — INC activity indicator
#define PIN_DEC_LED     9   // Green LED — DEC activity indicator
#define PIN_SW_POS4     10  // Rotary switch position 4: ECONOMY (2200)
#define PIN_SW_POS5     11  // Rotary switch position 5: LOW/EMERGENCY COARSE
// A4 = SDA (OLED), A5 = SCL (OLED) — claimed by Wire library

// Rotary switch wiring:
//   COMMON terminal -> GND
//   Each position terminal -> its Nano pin above
//   Internal pull-ups keep all pins HIGH; selected position pulls its pin LOW.
//   No external resistors needed.

// ── DISPLAY ───────────────────────────────────────────────────────────────────
#define OLED_I2C_ADDR   0x3C    // Change to 0x3D if display is blank at boot
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// ── EEPROM LAYOUT ─────────────────────────────────────────────────────────────
#define EE_MAGIC        0x00    // 1 byte: 0xAE = valid data present
#define EE_PRESET1      0x01    // 2 bytes uint16
#define EE_PRESET2      0x03
#define EE_PRESET3      0x05
#define EE_PRESET4      0x07
#define EEPROM_MAGIC    0xAE

// ── PRESET TABLE ──────────────────────────────────────────────────────────────
struct Preset {
  const char* name;       // Short name for OLED (fits in large font)
  const char* label;      // Descriptive label (small font, second row)
  uint16_t    rpm;        // Target RPM (unused if fullCoarse)
  bool        fullCoarse; // true: drive to coarse limit, no RPM target
};

Preset presets[5] = {
  { "MAX",    "TAKEOFF",    PRESET_MAX,          false },
  { "CLB",    "CRS CLIMB",  PRESET_CRUISE_CLIMB, false },
  { "CRUISE", "CRUISE",     PRESET_CRUISE,       false },
  { "ECO",    "ECONOMY",    PRESET_ECONOMY,      false },
  { "LOW",    "EMER COARSE",0,                   true  },
};

// ── SYSTEM STATE ──────────────────────────────────────────────────────────────
enum ControlMode { MODE_MANUAL, MODE_AUTO, MODE_ALARM };
enum RelayState  { RELAY_OFF, RELAY_INC, RELAY_DEC };

ControlMode currentMode  = MODE_MANUAL;
RelayState  relayState   = RELAY_OFF;
String      alarmMessage = "";

int8_t activePreset    = 2;  // Active switch position (0-4), default CRUISE
int8_t lastValidPreset = 2;  // Last confirmed position (held during transitions)

// ── SETPOINT RAMP STATE ───────────────────────────────────────────────────────
float    rampedSetpoint = (float)PRESET_CRUISE; // What the control loop tracks
float    targetSetpoint = (float)PRESET_CRUISE; // Where the ramp is headed
uint32_t lastRampTime   = 0;

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
uint32_t relayOnTime_ms    = 0;
uint32_t lastControlRun    = 0;
uint32_t lastDisplayUpdate = 0;

// ── DISPLAY ───────────────────────────────────────────────────────────────────
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayOK = false;


// =============================================================================
// RPM MEASUREMENT
// =============================================================================

float calculateRPM() {
  if (pulseInterval_us == 0) return 0.0;
  if ((millis() - lastMagPulse_ms) > MAG_TIMEOUT_MS) return 0.0;

  rpmBuffer[rpmBufIdx % RPM_AVG_SAMPLES] = pulseInterval_us;
  rpmBufIdx++;
  if (rpmBufIdx >= RPM_AVG_SAMPLES) rpmBufFull = true;

  uint8_t  count = rpmBufFull ? RPM_AVG_SAMPLES : rpmBufIdx;
  uint32_t total = 0;
  for (uint8_t i = 0; i < count; i++) total += rpmBuffer[i % RPM_AVG_SAMPLES];
  uint32_t avg = total / count;

  return (avg > 0) ? (60000000.0f / ((float)avg * PULSES_PER_REV)) : 0.0;
}

void updateRPM() {
  if (!newPulse) return;
  newPulse = false;
  lastMagPulse_ms = millis();
  actualRPM = calculateRPM();
}

bool magSignalLost() {
  return (millis() - lastMagPulse_ms) > MAG_TIMEOUT_MS;
}


// =============================================================================
// ROTARY SWITCH READING
// =============================================================================

const uint8_t SWITCH_PINS[5] = {
  PIN_SW_POS1, PIN_SW_POS2, PIN_SW_POS3, PIN_SW_POS4, PIN_SW_POS5
};

// Returns 0-4 if a position pin is grounded, -1 if between positions.
int8_t readSwitchRaw() {
  for (uint8_t i = 0; i < 5; i++) {
    if (digitalRead(SWITCH_PINS[i]) == LOW) return (int8_t)i;
  }
  return -1;
}

// Debounced switch read. Handles break-before-make transitions by requiring
// the new position to be stable for SWITCH_SETTLE_MS before accepting it.
// Returns a confirmed position (0-4), never -1.
static uint32_t switchChangeTime = 0;
static int8_t   pendingPosition  = -1;

int8_t readSwitch() {
  int8_t raw = readSwitchRaw();

  if (raw == -1) {
    // Between positions: hold current, reset any pending candidate
    pendingPosition = -1;
    return lastValidPreset;
  }

  if (raw == lastValidPreset) {
    // No change — already at a stable position
    pendingPosition = -1;
    return lastValidPreset;
  }

  // Position appears to have changed
  if (raw != pendingPosition) {
    // New candidate — start settle timer
    pendingPosition  = raw;
    switchChangeTime = millis();
  } else if ((millis() - switchChangeTime) >= SWITCH_SETTLE_MS) {
    // Stable for required time — accept the change
    lastValidPreset = raw;
    pendingPosition = -1;
  }

  return lastValidPreset; // Return last confirmed position while settling
}


// =============================================================================
// SETPOINT RAMPING
// =============================================================================

// Called every loop iteration. Walks rampedSetpoint toward targetSetpoint
// at RAMP_RATE_RPM_PER_SEC. The control loop always tracks rampedSetpoint,
// so preset transitions feel smooth rather than lurching to full motor duty.
//
// Exception: EMERGENCY/LOW (full coarse) bypasses ramping and engages
// immediately — that's exactly the case where you want instant response.

void updateRamp() {
  if (presets[activePreset].fullCoarse) return;

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
// RELAY CONTROL
// =============================================================================

void relayOff() {
  digitalWrite(PIN_INC_RELAY, LOW);
  digitalWrite(PIN_DEC_RELAY, LOW);
  digitalWrite(PIN_INC_LED,   LOW);
  digitalWrite(PIN_DEC_LED,   LOW);
  relayState    = RELAY_OFF;
  relayOnTime_ms = 0;
}

// Time-proportioned relay drive within each 100ms control window.
// duty = 0.0 (off) to 1.0 (fully on throughout window).
// NOT audio-frequency PWM — relay contacts are rated for this.
void setRelay(RelayState direction, float duty) {
  if (direction == RELAY_OFF) { relayOff(); return; }

  // Safety: cut relay if it's been on too long without RPM change
  if (relayState == direction && relayOnTime_ms > 0) {
    if ((millis() - relayOnTime_ms) > MAX_RELAY_MS) {
      relayOff();
      setAlarm("RELAY TIMEOUT");
      return;
    }
  }

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

  if (relayState != direction) relayOnTime_ms = millis();
  relayState = direction;
}


// =============================================================================
// ALARM HANDLING
// =============================================================================

void setAlarm(const char* msg) {
  relayOff();
  currentMode  = MODE_ALARM;
  alarmMessage = String(msg);
  Serial.print("ALARM: "); Serial.println(msg);
}

void clearAlarm() {
  if (currentMode == MODE_ALARM) {
    currentMode  = MODE_MANUAL;
    alarmMessage = "";
  }
}


// =============================================================================
// CONTROL ALGORITHM
// =============================================================================

// Runs every 100ms. Tracks rampedSetpoint so preset transitions are smooth.
void runControlLoop() {
  if (currentMode != MODE_AUTO) { relayOff(); return; }

  // Safety checks first
  if (magSignalLost()) {
    relayOff(); setAlarm("NO MAG SIGNAL"); return;
  }
  if (actualRPM > 0 && (actualRPM < 400 || actualRPM > 3500)) {
    relayOff(); setAlarm("RPM OUT OF RANGE"); return;
  }

  // Full coarse — immediate, no ramping
  if (presets[activePreset].fullCoarse) {
    setRelay(RELAY_DEC, 1.0);
    return;
  }

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
// DISPLAY
// =============================================================================

void updateDisplay() {
  if (!displayOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (currentMode == MODE_ALARM) {
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("!! ALARM !!");
    display.setTextSize(1);
    display.setCursor(0, 22); display.println(alarmMessage);
    display.setCursor(0, 36); display.println("CYCLE AUTO SWITCH");
    display.setCursor(0, 46); display.println("TO CLEAR");
    if ((millis() / 500) % 2 == 0)
      display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);

  } else {
    // Row 1 (large): preset name + mode indicator
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print(presets[activePreset].name);
    display.setTextSize(1);
    String mStr = (currentMode == MODE_AUTO) ? "AUTO" : "MAN";
    display.setCursor(SCREEN_WIDTH - (mStr.length() * 6), 4);
    display.print(mStr);

    // Row 2 (small): preset label
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print(presets[activePreset].label);

    display.drawLine(0, 28, SCREEN_WIDTH, 28, SSD1306_WHITE);

    // Row 3: SET RPM
    display.setCursor(0, 32);
    display.print("SET:");
    if (presets[activePreset].fullCoarse) {
      display.print(" FULL COARSE");
    } else {
      char buf[14];
      snprintf(buf, sizeof(buf), "%5d RPM", (int)rampedSetpoint);
      display.print(buf);
      // Ramp-in-progress indicator
      float diff = targetSetpoint - rampedSetpoint;
      if (fabs(diff) > 5.0) {
        display.print(diff > 0 ? " >" : " <");
      }
    }

    // Row 4: ACT RPM
    display.setCursor(0, 44);
    display.print("ACT:");
    if (actualRPM < 10 || magSignalLost()) {
      display.print("  ---- RPM");
    } else {
      char buf[14];
      snprintf(buf, sizeof(buf), "%5d RPM", (int)actualRPM);
      display.print(buf);
    }

    // Row 5: status bar
    display.setCursor(0, 56);
    if (currentMode != MODE_AUTO) {
      display.print("MANUAL");
    } else if (magSignalLost()) {
      display.print("NO MAG SIGNAL");
    } else if (presets[activePreset].fullCoarse) {
      display.print("COARSE >>>>");
    } else {
      float error = rampedSetpoint - actualRPM;
      if (fabs(error) < DEADBAND_RPM) {
        // HOLD: show a stability bar
        display.print("HOLD");
        display.drawLine(28, 60, SCREEN_WIDTH - 2, 60, SSD1306_WHITE);
      } else {
        // Activity bar: width proportional to duty
        float duty = min(fabs(error) / PROP_ZONE_RPM, 1.0f);
        uint8_t barW = (uint8_t)(duty * (SCREEN_WIDTH - 22));
        display.fillRect(22, 56, barW, 8, SSD1306_WHITE);
        // Label inverts over the filled bar
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, 56);
        display.print(error > 0 ? "INC" : "DEC");
        display.setTextColor(SSD1306_WHITE);
      }
    }
  }

  display.display();
}


// =============================================================================
// EEPROM
// =============================================================================

void loadSettings() {
  if (EEPROM.read(EE_MAGIC) != EEPROM_MAGIC) {
    // First boot — write defaults
    EEPROM.write(EE_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EE_PRESET1, (uint16_t)PRESET_MAX);
    EEPROM.put(EE_PRESET2, (uint16_t)PRESET_CRUISE_CLIMB);
    EEPROM.put(EE_PRESET3, (uint16_t)PRESET_CRUISE);
    EEPROM.put(EE_PRESET4, (uint16_t)PRESET_ECONOMY);
    Serial.println("EEPROM: defaults written");
  } else {
    uint16_t p1, p2, p3, p4;
    EEPROM.get(EE_PRESET1, p1); presets[0].rpm = p1;
    EEPROM.get(EE_PRESET2, p2); presets[1].rpm = p2;
    EEPROM.get(EE_PRESET3, p3); presets[2].rpm = p3;
    EEPROM.get(EE_PRESET4, p4); presets[3].rpm = p4;
    Serial.println("EEPROM: settings loaded");
  }
}


// =============================================================================
// SERIAL DEBUG CONSOLE
// =============================================================================

void printStatus() {
  Serial.print("Mode=");    Serial.print(currentMode == MODE_AUTO ? "AUTO" : currentMode == MODE_ALARM ? "ALARM" : "MAN");
  Serial.print(" SW=");     Serial.print(presets[activePreset].name);
  Serial.print(" Target="); Serial.print((int)targetSetpoint);
  Serial.print(" Ramped="); Serial.print((int)rampedSetpoint);
  Serial.print(" ACT=");    Serial.print((int)actualRPM);
  Serial.print(" Err=");    Serial.print((int)(rampedSetpoint - actualRPM));
  Serial.print(" Relay=");
  Serial.println(relayState == RELAY_INC ? "INC" : relayState == RELAY_DEC ? "DEC" : "OFF");
}

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 's': printStatus(); break;
    case 'a': currentMode = MODE_AUTO;   Serial.println("-> AUTO"); break;
    case 'm': currentMode = MODE_MANUAL; relayOff(); Serial.println("-> MANUAL"); break;
    case 'c': clearAlarm(); Serial.println("Alarm cleared"); break;
    case '1': activePreset=0; targetSetpoint=presets[0].rpm; Serial.println("Sim SW->1 MAX"); break;
    case '2': activePreset=1; targetSetpoint=presets[1].rpm; Serial.println("Sim SW->2 CLB"); break;
    case '3': activePreset=2; targetSetpoint=presets[2].rpm; Serial.println("Sim SW->3 CRS"); break;
    case '4': activePreset=3; targetSetpoint=presets[3].rpm; Serial.println("Sim SW->4 ECO"); break;
    case '5': activePreset=4; Serial.println("Sim SW->5 LOW"); break;
    case '?':
      Serial.println("Commands: s=status a=auto m=manual c=clear 1-5=sim_switch ?=help");
      break;
  }
}


// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println("350A Digital Governor v2.0");
  Serial.print("PULSES_PER_REV="); Serial.println(PULSES_PER_REV);
  Serial.print("RAMP_RATE="); Serial.print(RAMP_RATE_RPM_PER_SEC); Serial.println(" RPM/sec");

  // Output pins
  pinMode(PIN_INC_RELAY, OUTPUT); digitalWrite(PIN_INC_RELAY, LOW);
  pinMode(PIN_DEC_RELAY, OUTPUT); digitalWrite(PIN_DEC_RELAY, LOW);
  pinMode(PIN_INC_LED,   OUTPUT); digitalWrite(PIN_INC_LED,   LOW);
  pinMode(PIN_DEC_LED,   OUTPUT); digitalWrite(PIN_DEC_LED,   LOW);

  // RPM input and all 5 switch pins with internal pull-ups
  pinMode(PIN_RPM_INPUT, INPUT_PULLUP);
  for (uint8_t i = 0; i < 5; i++) pinMode(SWITCH_PINS[i], INPUT_PULLUP);

  // Magneto pulse interrupt — falling edge on INT0 (pin 2)
  EIFR = (1 << INTF0);
  attachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT), []() {
    uint32_t now = micros();
    if (lastPulseTime_us > 0) pulseInterval_us = now - lastPulseTime_us;
    lastPulseTime_us = now;
    newPulse = true;
  }, FALLING);

  for (uint8_t i = 0; i < RPM_AVG_SAMPLES; i++) rpmBuffer[i] = 0;
  lastMagPulse_ms = millis(); // Prevent immediate MAG alarm at boot

  // OLED
  displayOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!displayOK) {
    Serial.println("OLED not found (try OLED_I2C_ADDR 0x3D)");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);  display.println("350A Governor v2.0");
    display.setCursor(0, 12); display.println("Rotary Switch Panel");
    display.setCursor(0, 30); display.println("Initializing...");
    display.display();
    delay(1200);
  }

  loadSettings();

  // Read initial switch position at boot (allow 200ms for contacts to settle)
  uint32_t t0 = millis();
  while ((millis() - t0) < 200) {
    int8_t pos = readSwitchRaw();
    if (pos >= 0) { activePreset = pos; lastValidPreset = pos; break; }
  }
  if (readSwitchRaw() < 0) { activePreset = 2; lastValidPreset = 2; } // Default CRUISE

  // Seed the ramp at the current preset — no movement at boot
  if (!presets[activePreset].fullCoarse) {
    rampedSetpoint = (float)presets[activePreset].rpm;
    targetSetpoint = rampedSetpoint;
  }

  lastRampTime   = millis();
  lastControlRun = millis();
  currentMode    = MODE_MANUAL;
  relayOff();

  // Hardware watchdog — 250ms timeout
  // wdt_reset() must be called at least every 250ms or MCU resets and
  // relays de-energize. This is the last-resort firmware hang protection.
  wdt_enable(WDTO_250MS);

  Serial.print("Boot: SW position "); Serial.print(activePreset + 1);
  Serial.print(" ("); Serial.print(presets[activePreset].name); Serial.println(")");
  Serial.println("Ready. Type ? for commands.");
}


// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  wdt_reset(); // Must be first — resets 250ms watchdog

  uint32_t now = millis();

  // Update RPM from ISR data
  if (newPulse) updateRPM();

  // Read rotary switch with debounce
  int8_t sw = readSwitch();
  if (sw != activePreset) {
    activePreset = sw;
    if (presets[activePreset].fullCoarse) {
      rampedSetpoint = 0; // Emergency — no ramp, engage immediately
    } else {
      targetSetpoint = (float)presets[activePreset].rpm;
      // rampedSetpoint continues from wherever it is now — ramp handles the rest
    }
    Serial.print("Switch -> "); Serial.println(presets[activePreset].name);
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

  // Mag signal loss — alarm in AUTO only
  if (currentMode == MODE_AUTO && magSignalLost()) {
    setAlarm("NO MAG SIGNAL");
  }

  // Serial status in AUTO
  static uint32_t lastStatus = 0;
  if (currentMode == MODE_AUTO && (now - lastStatus) >= 2000) {
    lastStatus = now;
    printStatus();
  }
}
