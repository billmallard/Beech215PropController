# Bench Test Procedure

**Applies to: hardware Rev 3.4 with firmware v3.1** (panel potentiometer input, series switch). Firmware v3.1 does not run correctly on earlier boards.

## Why Bench Test?

You want to find every firmware bug, wiring error, and marginal solder joint **before** the board is in the engine compartment at 8,500 feet. The bench test takes about 2 hours and will catch the majority of problems.

## What Changed in v3.0 and v3.1 (read this if you bench-tested an earlier version)

- **The governor boots straight into AUTO.** In the aircraft the prop switch only powers the box in the AUTO position, so power-up means "govern". There is no serial command to start it. Serial `m` (relays off) and `a` remain as bench overrides.
- **No magneto signal is a standby, not an alarm.** Like the original 350A, the governor takes no action without an RPM signal and resumes by itself when the signal returns. Relays are still de-energized within 2 seconds of the last pulse.
- **The setpoint comes only from the panel pot** (10k linear, two wires, J1 pins E and F). Full clockwise = `POT_RPM_MAX`, full counter-clockwise = `POT_RPM_MIN`. The rotary switch, the presets and the EMERGENCY/LOW position are gone.
- **An open pot circuit is a standby** (`POT OPEN`): relays off, resumes by itself when the pot reads valid again.
- **Whenever governing starts or resumes, the setpoint starts at the RPM the prop is actually turning** and then walks to the knob position at 100 RPM/sec. Nothing lurches at switch-on. **The ramp never makes the governor drive away from the knob setting:** if the RPM moves toward the knob setting faster than the ramp (throttle advanced, engine starting), `Ramped` simply rides the actual RPM and no relay is driven.
- **The OLED is optional.** With nothing on the J2 service header the unit runs identically; use the serial console for the bench.
- **Nothing latches.** An RPM reading below 400 is a third standby (`RPM LOW`): relays off while it lasts, and governing resumes by itself the moment the reading comes back up, straight against the knob setting with no ramp restart. **There is no upper RPM limit:** a high reading is governed like any other, by commanding a decrease. There is no alarm state and no serial `c` command any more (all of this changed 2026-11-15).
- **The outputs are read back, and a failed driver is cut off (v3.1, Rev 3.4).** The firmware measures both relay outputs. One that is dead while commanded on is reported (`DRIVER INC: NO_OUTPUT`). One that is **live while not commanded** means an output transistor has failed short: the firmware opens the series switch ahead of both drivers, prints `DRIVER TRIP`, and stays that way **until power is cycled**. This is the only latched state. See Test 13.
- **Fault lamp (optional, not fitted by default).** If J3 and its parts are fitted, the lamp lights for one second at power-up and steadily after a trip.
- **There is no limit on how long a relay may be commanded.** Like the original 350A, the governor holds its output until the RPM reaches the knob setting; in the aircraft the pitch limit switches stop the motor at the stops. On the bench the simulated RPM never responds to the relay, so the relay stays on for as long as you leave an RPM error in place. (An 8-second `RELAY TIMEOUT` alarm existed until 2026-11-15 and was removed.)

---

## What You Need

