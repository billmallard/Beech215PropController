# Wiring Reference

Applies to hardware Rev 3.3 and firmware v3.0. For Rev 3.2 and earlier (rotary
switch panel) see the copy of this file in git history.

## 7-Pin Governor Cannon Connector (J1)

This is the aircraft-side connector. **The pinout and the function of every pin
are the same as the original 350A.** No aircraft wiring changes are required,
and the governor needs no other connection.

| J1 pin | Letter | Board net | Wire Color (typical) | Description |
|---|---|---|---|---|
| 1 | A | 12V_IN | Red | Aircraft bus through prop circuit breaker and the prop switch AUTO position |
| 2 | B | GND | Black/Green | Airframe ground |
| 3 | C | INC_RELAY | Varies | 12V output: energizes INC RPM relay (fine pitch / increase RPM) |
| 4 | D | DEC_RELAY | Varies | 12V output: energizes DEC RPM relay (coarse pitch / decrease RPM) |
| 5 | E | POT_HI | Varies | One end of the panel RPM control |
| 6 | F | GND | Varies | Other end of the panel RPM control (grounded on the board) |
| 7 | H | MAG_LEAD | White | Right magneto P-lead, through the 10kΩ isolating resistor in the harness |

**On the board, J1 is seven pins on an 8-position header: positions 1–6 and 8.**
Position 7 is left empty so the magneto pin (H) sits 5 mm from its neighbour.
The silkscreen reads 12V, GND, INC, DEC, POT, RTN, (gap), MAG.

**Pins E and F — the panel control.** The original panel control is a 10kΩ
linear control wired as a two-terminal rheostat. The original manual's check
(Service Note 1, step 5): under 100Ω between E and F with the knob full
clockwise, 10,000Ω full counter-clockwise, changing smoothly in between. Full
clockwise is the take-off (highest) RPM. The board reads that resistance; it
does not matter which of the two wires is on E and which is on F.

**Pin H — the magneto lead.** The isolating resistor is part of the aircraft
harness, at the magneto end, not inside the governor (original manual: pin H
reads 10,000Ω ±10% to ground). The board adds its own 10.2kΩ in series.

---

## Service Display Header (J2) — optional

J2 is a 4-pin header for an SSD1306 I2C display. It is a bench and
troubleshooting aid. **It is not part of the aircraft installation**; leave it
unpopulated or unplugged in the aircraft. The firmware checks for a display at
power-up and runs normally without one.

| Pin | Signal |
|---|---|
| 1 | +5V |
| 2 | GND |
| 3 | SCL |
| 4 | SDA |

Display modules do not agree on pin order. Check yours against this table
before plugging it in. The board has 4.7kΩ pull-ups on SCL and SDA.

---

## Arduino Nano Pin Assignment

| Nano Pin | Function | Direction | Notes |
|---|---|---|---|
| D2 | RPM_PULSE | Input | 6N137 optocoupler output; hardware interrupt INT0 |
| D6 | INC drive | Output | HIGH = INC relay on. NPN level shifter then PNP high-side switch |
| D7 | DEC drive | Output | HIGH = DEC relay on |
| D8 | INC_LED | Output | Red LED via 330Ω |
| D9 | DEC_LED | Output | Green LED via 330Ω |
| A3 | INC_SENSE | Analog in | INC relay output through a 33k/10k divider (output ÷ 4.3). Read back by the firmware to check the driver; report only |
| A6 | DEC_SENSE | Analog in | DEC relay output through a 33k/10k divider. Read back the same way |
| A7 | POT_SENSE | Analog in | Panel control. 10kΩ pull-up to 5V on pin E, 10kΩ series, 100nF |
| A4 | SDA | I2C | Optional display |
| A5 | SCL | I2C | Optional display |
| +5V | VCC5 | Power | From the LM2596 5V output (the +5V pin, not VIN) |
| GND | GND | Power | Two ground pins |
| D3, D4, D5, D10, D11, D12, D13, A0, A1, A2 | — | — | Unused. D3–D5, D10, D11 carried the rotary switch on Rev 3.2 |

**Pot reading.** `adc = 1023 × Rpot / (Rpot + 10k)`. A 0–10kΩ control reads
0–512. An open wire reads about 1023 and the firmware treats it as a fault. A
shorted wire reads 0, which is the same as full clockwise, as on the original.

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

The full fine and full coarse limit switches live in the prop hub and connect through the slip ring assembly. Their wires emerge from the rear of the prop hub. They are in series with the relay coils — when a limit is reached, the switch opens and the motor stops regardless of what the governor commands.

| Limit | Switch State (at limit) | Effect |
|---|---|---|
| Full Fine Pitch | Open | Prevents further INC RPM commands from running motor |
| Full Coarse Pitch | Open | Prevents further DEC RPM commands from running motor |

**After prop overhaul:** The limit switch wires are occasionally reconnected backwards at the prop during reinstallation. See original 350A Service Note 1, Step 8 for the diagnostic. Symptom: prop goes to coarse instead of fine when INC is commanded, or vice versa.

---

## Signal Levels

| Signal | Level | Notes |
|---|---|---|
| 12V supply | 11–15V | Aircraft bus. A 22V transient clamp and a 1A resettable fuse sit at the input |
| Magneto P-lead at pin H | Pulses, not a logic level | Grounded while the points are closed; rings to a high voltage in both polarities when they open. **Not yet measured on the aircraft** — see hardware_rev3_3/REV3_3_CHANGES.md |
| Relay drive (C, D) | 0V or bus voltage | Sourced from the bus through a 2N2907A. Full on for large errors; time-proportioned near the setpoint |
| 5V logic | 5.0V ±5% | Generated by the LM2596 module from the aircraft bus |
| Panel control (pin E) | 0–2.5V | 10kΩ pull-up to 5V against the 0–10kΩ control; 5V if the wire is open |
| I2C (SDA, SCL) | 0–5V | Only when a display is fitted |

---

## Pinout Cross-Reference: Original 350A vs. New Board

For IAs doing the 337 review, this table shows the equivalence of the external interface:

| Pin | Original function | New board | Same? |
|---|---|---|---|
| A | 12V supply input | 12V supply input | Yes |
| B | Airframe ground | Ground | Yes |
| C | INC RPM relay coil drive (+12V switched) | INC RPM relay coil drive (+12V switched) | Yes |
| D | DEC RPM relay coil drive (+12V switched) | DEC RPM relay coil drive (+12V switched) | Yes |
| E | Panel RPM control, end 1 | Panel RPM control, sensed | Yes — same control, same two wires |
| F | Panel RPM control, end 2 | Panel RPM control, return | Yes |
| H | Magneto P-lead sense (10kΩ isolator in harness) | Magneto P-lead sense (same harness isolator, plus 10.2kΩ on the board) | Yes |

No airframe wiring changes are needed or permitted without additional 337 documentation.

**Relay drive polarity.** The original 350A is a **high-side driver** — it switches +12V onto pins C and D to energize the relay coils, whose other ends return to ground through the pitch-limit switches. The new board does the same with one PNP transistor per channel. A ground-side driver such as the ULN2003A is not compatible with this wiring.

**What has not been compared.** The equivalence above is about the connector. The original's relay coil current, its magneto input sensitivity and its RPM-versus-knob-angle curve have not been measured, so the new board's behaviour on those three points is by design intent, not by test.
