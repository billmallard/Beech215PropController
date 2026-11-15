# Bill of Materials — Rev 3.4

Complete parts list for one governor build. All parts available from Digi-Key, Mouser, or Amazon.
Links are to Digi-Key where possible — they're reliable, stock everything, and ship fast.

**Estimated total cost: $50–90** depending on sourcing (Amazon vs. Digi-Key, bulk vs. singles)

Rev 3.4 reads the original 350A panel control (10k, connector pins E and F), so there is no panel unit to buy or build. What changed from Rev 3.3 and why: [../REV3_4_CHANGES.md](../REV3_4_CHANGES.md). Rev 3.4 adds the driver supply switch (Q5, Q6, R22–R27), which is fitted on every board, and an optional fault-lamp output (J3, Q7, R28–R31), which is **not** fitted by default and has its own section below.

---

## PCB

| Item | Specification | Qty | Source | Est. Cost | Notes |
|---|---|---|---|---|---|
| Governor PCB | 100×80mm 2-layer FR4 | 5 | JLCPCB | ~$2 + shipping | Upload `gerbers/350A_governor_JLCPCB.zip`; default settings; you get 5, use 1 |

**JLCPCB order settings:**
- Base Material: FR4
- Layers: 2
- PCB Thickness: 1.6mm
- Surface Finish: HASL (with lead) — cheapest and fine for this application
- Copper Weight: 1oz
- Everything else: defaults

---

## ICs and Active Components

| Ref | Part | Description | Digi-Key PN | Qty | Est. Cost | Notes |
|---|---|---|---|---|---|---|
| A1 | Arduino Nano (ATmega328P) | Microcontroller | — | 1 | $4–8 | Buy the CH340 version (most common on Amazon/eBay). Avoid Nano Every — different pinout. |
| U1 | 6N137 | High-speed optocoupler | 516-2878-5-ND | 1 | $0.60 | DIP-8. Do not substitute 4N25/4N35 — too slow for mag pulses. |
| PS1 | LM2596S-5V Module | DC-DC 5V switching regulator | — | 1 | $1–3 | Pre-built module. **Must be labeled "5V" not adjustable.** Verify output before installing. Available on Amazon, 10-packs for $8. |
| OLED (optional) | SSD1306 1.3" OLED | I2C display 128×64 | — | 0–1 | $6–10 | **Optional bench/service display** on J2. Not needed in the aircraft; the governor runs without it. Must be I2C, 128×64, 4-pin. J2 order is 5V, GND, SCL, SDA: check the pin order of your module before plugging it in. |

---

## Transistors — High-Side Relay Driver

Two-channel NPN+PNP circuit replacing the ULN2003A from Rev 2, plus (from Rev 3.4) a third NPN+PNP pair in series with the supply to both channels. All TO-92 package. Q1/Q2/Q5 pads are on a 2.54 mm pitch: spread the outer two leads slightly to fit.

| Ref | Part | Type | Digi-Key PN | Qty | Est. Cost | Notes |
|---|---|---|---|---|---|---|
| Q1, Q2 | 2N2907A | PNP — high-side switch | 2N2907A-ND | 2 | ~$0.50 ea | TO-92L. Switches +12V to relay coil. |
| Q3, Q4 | 2N2222A | NPN — level shifter | 2N2222A-ND | 2 | ~$0.30 ea | TO-92. Driven by Nano 5V logic. |
| Q5 | 2N2907A | PNP — driver supply switch | 2N2907A-ND | 1 | ~$0.50 | TO-92L, same part as Q1/Q2. In series with the +12V feed to both relay drivers, so the firmware can cut the supply to a driver that has failed short. |
| Q6 | 2N2222A | NPN — level shifter for Q5 | 2N2222A-ND | 1 | ~$0.30 | TO-92, same part as Q3/Q4. Driven by Nano D5. |

---

## Passives — Resistors

All resistors: 1/4W metal film, 1% tolerance

