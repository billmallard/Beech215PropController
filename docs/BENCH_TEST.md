# Bench Test Procedure

## Why Bench Test?

You want to find every firmware bug, wiring error, and marginal solder joint **before** the board is in the engine compartment at 8,500 feet. The bench test takes about 2 hours and will catch the majority of problems.

---

## What You Need

- Assembled governor PCB (firmware flashed, all components installed)
- A second Arduino Nano (the pulse generator — ~$5)
- 12V DC bench power supply, 2A minimum
- Your spare Beech 215-210 prop control motor
- A multimeter
- An oscilloscope (recommended, not mandatory)
- Short jumper wires
- USB cable for Nano

---

## Part 1: Build the Pulse Generator

This second Nano replaces the magneto for bench testing. It outputs a square wave at a frequency that corresponds to a specific RPM, letting you dial in different simulated engine speeds.

### Flash the Pulse Generator

Create a new Arduino sketch and flash it to the SECOND (test) Nano:

```cpp
// Prop Governor Bench Test — Magneto Pulse Generator
// Simulates magneto P-lead signal for ground testing
//
// Wiring: Pin 9 → 1kΩ → Governor PCB J1 pin H (magneto input)
//         GND → Governor PCB J1 pin B (common ground)
//
// Serial commands (9600 baud):
//   1 = 1800 RPM (idle, ~90 Hz)
//   2 = 2200 RPM (economy, ~110 Hz)
//   3 = 2300 RPM (cruise, ~115 Hz)
//   4 = 2450 RPM (cruise climb, ~122 Hz)
//   5 = 2650 RPM (max, ~133 Hz)
//   +/- = adjust RPM by ±50

const int OUT_PIN = 9;
const int PULSES_PER_REV = 3;   // Match firmware setting

int targetRPM = 2300;

void setup() {
  Serial.begin(9600);
  pinMode(OUT_PIN, OUTPUT);
  Serial.println("Pulse Generator Ready");
  Serial.print("Output: "); Serial.print(rpmToHz(targetRPM)); Serial.println(" Hz");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') targetRPM = 1800;
    else if (c == '2') targetRPM = 2200;
    else if (c == '3') targetRPM = 2300;
    else if (c == '4') targetRPM = 2450;
    else if (c == '5') targetRPM = 2650;
    else if (c == '+') targetRPM = min(targetRPM + 50, 3000);
    else if (c == '-') targetRPM = max(targetRPM - 50, 600);
    Serial.print("RPM: "); Serial.print(targetRPM);
    Serial.print(" ("); Serial.print(rpmToHz(targetRPM)); Serial.println(" Hz)");
  }

  // Generate square wave at target frequency
  float hz = rpmToHz(targetRPM);
  long halfPeriodUs = (long)(500000.0 / hz);
  digitalWrite(OUT_PIN, HIGH);
  delayMicroseconds(halfPeriodUs);
  digitalWrite(OUT_PIN, LOW);
  delayMicroseconds(halfPeriodUs);
}

float rpmToHz(int rpm) {
  return (rpm * PULSES_PER_REV) / 60.0;
}
```

---

## Part 2: Bench Harness Wiring

```
BENCH SUPPLY (12–14V, 2A)
  +12V ─────────────────────────── J1 Pin A (12V_RAW)
  GND ──────────────────────────── J1 Pin B (GND)

PULSE GENERATOR NANO
  Pin 9 ──── 1kΩ resistor ──────── J1 Pin H (MAG input)
  GND ──────────────────────────── J1 Pin B (GND) [common with supply GND]

PROP MOTOR (wired like airframe relays)
  +12V ───── INC Relay COM ──────── Motor terminal J (via INC relay)
  +12V ───── DEC Relay COM ──────── Motor terminal J (via DEC relay)
  INC Relay NO ─────────────────── Motor terminal K (fine pitch)
  DEC Relay NO ─────────────────── Motor terminal K (coarse pitch)
  ⚠ Or connect J1 pins C and D to 12V indicator LEDs if you don't have
    external relays for the bench — this confirms governor outputs without
    running the motor. Motor test comes after relay test is confirmed.

SCOPE (if available)
  CH1 ──── J1 Pin H (view input pulse signal)
  CH2 ──── 6N137 output pin (view cleaned signal after optocoupler)
  GND ──── J1 Pin B
```

