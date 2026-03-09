# Wiring Reference

## 7-Pin Governor Cannon Connector (J1)

This is the aircraft-side connector. **Pinout is identical to the original 350A.** No aircraft wiring changes are required.

| Pin | Letter | Signal | Wire Color (typical) | Description |
|---|---|---|---|---|
| 1 | A | 12V_RAW | Red | Aircraft bus through prop circuit breaker and AUTO/OFF switch |
| 2 | B | GND | Black/Green | Airframe ground |
| 3 | C | INC_RELAY | Varies | 12V output: energizes INC RPM relay (fine pitch / increase RPM) |
| 4 | D | DEC_RELAY | Varies | 12V output: energizes DEC RPM relay (coarse pitch / decrease RPM) |
| 5 | E | POT_END_1 | Varies | One end of panel rheostat/potentiometer |
| 6 | F | POT_END_2 | Varies | Other end of panel rheostat/potentiometer |
| 7 | H | MAG_P_LEAD | White | Right magneto P-lead (through 10kΩ isolating resistor in governor) |

**Note on pins E and F:** In the original installation these are the two ends of the panel knob rheostat. In the new system, pin E is connected to one end of the new linear pot (tied to +5V through the PCB), pin F to the other end (tied to GND). The wiper is read by the Nano ADC on pin A0. You may also use E and F as +5V reference supply to the pot from the panel, with the wiper returned on the panel cable — either approach works.

---

## Panel Unit to PCB Cable (J2 and J3)

Run a shielded multi-conductor cable (minimum 15 conductors, or two 8-conductor cables) between the panel unit and the governor box.

### J2 — 10-pin Panel Connector

| Pin | J2 Position | Signal | Panel Component |
|---|---|---|---|
| 1 | 1 | +5V | Panel unit power supply |
| 2 | 2 | GND | Panel unit ground |
| 3 | 3 | SDA | OLED display I2C data |
| 4 | 4 | SCL | OLED display I2C clock |
| 5 | 5 | ENC_A | Rotary encoder Channel A |
| 6 | 6 | ENC_B | Rotary encoder Channel B |
| 7 | 7 | ENC_SW | Encoder pushbutton switch |
| 8 | 8 | BTN_MAX | Preset 1 pushbutton (MAX 2650) |
| 9 | 9 | BTN_CLIMB | Preset 2 pushbutton (CRUISE CLIMB 2450) |
| 10 | 10 | BTN_CRUISE | Preset 3 pushbutton (CRUISE 2300) |

### J3 — 5-pin Panel Connector

| Pin | J3 Position | Signal | Panel Component |
|---|---|---|---|
| 1 | 1 | BTN_ECO | Preset 4 pushbutton (ECONOMY 2200) |
| 2 | 2 | A0_WIPER | Panel pot wiper (continuous RPM control) |
| 3 | 3 | POT_E | One end of panel pot (+5V reference) |
| 4 | 4 | POT_F | Other end of panel pot (GND) |
| 5 | 5 | GND | Ground |

**Note:** BTN_LOW (Emergency/Coarse) is on Nano pin A1. Wire from J3 pin 1 or add a dedicated connector pin if your panel layout puts the button in a different position.

---

## Arduino Nano Pin Assignment

For reference when troubleshooting or modifying firmware:

| Nano Pin | Function | Direction | Notes |
|---|---|---|---|
| D2 | RPM_PULSE | Input | From 6N137 optocoupler output; hardware interrupt INT0 |
| D3 | ENC_SW | Input | Encoder pushbutton; internal pull-up |
| D4 | ENC_A | Input | Encoder channel A; internal pull-up |
| D5 | ENC_B | Input | Encoder channel B; internal pull-up |
| D6 | INC_RELAY_DRIVE | Output | To ULN2003A IN1; energizes INC relay |
| D7 | DEC_RELAY_DRIVE | Output | To ULN2003A IN2; energizes DEC relay |
| D8 | INC_LED | Output | Red LED via 330Ω; visual indicator |
| D9 | DEC_LED | Output | Green LED via 330Ω; visual indicator |
| D10 | BTN_MAX | Input | Preset 1 (MAX 2650); internal pull-up |
| D11 | BTN_CLIMB | Input | Preset 2 (CRUISE CLIMB 2450); internal pull-up |
| D12 | BTN_CRUISE | Input | Preset 3 (CRUISE 2300); internal pull-up |
| A0 | POT_WIPER | Analog Input | Panel potentiometer continuous control |
| A1 | BTN_ECO | Input | Preset 4 (ECONOMY 2200); internal pull-up |
| A4 | SDA | I2C | OLED display data |
| A5 | SCL | I2C | OLED display clock |
| VIN | 5V_INPUT | Power | From LM2596 5V output |
| GND | GND | Power | Multiple ground connections |

