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

**Note on pins E and F:** In the original installation these are the two ends of the panel rheostat. In the rotary switch implementation, pin E carries GND to the panel (switch COMMON). Pin F is spare. The 10-wire J2 panel cable handles all panel unit connections directly without needing J1 pins E/F for signal routing.

---

## Panel Unit to PCB Cable (J2 — single connector)

A single 10-conductor shielded cable connects the panel unit to the governor box. The rotary switch replaces the original pot, 5 buttons, and encoder — **10 wires does the whole job.**

### J2 — 10-pin Panel Connector

| Pin | Signal | Panel Connection |
|---|---|---|
| 1 | +5V | OLED VCC |
| 2 | GND | OLED GND, switch common (both tie here) |
| 3 | SDA | OLED I2C data |
| 4 | SCL | OLED I2C clock |
| 5 | SW_POS1 | Switch position 1 terminal (MAX 2650) |
| 6 | SW_POS2 | Switch position 2 terminal (CRUISE CLIMB 2450) |
| 7 | SW_POS3 | Switch position 3 terminal (CRUISE 2300) |
| 8 | SW_POS4 | Switch position 4 terminal (ECONOMY 2200) |
| 9 | SW_POS5 | Switch position 5 terminal (LOW/EMERGENCY COARSE) |
| 10 | SW_COMMON | Switch COMMON terminal (ties to GND at panel or at PCB) |

**Switch wiring principle:** The rotary switch COMMON terminal is connected to GND (pin 10 / pin 2). Each position terminal goes to its respective signal wire (pins 5–9). Internal pull-ups in the Nano keep all signal lines HIGH; whichever position is selected pulls its line LOW through the switch. No external resistors needed anywhere in the switch circuit.

**Note on SW_COMMON:** Pin 10 and Pin 2 are both GND — you can tie the switch common directly to pin 2 at the panel connector if that's easier, and leave pin 10 unused. The separate pin 10 just makes the wiring cleaner.

---

## Arduino Nano Pin Assignment

| Nano Pin | Function | Direction | Notes |
|---|---|---|---|
| D2 | RPM_PULSE | Input | 6N137 optocoupler output; hardware interrupt INT0 |
| D3 | SW_POS1 | Input | Rotary switch position 1 (MAX 2650); internal pull-up |
| D4 | SW_POS2 | Input | Rotary switch position 2 (CRUISE CLIMB 2450); internal pull-up |
| D5 | SW_POS3 | Input | Rotary switch position 3 (CRUISE 2300); internal pull-up |
| D6 | INC_RELAY_DRIVE | Output | To ULN2003A IN1; energizes INC relay |
| D7 | DEC_RELAY_DRIVE | Output | To ULN2003A IN2; energizes DEC relay |
| D8 | INC_LED | Output | Red LED via 330Ω; INC activity indicator |
| D9 | DEC_LED | Output | Green LED via 330Ω; DEC activity indicator |
| D10 | SW_POS4 | Input | Rotary switch position 4 (ECONOMY 2200); internal pull-up |
| D11 | SW_POS5 | Input | Rotary switch position 5 (LOW/EMERGENCY COARSE); internal pull-up |
| D12 | (spare) | — | Available for future use |
| A0 | (spare) | — | Available for future use |
| A1 | (spare) | — | Available for future use |
| A4 | SDA | I2C | OLED display data (Wire library) |
| A5 | SCL | I2C | OLED display clock (Wire library) |
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
| E | Rheostat end 1 | J1-E | SW_COMMON (GND) | ✓ Functionally equivalent — switch common replaces rheostat end; GND is carried to panel |
| F | Rheostat end 2 | J1-F | Spare / NC | ✓ Pin present; unused in rotary switch implementation |
| H | Magneto P-lead sense | J1-H | MAG_P_LEAD (through 10kΩ isolator) | ✓ Identical; isolator preserved |

The relay coil loads (current draw on C and D) are identical. The magneto input impedance is identical (10kΩ isolating resistor preserved in the new design). The power supply loading on pin A is lower (switching regulator vs. linear), which is a improvement. No airframe wiring changes are needed or permitted without additional 337 documentation.