- Assembled governor PCB (firmware flashed, all components installed)
- A second Arduino Nano (the pulse generator — ~$5)
- A 470Ω resistor (pulse generator output)
- A 10kΩ **linear** potentiometer (or the aircraft's own panel control)
- 12V DC bench power supply, 2A minimum
- Your spare Beech 215-210 prop control motor
- A multimeter
- An oscilloscope (recommended, not mandatory)
- Short jumper wires and a small clip lead
- USB cable for Nano

---

## Part 1: Build the Pulse Generator

This second Nano replaces the magneto for bench testing. It outputs a square wave at a frequency that corresponds to a specific RPM, letting you dial in different simulated engine speeds.

### Flash the Pulse Generator

Create a new Arduino sketch and flash it to the SECOND (test) Nano:

```cpp
// Prop Governor Bench Test — Magneto Pulse Generator
// Simulates the magneto signal for bench testing
//
// Wiring: Pin 9 → 470Ω → MAG_FILT node on the governor PCB
//                        (banded/cathode lead of D6 = U1 pin 2)
//         GND → Governor PCB ground (J1 pin B)
//
// Serial commands (9600 baud):
//   1 = 1800 RPM (~90 Hz)
//   2 = 2200 RPM (~110 Hz)
//   3 = 2300 RPM (~115 Hz)
//   4 = 2450 RPM (~122 Hz)
//   5 = 2650 RPM (~133 Hz)
//   6 = 3600 RPM (over-range test)
//   0 =  300 RPM (under-range test)
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
    else if (c == '6') targetRPM = 3600;
    else if (c == '0') targetRPM = 300;
    else if (c == '+') targetRPM = min(targetRPM + 50, 3800);
    else if (c == '-') targetRPM = max(targetRPM - 50, 300);
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

### Where the pulse generator connects, and why it is not J1 pin H

The 6N137 needs roughly 5mA or more through its input LED to switch. On the board the magneto input passes through two series resistors (R1 + R9, 5.1kΩ each). A 5V Nano pin driving J1 pin H through those resistors delivers about 0.35mA — the optocoupler never switches and the governor shows `NO MAG SIGNAL`. (Earlier versions of this document said to inject through 1kΩ at pin H. That cannot work.)

For bench work, **bypass the series resistors**: connect the generator through **470Ω** directly to the **MAG_FILT** node, which is the banded (cathode) lead of D6 and also U1 pin 2. That gives about 7mA. Generator ground goes to board ground.

This means the bench test exercises everything from the optocoupler onward, but **not** R1/R9 or the J1 pin H wiring. Check those two resistors with an ohmmeter (J1 pin H to the D6 banded lead should read about 10.2kΩ). The complete input path is proven on the aircraft during the ground run, where the magneto supplies far more voltage than a bench generator.

**In the aircraft** the original 350A harness already has its own 10kΩ isolating resistor at the magneto end of the "H" lead (original manual, Service Note 1 step 4: pin H to airframe ground reads 10,000Ω ±10%). The on-board resistors are in addition to it.

---

## Part 2: Bench Harness Wiring

```
BENCH SUPPLY (12–14V, 2A)
  +12V ─────────────────────────── J1 Pin A (12V)
  GND ──────────────────────────── J1 Pin B (GND)

PANEL POT (10k linear — use two terminals: the wiper and ONE end)
  Wiper ────────────────────────── J1 Pin E (pot sense)
  One end ──────────────────────── J1 Pin F (board ground)
  Choose the end that gives LOW resistance at full clockwise.

PULSE GENERATOR NANO
  Pin 9 ──── 470Ω resistor ─────── D6 banded lead (MAG_FILT, U1 pin 2)
  GND ──────────────────────────── J1 Pin B (GND) [common with supply GND]

PROP MOTOR (wired like airframe relays)
  +12V ───── INC Relay COM ──────── Motor terminal J (via INC relay)
  +12V ───── DEC Relay COM ──────── Motor terminal J (via DEC relay)
  INC Relay NO ─────────────────── Motor terminal K (fine pitch)
  DEC Relay NO ─────────────────── Motor terminal K (coarse pitch)
  NOTE: Or connect J1 pins C and D to 12V indicator LEDs if you don't have
    external relays for the bench — this confirms governor outputs without
    running the motor. Motor test comes after relay test is confirmed.

SCOPE (if available)
  CH1 ──── D6 banded lead (view input pulse signal)
  CH2 ──── 6N137 output, U1 pin 6 (view cleaned signal after optocoupler)
  GND ──── J1 Pin B
```

**Check the pot with an ohmmeter before connecting it.** Across the two terminals you are using it should read under 100Ω at full clockwise and about 10kΩ at full counter-clockwise, changing smoothly in between. This is the same check the original 350A manual specifies for the aircraft's panel rheostat (pins E and F of the cable connector).

**Note on motor wiring for bench test:** The airframe relays that normally switch the motor aren't on this PCB — they live in the airframe. For bench testing, wire two small 12V SPDT relays externally. Connect relay coils to J1 pins C (INC) and D (DEC) with GND return. The relay contacts switch 12V to the motor in the correct direction for INC and DEC commands. The motor limit switches aren't needed for bench test unless you want to verify that behavior too.

### Watching the governor without a display

Open a serial terminal on the governor Nano at 9600 baud. Type `s` for a status line (it also prints every 2 seconds in AUTO):

```
Mode=AUTO Hold=- PotADC=341 Target=2225 Ramped=2225 ACT=2203 Err=22 Relay=OFF DrvInc=OK DrvDec=OK DrvSw=OK
```

| Field | Meaning |
|---|---|
| `Mode` | `AUTO` or `MAN` (bench override, relays off) |
| `Hold` | Why AUTO is not driving: `NO_MAG`, `RPM_LOW`, `POT_OPEN`, `DRV_TRIP`, or `-` when governing |
| `PotADC` | Filtered pot reading: 0 at full CW, about 341 at mid-travel, about 512 at full CCW, about 1023 with the pot disconnected |
| `Target` | RPM the knob is asking for |
| `Ramped` | Setpoint the control loop is tracking right now (walks toward `Target`) |
| `ACT` | Measured RPM |
| `Relay` | `INC`, `DEC` or `OFF` |
| `DrvInc`, `DrvDec` | Output readback for each relay driver: `OK`, `STUCK_ON` (output live while commanded off; trips the series switch) or `NO_OUTPUT` (output dead while commanded on; report only). See Test 13 |
| `DrvSw` | Readback of the series switch that feeds both drivers: `OK`, `NO_OUTPUT` (closed but dead) or `STUCK_ON` (live while open) |

---

## Part 3: Bench Test Sequence

Expected knob readings below assume the defaults `POT_RPM_MAX 2650` and `POT_RPM_MIN 1800`. If you have changed them, use: `Target = MAX − (MAX − MIN) × R / 10kΩ`.

### Test 1: Power-On State

1. Pulse generator disconnected or unpowered. Pot connected. Apply 12V.
2. Serial console shows the boot banner ending in `AUTO. Type ? for commands.`
3. Status line: `Mode=AUTO Hold=NO_MAG ... Relay=OFF DrvInc=OK DrvDec=OK DrvSw=OK`. If a fault lamp is fitted it lights for one second at power-up and goes out. With an OLED fitted the bottom line reads `NO MAG SIGNAL` and the mode indicator reads `AUTO`.
4. Leave it for 30 seconds. **No relay clicks, no alarm.** The governor must sit in this standby indefinitely — this is the engine-not-running case.
5. Current draw: 100–200mA at 12V (normal)

### Test 2: Magneto Signal Acquisition

1. Power on the pulse generator — set to 2300 RPM (command `3`)
2. Within about half a second `ACT` reads close to 2300 and `Hold` changes to `-`
3. If it stays at `NO_MAG`: check the 470Ω resistor and the clip on the D6 banded lead, check generator ground, check U1 is seated with pin 1 aligned
4. If RPM reads wildly wrong: verify `PULSES_PER_REV` matches in both sketches

**Using a scope:** CH1 on the D6 banded lead should show a square wave of about 1.5V (the optocoupler LED clamps it). CH2 on U1 pin 6 should show a clean, sharp 5V square wave at the same frequency. If CH2 is stuck high, check that U1 pin 7 (enable) is at 5V and that R2 (4.7kΩ pull-up) is installed.

### Test 3: Panel Pot Range

Generator at 2300 RPM. Read `Target` for each knob position:

| Knob position | Pot resistance | `PotADC` | `Target` (defaults) |
|---|---|---|---|
| Full clockwise | under 100Ω | 0–10 | 2650 (within ~10 RPM) |
| Quarter turn back | 2.5kΩ | ~205 | ~2440 |
| Mid-travel | 5kΩ | ~341 | ~2225 |
| Three-quarter | 7.5kΩ | ~438 | ~2010 |
| Full counter-clockwise | 10kΩ | ~512 | 1800 |

1. Sweep slowly stop to stop: `Target` must change smoothly and in one direction only — clockwise always raises RPM.
2. Leave the knob still anywhere in its travel for a minute: `Target` must not change at all.
3. If clockwise *lowers* the target, the wrong end of the pot is connected — move the pin F wire to the other end terminal.
4. A pot that reads a little over 10kΩ at the CCW stop still gives exactly `POT_RPM_MIN`; that is normal.

### Test 4: RPM Error Response

1. Generator at 2300 RPM. Turn the knob until `Target` reads 2300 (±10). Wait for `Ramped` to arrive there. `Relay=OFF` — the governor is in its deadband.
2. Command the pulse generator to 2200 RPM (command `2`). Expected:
   - `Relay=INC` (RPM too low, commanding increase); red LED D1 lights
   - INC relay clicks on (indicator LED lights if using LEDs)
   - If motor connected: prop moves toward fine pitch
3. Command the generator back to 2300 RPM — the relay releases.
4. Command the generator to 2400 RPM (`3`, then `+` twice). Expected: `Relay=DEC`, green LED D2, DEC relay clicks on.
5. Command back to 2300 — relays off.
6. Now repeat step 2 and leave it for a full minute. The relay must stay on the whole time and the status must stay `AUTO`: the governor holds its output until the RPM gets there, as the original does. Command back to 2300 — relay off.

### Test 5: Proportional Control Verification

This test verifies the proportional zone is working (motor slows near setpoint).

1. Knob set for `Target` 2300, generator at 2300 RPM
2. Command the generator to 1800 RPM (large error) — INC at full duty immediately
3. Step RPM back up with `+`, not lingering more than a few seconds at each step: 2000, 2100, 2200, 2250
4. Within 75 RPM of the setpoint, the relay should start clicking ON and OFF in a pulse pattern (proportional mode)
5. Within the 25 RPM deadband, relay activity should stop completely

If you have a scope, put it on J1 pin C (INC relay drive). You should see:
- Steady HIGH during large error (full duty)
- Pulsing HIGH/LOW as you enter the proportional zone
- Steady LOW in the deadband

### Test 6: Knob Movement and Setpoint Ramp

1. Generator at 2300 RPM, knob set for `Target` 2300, relays off.
2. Turn the knob quickly to full clockwise. `Target` jumps to 2650 at once; `Ramped` climbs from 2300 at 100 RPM/sec (about 3.5 seconds). With an OLED fitted a `>` shows next to the KNOB line while ramping.
3. The INC relay comes on as `Ramped` passes 2325, pulsing at first and reaching full duty by about 2375. It does **not** slam on the instant the knob moves.
4. As soon as `Ramped` reaches 2650, step the generator up to 2650 (command `5`) — relays off.
5. Turn the knob quickly back until `Target` reads about 2300. `Ramped` walks *down* from 2650; DEC comes on progressively as it passes 2625. When `Ramped` arrives, command the generator to 2300 (command `3`) — relays off.
6. **Ramp rate check (optional):** send `m` (relays off), turn the knob from full CCW to full CW, and watch `Ramped` by sending `s` repeatedly: 850 RPM should take 8 to 9 seconds. Send `a` when done.
7. **The ramp never drives away from the knob.** Generator at 1800 RPM, knob full clockwise (`Target` 2650). Unplug the generator for 3 seconds and plug it back in, so that `Ramped` restarts at 1800 and begins to climb. While `Ramped` is still below 2200, step the generator to 2300 (command `3`). Expected: the INC relay drops at once and **the DEC relay never comes on**; `Ramped` jumps to about 2300 and carries on climbing, and INC returns within a second.
8. **RPM already past the knob.** Generator at 2300, knob full counter-clockwise (`Target` 1800), and wait for `Ramped` to start walking down. Step the generator to 1700. Expected: `Relay=INC` at once and `Ramped` reads 1800 — the governor works against the knob setting, not against the lagging ramp.

### Test 7: Safety — Pot Open Circuit

1. Governing normally (generator 2300, knob for `Target` 2300, then generator to 2200 so the INC relay is on).
2. Pull the wire off J1 pin E (or pin F).
3. Within about half a second the relay drops, status shows `Hold=POT_OPEN`, and the OLED (if fitted) shows `POT OPEN`. `Mode` stays `AUTO` — this is a standby, not a latched alarm. `Target` holds its last good value.
4. Leave it 30 seconds: no relay activity.
5. Reconnect the wire. Within about half a second `Hold` returns to `-`, and `Ramped` restarts at the actual RPM (2200) and walks to the knob setting — no step.
6. Short pins E and F together with a jumper: `PotADC` reads 0 and `Target` reads `POT_RPM_MAX`. A shorted pot circuit is indistinguishable from full clockwise and is treated as full clockwise, the same as the original 350A.

### Test 8: Safety — Magneto Signal Loss

1. Governing normally with a relay on (as in Test 7 step 1).
2. Unplug the pulse generator output (simulates magneto failure or a broken sense lead).
3. **Within 2 seconds** both relays de-energize. Status shows `Hold=NO_MAG`, `Mode=AUTO`. No alarm.
4. Leave it 30 seconds: no relay activity.
5. Reconnect the generator. Within about half a second governing resumes by itself, with `Ramped` restarting from the actual RPM.
6. Repeat with the generator set to a different RPM before reconnecting, and confirm `Hold` never shows `RPM_LOW` on reconnection.

### Test 9: Safety — Very High and Very Low RPM

1. Governing normally with `Target` at 2300. Command the generator to 3600 RPM (command `6`).
2. **`Relay=DEC` and it stays on.** There is no upper RPM limit: like the original, the governor answers any high reading by commanding a decrease, which is what an overspeed needs. `Hold=-`, `Mode=AUTO`.
3. Command the generator back to 2300 — relays off.
4. Command 300 RPM (command `0`). Relays drop. Status shows `Hold=RPM_LOW`, `Mode=AUTO`. Leave it 30 seconds: no relay activity. Below 400 RPM the engine is not running in any useful sense and the governor takes no action.
5. Command 450 RPM. **At once**, with nothing sent on the console, `Hold=-` and `Relay=INC` (toward fine pitch, as for an engine being restarted in flight). `Ramped` must still read 2300 — this standby does not restart the ramp.

### Test 10: Running Without a Display

1. Remove 12V. Unplug the OLED from J2 (if fitted). Reapply 12V.
2. Serial banner includes `No OLED (optional) - running without display`.
3. Repeat Tests 2 and 4. Behaviour must be identical. This is the configuration the aircraft flies in.

### Test 11: Watchdog Test (Optional)

This requires a modified firmware with a deliberate infinite loop triggered by a serial command. The watchdog should reset the MCU within 250ms; the relays drop during the reset and the governor comes back up in its normal power-on state (AUTO, standby until RPM is valid again, ramp restarted from actual RPM). Skip this test if you don't want to modify firmware — the watchdog is a hardware feature of the ATmega328P that doesn't require verification.

### Test 12: Motor Physical Test

If you have external relays wired to the motor:

1. Knob set for `Target` 2300; command the generator well below it (1800 RPM)
2. Motor should run in the INC direction (fine pitch / more RPM)
3. Time the full sweep from one pitch stop to the other — with correct gears it should take **16–32 seconds**. The governor keeps the relay on for as long as the RPM error is there, so **if your bench rig has no pitch limit switches, remove the error (step 6) before the motor reaches the stop**. In the aircraft the limit switch stops the motor
4. **If sweep takes 6–12 seconds**: WRONG GEARS. Do not fly. See original 350A Service Note 3.
5. Command the generator well above the setpoint (2650) — motor should reverse cleanly
6. Verify motor stops when you remove the RPM error (bring simulated RPM to setpoint)

---

### Test 13: Output Readback and the Series Switch

The board divides each relay output, and the switched supply that feeds both drivers, down to a Nano analog pin (A3 for INC, A6 for DEC, A2 for the switch). The firmware checks that each is on when commanded on and off when commanded off.

1. **Healthy board.** Through Tests 4 to 6 every status line must end `DrvInc=OK DrvDec=OK DrvSw=OK`, and no `DRIVER` line may appear on the console. This holds with relays, indicator LEDs or nothing at all connected to pins C and D.
2. **Series switch.** With the governor powered and idle, J1 pin A to the Q1 emitter pad should read under 0.5V (switch closed). Hold the Nano in reset (press and hold its reset button): the same reading rises to the full supply voltage, because the switch opens whenever the Nano is not running. Release the button.
3. **Output live when it should be off: the trip.** Generator at the knob setting, `Relay=OFF`. Put a jumper from J1 pin A to J1 pin C (the same jumper the original manual uses for its airframe test). The INC relay or indicator comes on, fed by the jumper. Within half a second the console prints `DRIVER INC: STUCK_ON` and `DRIVER TRIP`, and the status line shows `Hold=DRV_TRIP DrvInc=STUCK_ON`. A fitted fault lamp lights.
4. **The trip stays.** Remove the jumper. Command the generator 300 RPM above and then 300 RPM below the knob setting: **neither relay operates**, and `Hold=DRV_TRIP` remains. Wait a full minute; nothing changes.
5. **Power cycle clears it.** Remove and re-apply 12V. The governor starts normally, `DrvInc=OK`, and governs again.
6. Repeat steps 3 to 5 from pin A to pin D: `DRIVER DEC: STUCK_ON`, `DRIVER TRIP`.
7. **Trip while the other relay is driven.** Command the generator 100 RPM below the knob setting so `Relay=INC`, then fit the jumper from pin A to pin D. The governor must trip and the INC relay must drop out with it.
8. **Output dead when it should be on.** After a power cycle, clip a lead from Nano pin A3 to GND (this only grounds the sense input, through 33kΩ; it is harmless). Command the generator 100 RPM below the knob setting so `Relay=INC`. Within half a second: `DRIVER INC: NO_OUTPUT`. This is a report only: the INC relay still operates normally and there is no trip. Remove the lead while INC is still commanded: `DRIVER INC: OK`.
9. A fault that appears with nothing unusual connected means a driver transistor (Q1 to Q6), its resistors, or a divider (R14 to R17, R26, R27) is wrong. `DrvSw=NO_OUTPUT` together with `NO_OUTPUT` on a commanded relay means the series switch is not closing.

With motor relays connected, the jumper in steps 3, 6 and 7 runs the motor for as long as it is fitted. Fit it briefly.

## Part 4: Scope Reference Waveforms

If you have an oscilloscope, these are the waveforms to expect at key test points:

| Test point | Normal waveform |
|---|---|
| D6 banded lead (MAG_FILT, bench injection point) | Square wave about 1.5V high, 90–133 Hz at normal RPM |
| 6N137 output (U1 pin 6) | Clean square wave, same frequency, sharp edges, 5V amplitude |
| J1 pin C or D (relay drive) | 0V in deadband; 12V steady for large errors; pulsed 12V in proportional zone |
| J1 pin E (pot sense) | DC: 0V at full CW, about 1.67V at mid-travel, about 2.5V at full CCW, 5V with the pot disconnected |
| Nano 5V pin | 5.0V ±0.1V, minimal ripple |
| 12V supply | Should not sag more than 0.5V when relay energizes |

---

## Common Bench Test Problems

**OLED doesn't light up (if one is fitted):**
- The display is detected once at power-up — plug it in before applying 12V
- Check I2C address (0x3C vs 0x3D) — modify `OLED_I2C_ADDR` in firmware and reflash
- Check the J2 service header wiring against the board silkscreen (5V, GND, SCL, SDA)
- Verify 5V is reaching the OLED VCC pin

**RPM reads zero (`Hold=NO_MAG`) or wildly fluctuating:**
- Verify the generator goes through 470Ω to the D6 banded lead, **not** to J1 pin H
- Check optocoupler is seated correctly in socket, pin 1 aligned
- Measure voltage at U1 pin 8 (Vcc) — should be 5V. Pin 5 is ground.
- Measure U1 pin 7 (enable) — should be 5V
- Verify R2 (4.7kΩ pull-up) is installed

**`Hold=POT_OPEN` with the pot connected:**
- Measure across J1 pins E and F with the board unpowered: it should read the pot resistance (0–10kΩ)
- Measure J1 pin E to ground with the board powered: 0–2.5V is valid, 5V means the pot circuit is open
- Check R18 (10kΩ pull-up) and R19 (10kΩ series) are fitted

**Relays won't energize:**
- Check J1 pins C and D for 12V output when RPM error is present (multimeter)
- If 12V present but relay doesn't click: relay coil circuit issue, not the board's problem
- Check transistors Q1–Q4: verify flat-body orientation matches silkscreen, inspect solder joints on all three leads of each device

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
