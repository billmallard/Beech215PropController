# Bill of Materials — Rev 3

Complete parts list for one governor build. All parts available from Digi-Key, Mouser, or Amazon.
Links are to Digi-Key where possible — they're reliable, stock everything, and ship fast.

**Estimated total cost: $60–100** depending on sourcing (Amazon vs. Digi-Key, bulk vs. singles)

---

## PCB

| Item | Specification | Qty | Source | Est. Cost | Notes |
|---|---|---|---|---|---|
| Governor PCB | 100×80mm 2-layer FR4 | 5 | JLCPCB | ~$2 + shipping | Upload Gerber zip from `production/` folder; default settings; you get 5, use 1 |

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
| OLED | SSD1306 1.3" OLED | I2C display 128×64 | — | 1 | $6–10 | Must be I2C (2-wire), 128×64. Many on Amazon. Verify module is 4-pin (VCC/GND/SCL/SDA). |

---

## Transistors — High-Side Relay Driver

Two-channel NPN+PNP circuit replacing the ULN2003A from Rev 2. All TO-92 package.

| Ref | Part | Type | Digi-Key PN | Qty | Est. Cost | Notes |
|---|---|---|---|---|---|---|
| Q1, Q2 | 2N2907A | PNP — high-side switch | 2N2907A-ND | 2 | ~$0.50 ea | TO-92L. Switches +12V to relay coil. |
| Q3, Q4 | 2N2222A | NPN — level shifter | 2N2222A-ND | 2 | ~$0.30 ea | TO-92. Driven by Nano 5V logic. |

---

## Passives — Resistors

All resistors: 1/4W metal film, 1% tolerance

| Ref | Value | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|
| R1 | 10kΩ | 10KEBK-ND | 1 | Magneto input current limit |
| R2 | 4.7kΩ | 4.7KEBK-ND | 1 | Optocoupler pull-up |
| R3, R4 | 330Ω | 330EBK-ND | 2 | LED current limiters |
| R5, R6 | 1kΩ | 1.0KEBK-ND | 2 | NPN base resistors (Q3, Q4) |
| R7, R8 | 10kΩ | 10KEBK-ND | 2 | PNP base pull-up to +12V (Q1, Q2) |

**Tip:** R1, R7, R8 are all 10kΩ — order 4 total on a single line item.

---

## Passives — Capacitors

| Ref | Value | Type | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|---|
| C1, C4, C5 | 100nF (0.1µF) | Ceramic disc | 445-173-ND | 3 | Non-polarized; marked "104" |
| C2 | 100µF 25V | Electrolytic | 493-1215-ND | 1 | **POLARIZED** — observe polarity. 12V input bulk cap. |
| C3 | 47µF 10V | Electrolytic | 493-1205-ND | 1 | **POLARIZED** — observe polarity. 5V output filter cap. |

---

## Protection — Diodes and Fuse

| Ref | Part | Digi-Key PN | Qty | Notes |
|---|---|---|---|---|
| D3, D4 | 1N4001 | 1N4001FSCT-ND | 2 | Flyback diodes — clamp relay coil inductive spike when PNP switches off. **Do not omit.** Observe polarity (cathode to +12V). |
| F1 | 1A PTC resettable fuse | — | 1 | In series with +12V_RAW input. Self-resets after overcurrent. Belkin/Bel Fuse 0ZRE or equivalent. |

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
| J1 | 1×7 2.54mm male header | 1 | 7-pin aircraft interface connector; or cut from 40-pin strip |
| J2 | 1×10 2.54mm male header | 1 | 10-pin panel unit connector; or cut from 40-pin strip |
| PS1 | 1×5 2.54mm male header | 1 | 5-pin for LM2596 module |
| — | 40-pin 2.54mm male header strip | 1 | Break-apart; covers J1/J2/PS1 (22 pins needed) |

**Tip:** Buy a 40-pin break-apart header strip and a 40-pin female socket strip on Amazon (~$1 each). These cover all connector needs on the board.

---

## Panel Unit Components

| Part | Qty | Est. Cost | Notes |
|---|---|---|---|
| 5-position rotary switch | 1 | $8–15 | **Single-pole, 5-position.** Lorlin CK1024 or Grayhill 56-series. Must have positive detents (not a continuous encoder). Panel-mount with standard ¼" shaft. |
| Project enclosure or panel plate | 1 | $5–15 | Size depends on your panel opening. Aluminum preferred. |
| Multi-conductor shielded cable | ~3 ft | $5–10 | 10 conductors sufficient. Belden 9536 or equivalent avionics-grade cable. |

**Switch selection notes:**
- Lorlin CK1024: 12-position base type, order 5-position variant. Available Mouser/Digi-Key. ~$8.
- Grayhill 56-series: industry-standard panel switch, very positive detent feel, standard ¼" D-shaft. ~$12–15.
- Avoid: continuous rotary encoders (no detents), wafer switches without detents, or switches with more than 5 positions (firmware only uses 5).
- The original prop control knob reuses directly on any ¼" D-shaft switch.

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
6N137 × 1, 2N2907A × 2, 2N2222A × 2, 1N4001 × 2, 10kΩ resistor × 4 (R1/R7/R8 + spare), 4.7kΩ × 1, 1kΩ × 2, 330Ω × 2, 100nF cap × 3, 100µF 25V cap × 1, 47µF 10V cap × 1, DIP-8 socket × 1

**Estimated Digi-Key subtotal: ~$14–20**

### Amazon or eBay (modules and panels):
Arduino Nano × 1, LM2596-5V module × 1, SSD1306 OLED 1.3" × 1, LED assortment (red + green 3mm) × 1, pin header strips × 1, female socket strips × 1, 1A PTC fuse × 1

**Estimated Amazon subtotal: ~$18–28**

### Mouser or Digi-Key (rotary switch):
5-position rotary switch (Lorlin CK1024 or Grayhill 56-series) × 1

**Estimated switch cost: ~$8–15**

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

**2N2907A (Q1, Q2):** TO-92L package (also called TO-226 or TO-92 wide). The 2N2907A is available in TO-18 (metal can) and TO-92L (plastic). The PCB footprint is TO-92L — do not install the metal can TO-18 variant. Pin order is EBC (Emitter-Base-Collector) from left to right when facing the flat side.

**2N2222A (Q3, Q4):** Standard TO-92 package. Very common; any major supplier. Pin order is EBC. Do not substitute 2N2219 (TO-39 metal can) — wrong footprint.

**1N4001 (D3, D4):** Any 1N400x series (1N4001 through 1N4007) works here; the voltage rating doesn't matter in this application. Observe polarity — cathode (banded end) to the +12V rail.

**LM2596 module:** Must be the fixed 5V version. The adjustable version ships set to ~12V and will destroy the Nano if installed without adjusting first. The fixed 5V version has a yellow label; the adjustable has a blue trimmer. Buy the fixed 5V version or verify before installing.

**OLED display:** If display shows garbage on boot, the I2C address is 0x3D not 0x3C. Change one line in firmware and reflash. Both addresses are common among different module batches.