**Note on motor wiring for bench test:** The airframe relays that normally switch the motor aren't on this PCB — they live in the airframe. For bench testing, wire two small 12V SPDT relays externally. Connect relay coils to J1 pins C (INC) and D (DEC) with GND return. The relay contacts switch 12V to the motor in the correct direction for INC and DEC commands. The motor limit switches aren't needed for bench test unless you want to verify that behavior too.

---

## Part 3: Bench Test Sequence

### Test 1: Power-On State

1. Connect bench supply, do NOT engage AUTO yet
2. OLED should display:
   ```
   MANUAL
   SET: 2300 RPM
   ACT: ---- RPM
   NO MAG SIGNAL
   ```
3. Current draw: 100–200mA at 12V (normal)
4. No relay clicks should occur

### Test 2: Magneto Signal Acquisition

1. Power on pulse generator Nano — set to 2300 RPM (command `3`)
2. Governor OLED should now show actual RPM reading near 2300
3. If it shows `----` or `NO MAG SIGNAL`: check J1 pin H wiring, check 1kΩ resistor
4. If RPM reads wildly wrong: verify `PULSES_PER_REV` matches in both sketches

**Using a scope:** CH1 on J1 pin H should show the raw square wave (may have some ringing). CH2 on the 6N137 output should show a clean, sharp square wave at the same frequency. If CH2 is noisy or has missed pulses, check C1 (100nF bypass) and the 4.7kΩ pull-up (R2).

### Test 3: AUTO Mode Engagement

1. Pulse generator running at 2300 RPM
2. Press the CRUISE preset button (or verify default)
3. Engage AUTO via the panel switch
4. OLED should show:
   ```
   CRUISE        AUTO
   SET: 2300 RPM
   ACT: 2300 RPM
   HOLD ─────────
   ```
5. No relay activity — RPM matches setpoint, governor in deadband

### Test 4: RPM Error Response

1. While in AUTO mode, command pulse generator to 2200 RPM (command `2`)
2. Expected:
   - OLED shows `INC▲` status (RPM too low, commanding increase)
   - INC relay clicks on (indicator LED lights if using LEDs)
   - If motor connected: prop moves toward fine pitch
3. Command pulse generator back to 2300 RPM
4. Motor stops, OLED shows `HOLD`

5. Command pulse generator to 2400 RPM (use `+` to step up)
6. Expected:
   - OLED shows `DEC▼` (RPM too high, commanding decrease)
   - DEC relay clicks on
7. Command back to 2300 — HOLD state returns

### Test 5: Proportional Control Verification

This test verifies the proportional zone is working (motor slows near setpoint).

1. Command pulse generator to 2300 RPM (at setpoint)
2. Quickly command to 1800 RPM (large error)
3. Governor should drive INC at full duty immediately
4. Slowly step RPM back up: 2000, 2100, 2200, 2250, 2275, 2295
5. As RPM approaches within 75 RPM of setpoint, the relay should start clicking ON and OFF in a pulse pattern (proportional mode)
6. Within the 25 RPM deadband, relay activity should stop completely

If you have a scope, put it on J1 pin C (INC relay drive). You should see:
- Steady HIGH during large error (full duty)
- Pulsing HIGH/LOW as you enter the proportional zone
- Steady LOW in the deadband

### Test 6: Preset Buttons

In AUTO mode at 2300 RPM:

1. Press MAX button — setpoint should jump to 2650, INC relay drives prop toward fine
2. Press ECO button — setpoint drops to 2200, DEC relay activates
3. Press CRS button — returns to 2300
4. Encoder CW: setpoint increments +10 RPM per detent
5. Encoder CCW: setpoint decrements −10 RPM per detent
6. Encoder push: should reset trim to zero (if implemented) or confirm preset