---

## Aircraft Wiring Variants (Early vs. Late Models)

The Bonanza had a wiring change at serial number D-1821. This affects how the prop motor relays are connected and whether the limit switches are in the control loop.

### Early Models (Serial Numbers through D-1820)

- The prop control switch energizes relays when moved to INC or DEC position
- Relays supply voltage to the pitch change motor
- Limit switches are in the relay coil circuit (in some configurations)
- The governor (original or replacement) commands the relays through the same switch

**Symptom if you have the early wiring:** If MANUAL operation works but AUTO does nothing, verify relay operation per the original 350A Service Note 1 (Airframe Systems Test).

### Later Models (Serial Numbers D-1821 and On)

- Panel switch supplies motor current directly (no relay for manual operation)
- Relays handle only the automatic (governor) function
- Simpler and more reliable for automatic operation
- Relay failure affects only AUTO mode, not MANUAL

**To determine which wiring you have:** With engine OFF, master ON, move prop switch to INC or DEC. If motor stops at pitch extremes (limit switches working), you likely have early wiring. If motor continues at pitch extremes making a clicking sound, you have late wiring or the limit switches are bypassed.

See the original 350A Service Note 9 (MANUAL WIRING) and Figures C and D in the original documentation for schematic details.

---

## Limit Switch Wiring

The full fine and full coarse limit switches live in the prop hub and connect through the slip ring assembly. Their wires emerge from the rear of the prop hub. They are in series with the relay contacts — when a limit is reached, the switch opens and the motor stops regardless of what the governor commands.

| Limit | Switch State (at limit) | Effect |
|---|---|---|
| Full Fine Pitch | Open | Prevents further INC RPM commands from running motor |
| Full Coarse Pitch | Open | Prevents further DEC RPM commands from running motor |

**After prop overhaul:** The limit switch wires are occasionally reconnected backwards at the prop during reinstallation. See original 350A Service Note 1, Step 8 for the diagnostic. Symptom: prop goes to coarse instead of fine when INC is commanded, or vice versa.

---

## Signal Levels

| Signal | Level | Notes |
|---|---|---|
| 12V supply | 11–15V | Aircraft battery/alternator bus; tolerant of normal variation |
| Magneto P-lead | 0–12V | Coil fires produce brief ground pulses; rest state is ~12V |
| Relay drive (C, D) | 0V or 12V | Bang-bang for large errors; time-proportioned in proportional zone |
| 5V logic (Nano, OLED, optocoupler) | 5.0V ±5% | Generated by LM2596 from aircraft bus |
| I2C bus (SDA, SCL) | 0–5V | 400kHz fast-mode; 4.7kΩ pull-ups on each line |
| ADC input (pot wiper) | 0–5V | Linear across 0–10kΩ pot sweep; 50-sample averaged |

---

## Pinout Cross-Reference: Original 350A vs. New Board

For IAs doing the 337 review, this table shows the complete equivalence of the external interface:

| Original 350A Pin | Original Function | New Board Pin | New Function | Equivalent? |
|---|---|---|---|---|
| A | 12V supply input | J1-A | 12V_RAW | ✓ Identical |
| B | Airframe ground | J1-B | GND | ✓ Identical |
| C | INC RPM relay coil drive | J1-C | INC_RELAY_DRIVE | ✓ Identical behavior |
| D | DEC RPM relay coil drive | J1-D | DEC_RELAY_DRIVE | ✓ Identical behavior |
| E | Rheostat end 1 | J1-E | POT reference | ✓ Functionally equivalent |
| F | Rheostat end 2 | J1-F | POT reference | ✓ Functionally equivalent |
| H | Magneto P-lead sense | J1-H | MAG_P_LEAD (through 10kΩ isolator) | ✓ Identical; isolator preserved |

The relay coil loads (current draw on C and D) are identical. The magneto input impedance is identical (10kΩ isolating resistor preserved in the new design). The power supply loading on pin A is lower (switching regulator vs. linear), which is a improvement. No airframe wiring changes are needed or permitted without additional 337 documentation.