| Ref | Value | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|
| R1, R9 | 5.1kΩ | 5.1KEBK-ND | 2 | Magneto input series resistors (two in series share the voltage) |
| R2 | 4.7kΩ | 4.7KEBK-ND | 1 | Optocoupler pull-up. **Pads are 15.24 mm apart** — bend the leads wider than the others. |
| R3, R4 | 330Ω | 330EBK-ND | 2 | LED current limiters |
| R5, R6 | 1kΩ | 1.0KEBK-ND | 2 | NPN base resistors (Q3, Q4) |
| R7, R8 | 10kΩ | 10KEBK-ND | 2 | PNP base pull-up to +12V (Q1, Q2) |
| R10, R12 | 10kΩ | 10KEBK-ND | 2 | Driver input pull-downs: hold the relays off while the Nano is in reset |
| R11, R13 | 1kΩ | 1.0KEBK-ND | 2 | PNP base series resistors (Q1, Q2). Sized for relay coils up to about 250 mA — see REV3_3_CHANGES.md |
| R14, R16 | 33kΩ | 33KEBK-ND | 2 | Relay output readback dividers (top) |
| R15, R17 | 10kΩ | 10KEBK-ND | 2 | Relay output readback dividers (bottom) |
| R18 | 10kΩ | 10KEBK-ND | 1 | Pot pull-up to 5V. Sets the pot scale: use 1% |
| R19 | 10kΩ | 10KEBK-ND | 1 | Pot input series protection |
| R20, R21 | 4.7kΩ | 4.7KEBK-ND | 2 | I2C pull-ups (only matter if a display is fitted) |
| R22 | 1kΩ | 1.0KEBK-ND | 1 | Q5 base series resistor (same sizing as R11/R13) |
| R23 | 10kΩ | 10KEBK-ND | 1 | Q5 base pull-up to +12V: holds the switch open unless Q6 pulls it closed |
| R24 | 1kΩ | 1.0KEBK-ND | 1 | Q6 base resistor, from Nano D5 |
| R25 | 10kΩ | 10KEBK-ND | 1 | D5 pull-down: holds the switch open while the Nano is in reset |
| R26 | 33kΩ | 33KEBK-ND | 1 | Switched-rail readback divider (top), to Nano A2 |
| R27 | 10kΩ | 10KEBK-ND | 1 | Switched-rail readback divider (bottom). Also discharges the switched rail when Q5 is open |

**Tip:** 10kΩ ×11 (R7, R8, R10, R12, R15, R17, R18, R19, R23, R25, R27), 1kΩ ×6 (R5, R6, R11, R13, R22, R24), 4.7kΩ ×3, 5.1kΩ ×2, 33kΩ ×3, 330Ω ×2. The optional fault lamp adds 1kΩ ×3 and 10kΩ ×1.
The Digi-Key numbers for the new values follow the pattern of the existing lines; confirm each resolves before ordering.

---

## Passives — Capacitors

| Ref | Value | Type | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|---|
| C1, C4, C5, C6 | 100nF (0.1µF) | Ceramic disc | 445-173-ND | 4 | Non-polarized; marked "104". C6 filters the pot input. |
| C2 | 100µF 50V | Electrolytic, 8 mm diameter, 3.5 mm lead pitch, 105°C | — | 1 | **POLARIZED** — observe polarity. 12V bulk cap, now behind the fuse. The 50V rating is deliberate; do not fit a 25V part. |
| C3 | 47µF 10V | Electrolytic | 493-1205-ND | 1 | **POLARIZED** — observe polarity. 5V output filter cap. |

---

## Protection — Diodes and Fuse

| Ref | Part | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|
| D3, D4 | 1N4001 | 1N4001FSCT-ND | 2 | Flyback diodes across each relay coil circuit — clamp the coil's inductive spike when the PNP switches off. **Do not omit.** Polarity: cathode (band) to the relay output pad (marked "K" on silkscreen), anode to ground. |
| D5 | P6KE22A | — | 1 | 600 W unidirectional TVS, DO-15. Clamps 12V bus transients and, with F1, protects against reversed polarity. Band (cathode) to the "K" mark. Do not fit the bidirectional P6KE22CA. |
| D6 | 1N4148 | 1N4148FS-ND | 1 | Reverse clamp across the optocoupler LED. Band to the "K" mark. |
| F1 | 1A PTC resettable fuse | Bel Fuse 0ZRE0100FF | 1 | In series with the +12V input. **The footprint fits the 0ZRE0100FF: 10.2 mm lead spacing.** Smaller PTCs with 5.1 mm spacing do not fit. |