### Test 7: Emergency / Low RPM Preset

1. In AUTO mode, press LOW (Emergency) button
2. Governor should command full DEC (coarse pitch direction) continuously
3. The motor should run toward full coarse pitch limit
4. When the full coarse limit switch opens (if wired) or after MAX_RELAY_MS timeout, relay should stop
5. OLED should show `LOW / COARSE`

### Test 8: Safety — Magneto Signal Loss

1. In AUTO mode, stable RPM
2. Unplug the pulse generator (simulates magneto failure or broken P-lead)
3. Within 2 seconds, governor should:
   - De-energize both relays
   - Display `ALARM: NO MAG SIGNAL`
   - Revert to MANUAL mode indication
4. Reconnect pulse generator — alarm should clear only after cycling the AUTO switch

### Test 9: Watchdog Test (Optional)

This requires a modified firmware with a deliberate infinite loop triggered by a serial command. The watchdog should reset the MCU within 250ms and return to power-on MANUAL state. Skip this test if you don't want to modify firmware — the watchdog is a hardware feature of the ATmega328P that doesn't require verification.

### Test 10: Motor Physical Test

If you have external relays wired to the motor:

1. Command RPM well below setpoint (1800 RPM target, 2300 setpoint)
2. Motor should run in the INC direction (fine pitch / more RPM)
3. Time the motor running — with correct gears, full sweep should take **16–32 seconds**
4. **If sweep takes 6–12 seconds**: WRONG GEARS. Do not fly. See original 350A Service Note 3.
5. Manually activate DEC relay — motor should reverse cleanly
6. Verify motor stops when you remove the RPM error (bring simulated RPM to setpoint)

---

## Part 4: Scope Reference Waveforms

If you have an oscilloscope, these are the waveforms to expect at key test points:

| Test point | Normal waveform |
|---|---|
| J1 pin H (magneto input) | Square wave, 90–133 Hz at normal RPM, may have ringing on edges |
| 6N137 output (pin 6) | Clean square wave, same frequency, sharp edges, 5V amplitude |
| J1 pin C or D (relay drive) | 0V in deadband; 12V steady for large errors; pulsed 12V in proportional zone |
| Nano VIN pin | 5.0V ±0.1V, minimal ripple |
| 12V supply | Should not sag more than 0.5V when relay energizes |

---

## Common Bench Test Problems

**OLED doesn't light up:**
- Check I2C address (0x3C vs 0x3D) — modify `OLED_I2C_ADDR` in firmware and reflash
- Check J2 connector wiring (SDA/SCL to Nano A4/A5)
- Verify 5V is reaching the OLED VCC pin

**RPM reads zero or wildly fluctuating:**
- Verify 1kΩ series resistor is in line between pulse generator and J1 pin H
- Check optocoupler is seated correctly in socket, pin 1 aligned
- Measure voltage at U1 pin 5 (Vcc) — should be 5V
- Verify R2 (4.7kΩ pull-up) is installed

**Relays won't energize:**
- Check J1 pins C and D for 12V output when RPM error is present (multimeter)
- If 12V present but relay doesn't click: relay coil circuit issue, not the board's problem
- Verify ULN2003A is seated correctly

**Motor runs in wrong direction:**
- INC and DEC relay outputs are swapped
- In the aircraft: this appears as prop going to coarse when it should go fine, or vice versa
- Fix: swap J1 pins C and D connections, or swap the motor lead polarity

**Governor keeps oscillating on the bench (expected):**
- Per the original 350A manual: *"servo dynamics are designed for best performance in flight, not on the ground."*
- Ground testing at low RPM with no aerodynamic load on the prop will show more oscillation than flight
- Do your tuning in flight, not on the bench

---

*After passing all bench tests, proceed to aircraft installation per [ASSEMBLY.md](ASSEMBLY.md) Section 8.*