---

## Optional fault lamp (not fitted by default)

These six parts drive an external fault lamp from Nano D4. **Leave all of them off the board unless the lamp is wanted.** The pads, tracks and silkscreen are on every board; nothing else on the board depends on them, and they are marked do-not-populate in the schematic.

| Ref | Part | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|
| J3 | 1×2 2.54mm male header | — | 0–1 | "FAULT LAMP". An LED wires directly across the two pins, anode to pin 1 (marked "+"). No external resistor is needed. |
| Q7 | 2N2222A | 2N2222A-ND | 0–1 | TO-92, same part and pinout as Q3/Q4/Q6. Switches the lamp return. |
| R28 | 1kΩ | 1.0KEBK-ND | 0–1 | From the +12V rail (ahead of Q5, so the lamp works with the driver supply switched off) to J3 pin 1. **Pads are 15.24 mm apart** — bend the leads wider, as for R2. |
| R29 | 1kΩ | 1.0KEBK-ND | 0–1 | From J3 pin 2 to Q7. With R28 it limits a lamp wire shorted to ground or to the bus to about 14 mA. |
| R30 | 1kΩ | 1.0KEBK-ND | 0–1 | Q7 base resistor, from Nano D4 |
| R31 | 10kΩ | 10KEBK-ND | 0–1 | D4 pull-down: lamp off while the Nano is in reset |

Lamp current is about 5 mA with a red or green LED on a 14 V bus.

---

## Indicators

| Ref | Part | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|
| D1 | 3mm Red LED | 754-1264-ND | 1 | Standard brightness; INC indicator |
| D2 | 3mm Green LED | 754-1221-ND | 1 | Standard brightness; DEC indicator |

---

## Connectors and Sockets

| Ref | Part | Qty | Notes |
|---|---|---|---|
| — | DIP-8 socket | 1 | For U1 (6N137). Use machined-pin (turned-pin) type for reliability. |
| — | 15-pin female single-row socket strip | 2 | For Arduino Nano. Cut to length or buy pre-cut Nano socket kit. |
| J1 | 1×8 2.54mm male header, **position 7 pulled out** | 1 | Aircraft interface. Seven pins on an 8-position strip: positions 1–6 and 8. The empty position keeps the magneto pin 5 mm from its neighbour. |
| J2 | 1×4 2.54mm male header | 0–1 | Optional service-display header (5V, GND, SCL, SDA). Leave unpopulated for the aircraft. |
| J3 | 1×2 2.54mm male header | 0–1 | Optional fault-lamp header — see "Optional fault lamp" above. Leave unpopulated unless the lamp is wanted. |
| PS1 | 1×5 2.54mm male header | 1 | 5-pin for LM2596 module |
| — | 40-pin 2.54mm male header strip | 1 | Break-apart; covers J1/J2/PS1 |

**Tip:** Buy a 40-pin break-apart header strip and a 40-pin female socket strip on Amazon (~$1 each). These cover all connector needs on the board.

---

## Panel Control

Nothing to buy if the aircraft already has the Airborne 350A installed: Rev 3.4 uses the existing panel control
through connector pins E and F.

| Part | Qty | Notes |
|---|---|---|
| Existing 350A panel control | 1 | 10kΩ linear, two wires, pins E and F. Original manual check: under 100Ω full clockwise, 10,000Ω full counter-clockwise, smooth in between. |
| Replacement, only if the original is worn out | 1 | 10kΩ **linear taper** panel potentiometer (marked "B10K"), wired as a two-terminal rheostat: the wiper plus the end terminal that gives minimum resistance at full clockwise. Not an audio-taper ("A10K") part. |

---

## Aircraft Interface

| Part | Qty | Source | Notes |
|---|---|---|---|
| 7-pin Cannon/MS connector (plug) | 1 | Aircraft Spruce, B&C Specialty, Stein Air | MS3106A-16-10S or equivalent. Match to existing connector on airframe. **Measure yours first.** ~$15–40 |
| Backshell for connector | 1 | Same sources | Strain relief; important for vibration environment |
| 22 AWG shielded wire | ~2 ft | Aircraft Spruce or local | For magneto lead connection inside governor box |
| Heat shrink tubing assortment | 1 | — | Cover all solder connections |
| Aluminum project box | 1 | Hammond Mfg or similar | Size your own governor box; original dimensions ~85×65×30mm |

---

## Tools Required (not included in cost estimate)

- Temperature-controlled soldering iron
- Multimeter
- Flush cutters
- Arduino IDE (free download)
- USB-A cable for programming Nano
- Oscilloscope (recommended but optional)
- Second Arduino Nano for bench test pulse generator (~$5 extra)

---

## Complete Order Summary

### Digi-Key single order (all board passives + ICs):
6N137 × 1, 2N2907A × 3, 2N2222A × 3, 1N4001 × 2, 1N4148 × 1, P6KE22A × 1, 0ZRE0100FF × 1, 10kΩ × 11, 1kΩ × 6, 4.7kΩ × 3, 5.1kΩ × 2, 33kΩ × 3, 330Ω × 2, 100nF cap × 4, 100µF 50V cap × 1, 47µF 10V cap × 1, DIP-8 socket × 1

**Estimated Digi-Key subtotal: ~$14–20**

### Amazon or eBay (modules and panels):
Arduino Nano × 1, LM2596-5V module × 1, LED assortment (red + green 3mm) × 1, pin header strips × 1, female socket strips × 1, SSD1306 OLED 1.3" × 1 (optional, bench only)

**Estimated Amazon subtotal: ~$18–28**

### Aircraft Spruce or B&C (aircraft hardware):
7-pin Cannon connector and backshell, avionics wire

**Estimated aircraft hardware: ~$20–45**

### JLCPCB:
5× PCBs + shipping (typically $2 boards + $8–15 shipping to US)

**Estimated PCB: ~$10–17**

---

## Notes on Substitutions

**Arduino Nano:** The CH340 USB chip version is fine. Avoid:
- Arduino Nano Every (different pinout)
- Arduino Nano 33 IoT / BLE (3.3V logic — relay drive levels won't work)
- Any 3.3V variant

**6N137 optocoupler:** Do not substitute with slower optocouplers (4N25, 4N35, PC817). The magneto signal at cruise RPM is ~130 Hz and the pulse width matters. The 6N137 has 10ns response time — slower types will miss pulses or distort the signal.

**2N2907A (Q1, Q2, Q5):** TO-92L package (also called TO-226 or TO-92 wide). The 2N2907A is available in TO-18 (metal can) and TO-92L (plastic). The PCB footprint is TO-92L — do not install the metal can TO-18 variant. Pin order is EBC (Emitter-Base-Collector) from left to right when facing the flat side.

**2N2222A (Q3, Q4, Q6, and Q7 if fitted):** Standard TO-92 package. Very common; any major supplier. Pin order is EBC. Do not substitute 2N2219 (TO-39 metal can) — wrong footprint. **Do not use the ON Semi P2N2222A: its pinout is reversed (C-B-E).** Check the datasheet of the exact part you buy.

**1N4001 (D3, D4):** Any 1N400x series (1N4001 through 1N4007) works here; the voltage rating doesn't matter in this application. Observe polarity — cathode (banded end) to the relay output pad (the "K" silkscreen mark), anode to ground.

**LM2596 module:** Must be the fixed 5V version. The adjustable version ships set to ~12V and will destroy the Nano if installed without adjusting first. The fixed 5V version has a yellow label; the adjustable has a blue trimmer. Buy the fixed 5V version or verify before installing.

**OLED display (optional):** If display shows garbage on boot, the I2C address is 0x3D not 0x3C. Change one line in firmware and reflash. Both addresses are common among different module batches.
