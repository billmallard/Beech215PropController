# Assembly Instructions

## Overview

This guide covers the complete build from bare PCB to installed system. Estimated time for an experienced solderer: 3–4 hours for the governor PCB. There is no panel unit to build. First-time builders should budget a full day and not rush.

**Skill level required:** Basic soldering. All components are through-hole — no SMD. If you've built a kit before, you can build this.

---

## Tools Required

- Temperature-controlled soldering iron (650–700°F / 340–370°C)
- 60/40 or 63/37 rosin-core solder, 0.031" diameter
- Flush cutters (lead trimmers)
- Digital multimeter
- Small Phillips and flat screwdrivers
- Tweezers
- Magnifying glass or loupe
- Isopropyl alcohol (90%+) and brush for flux cleanup
- Arduino IDE installed on a computer with USB-A port

**Recommended but not required:**
- PCB holder / helping hands
- Desoldering braid (for mistakes)
- Continuity tester (your multimeter in beep mode)

---

## Parts Checklist

Verify all parts against [BOM.md](../hardware_rev3_4/hardware/BOM.md) before starting. Lay them out and identify each one. The most common build error is swapping similar-looking components (e.g., installing a 330Ω where a 10kΩ belongs). Resistor color bands are your friend — confirm each one with your multimeter before soldering.

---

## Section 1: PCB Preparation

### 1.1 Inspect the Board

When your JLCPCB boards arrive (you'll receive 5 — use one, keep the rest):

- Check for any obvious defects: scratches through copper traces, missing pads, solder bridges from fabrication
- Hold it up to light and look for pinholes in the FR4 (rare but check)
- Verify the board dimensions are 100mm × 80mm
- Confirm 4 mounting holes in the corners

### 1.2 Production Gerbers

The Gerbers in `hardware/gerbers/350A_governor_JLCPCB.zip` are ready to upload to JLCPCB. They were generated from the current V3 PCB with copper zones filled and 0 DRC violations — no additional KiCad steps are required before ordering.

---

## Section 2: Governor PCB Assembly

**Assembly order matters.** Always install lowest-profile components first so the board lies flat while soldering subsequent components.

### 2.1 Resistors (install first — lowest profile)

| Ref | Value | Color Code | Location |
|---|---|---|---|
| R1, R9 | 5.1kΩ | Grn-Brn-Red-Gold | Magneto input series resistors |
| R2 | 4.7kΩ | Yel-Vio-Red-Gold | Optocoupler pull-up — **pads are 15.24 mm apart; bend the leads wider** |
| R3 | 330Ω | Org-Org-Brn-Gold | INC LED current limit |
| R4 | 330Ω | Org-Org-Brn-Gold | DEC LED current limit |
| R5, R6 | 1kΩ | Brn-Blk-Red-Gold | NPN base resistors (Q3, Q4) |
| R7, R8 | 10kΩ | Brn-Blk-Org-Gold | PNP base pull-ups (Q1, Q2) |
| R10, R12 | 10kΩ | Brn-Blk-Org-Gold | Driver input pull-downs |
| R11, R13 | 1kΩ | Brn-Blk-Red-Gold | PNP base series resistors (Q1, Q2) |
| R14, R16 | 33kΩ | Org-Org-Org-Gold | Relay readback dividers (top) |
| R15, R17 | 10kΩ | Brn-Blk-Org-Gold | Relay readback dividers (bottom) |
| R18, R19 | 10kΩ | Brn-Blk-Org-Gold | Panel control pull-up and series resistor |
| R20, R21 | 4.7kΩ | Yel-Vio-Red-Gold | I2C pull-ups (optional display) |
| R22, R24 | 1kΩ | Brn-Blk-Red-Gold | Series switch: Q5 base series, Q6 base |
| R23, R25 | 10kΩ | Brn-Blk-Org-Gold | Series switch: Q5 base pull-up, drive-line pull-down |
| R26 | 33kΩ | Org-Org-Org-Gold | Series switch readback divider (top) |
| R27 | 10kΩ | Brn-Blk-Org-Gold | Series switch readback divider (bottom) |
| R28–R31 | — | — | **Leave empty.** Optional fault lamp; see 2.8 |

Color codes are for 4-band 5% parts; 1% metal-film parts carry five bands. Trust the meter, not the bands.
The resistor and diode designators are printed **inside** each part outline, so they are hidden once the part is fitted. Work from the silkscreen before you place each one.

Resistors are non-polarized — orientation doesn't matter. Bend leads at 90°, insert, tape or bend leads slightly to hold in place, solder, trim leads flush.

**Verify each with your multimeter before soldering.**

### 2.2 Ceramic Capacitors (non-polarized, install next)

| Ref | Value | Marking | Notes |
|---|---|---|---|
| C1 | 100nF | 104 | Magneto noise bypass |
| C4 | 100nF | 104 | VIN_5V supply bypass |
| C5 | 100nF | 104 | 5V supply bypass |
| C6 | 100nF | 104 | Panel control input filter |

Ceramic caps are non-polarized. The "104" marking means 10 × 10⁴ pF = 100nF = 0.1µF. These are all the same part.

### 2.3 Electrolytic Capacitors (POLARIZED — observe polarity)

| Ref | Value | Location | Notes |
|---|---|---|---|
| C2 | 100µF / 50V | 12V rail, top-left of board | Longer lead = POSITIVE. Stripe on body = NEGATIVE. Use a 50V part. |
| C3 | 47µF / 10V | LM2596 output | Same polarity convention. |

⚠️ **Electrolytic capacitors installed backwards will fail, possibly violently.** The PCB has a "+" marking on the positive pad. The longer lead goes to "+". The stripe on the capacitor body marks the NEGATIVE lead.

### 2.4 LEDs (POLARIZED)

| Ref | Color | Notes |
|---|---|---|
| D1 | Red | INC RPM activity indicator |
| D2 | Green | DEC RPM activity indicator |

LEDs are polarized. The **longer lead is the anode (+)**. The flat side of the LED body is the cathode (−). The PCB silkscreen shows a triangle pointing toward a bar — the bar is the cathode.

Install these so the lens is just above the PCB surface. Do not push them all the way flush — they will be visible indicators during testing.

### 2.5 Transistors (Q1–Q6)

The relay driver circuit uses two NPN+PNP transistor pairs, and a third pair (Q5, Q6) forms the series switch that feeds them both — all TO-92 package, about the same size as a small LED. **Q7 is left empty** (optional fault lamp).

| Ref | Part | Type | Notes |
|---|---|---|---|
| Q1, Q2 | 2N2907A | PNP — high-side switch | TO-92L (wide body). Switches +12V to relay coil. |
| Q3, Q4 | 2N2222A | NPN — level shifter | Standard TO-92. Driven by Nano D6/D7 (5V logic). |
| Q5 | 2N2907A | PNP — series switch | Same part and footprint as Q1/Q2. Feeds +12V to Q1 and Q2; the firmware opens it if a relay output is live that it did not command. |
| Q6 | 2N2222A | NPN — level shifter for Q5 | Same part and footprint as Q3/Q4. Driven by Nano D5. |

**Pin orientation (critical):** Facing the flat side of the TO-92 body, pin order is **E – B – C** left to right for both types. The PCB silkscreen shows the flat body side — match it.

**2N2907A package note:** Plastic TO-92 only. Do not use the TO-18 metal-can variant. The Q1/Q2/Q5 pads are 2.54 mm apart: spread the two outer leads a little before inserting.

**2N2222A warning:** some parts sold as "2N2222A" in TO-92 (the ON Semi P2N2222A in particular) have the pinout reversed, C – B – E. Check the datasheet of the part you bought.

### 2.6 Diodes and Fuse

| Ref | Part | Notes |
|---|---|---|
| D3, D4 | 1N4001 | Flyback diodes across the relay coil circuits. **Cathode (banded end) to the pad marked "K"**. Do not omit. |
| D5 | P6KE22A | Transient clamp on the 12V rail. Band to the "K" pad. It is the large diode next to the fuse. |
| D6 | 1N4148 | Small glass diode beside U1; protects the optocoupler LED. Band to the "K" pad. |
| F1 | 1A PTC fuse (0ZRE0100FF) | In series with the 12V input. Not polarized. Leads are 10.2 mm apart. |

⚠️ **Diode polarity:** every diode's band goes to the "K" mark. A reversed D3 or D4 conducts DC through the relay coil; a reversed D5 shorts the 12V rail and trips the fuse.

> **Only build on a board marked "REV 3.4"** (bottom center). Rev 3, 3.1 and 3.2 boards have faults in the relay driver, the optocoupler enable pin and the fuse footprint, and no panel-control input. See [REV3_3_CHANGES.md](../hardware_rev3_3/REV3_3_CHANGES.md).

### 2.7 IC Sockets (install before ICs)

**Install sockets, not the ICs directly.** This allows replacement without desoldering.

| Ref | Socket | Notes |
|---|---|---|
| U1 | DIP-8 socket | For 6N137 optocoupler |

Align the notch on the socket with the notch shown on the PCB silkscreen. Solder one corner pin first, verify alignment, then solder remaining pins.

### 2.8 Connectors and Headers

Install these before the Arduino socket — they need the iron to get in from the sides.

**J1 — 7-pin Governor Cannon Interface Header:**
- Cut an **8-position** strip of 2.54mm header and pull out the pin in position 7 (second from the end)
- The seven remaining pins are, from the top: 12V (A), GND (B), INC (C), DEC (D), POT (E), RTN (F), gap, MAG (H)
- This is the most critical connector on the board — carries 12V, relay outputs, the panel control and the magneto signal
- Solder with care; cold joints here will cause intermittent failures

**PS1 — LM2596 Module Header:**
- 1×5 2.54mm pin header
- The LM2596 breakout module plugs in here

**J2 — Service Display Header (4-pin, optional):**
- 1×4 2.54mm pin header: 5V, GND, SCL, SDA
- Only needed if you want a display on the bench. Leave it off for the aircraft.

**J3 — Fault Lamp Header (2-pin, optional, NOT fitted by default):**
- Leave J3, Q7, R28, R29, R30 and R31 off the board. The governor is complete without them
- They are only for an installation that adds a fault lamp outside the box. If you do fit them: R28, R29, R30 are 1kΩ (R28 has the wide 15.24 mm lead spacing), R31 is 10kΩ, Q7 is a 2N2222A, and an LED wires straight across J3 with its anode on the pin marked +. See [WIRING.md](WIRING.md)

### 2.9 Arduino Nano Socket

Use two 15-pin female socket strips (or a 30-pin strip cut in half). This lets you remove the Nano for firmware updates without disturbing the installation.

- Install female sockets on the PCB pads
- The Nano plugs in from above
- Verify orientation: the USB port sits over the "USB" outline on the silkscreen, toward the bottom edge of the board

### 2.10 IC Installation

Once all soldering is complete and you've done the pre-power inspection (Section 4.1):

- **U1 (6N137):** Align pin 1 (notch end) with pin 1 on socket. Press firmly and evenly.

### 2.11 LM2596 5V Module

The LM2596S-5V is a pre-built DC-DC converter module. It should arrive pre-set to 5V output, but **verify this before plugging it in** (see Section 4.2).

Plug into PS1 header. The module sits about 10mm above the PCB.

### 2.12 Arduino Nano

Flash the firmware BEFORE installing in the board (easier to handle). See Section 3.

---

## Section 3: Firmware

### 3.1 Install Arduino IDE

Download Arduino IDE 2.x from arduino.cc. Install the CH340 driver if on Windows (the Nano uses a CH340 USB-serial chip — Windows needs this driver; Mac and Linux usually don't).

### 3.2 Install Required Libraries

In Arduino IDE, go to Tools → Manage Libraries and install:
- **Adafruit SSD1306** (by Adafruit)
- **Adafruit GFX Library** (by Adafruit, will be installed as a dependency)

### 3.3 Configure Firmware Constants

Open `firmware/governor_350A/governor_350A.ino`. At the top of the file, find the configuration section and set:

```cpp
// ── CRITICAL: Set for your engine ──────────────────────────────
#define PULSES_PER_REV  3    // 6-cyl Continental (IO-470, IO-520, E-185/E-225)
                             // Change to 2 for 4-cylinder engines

// ── Panel control range — SET FOR YOUR ENGINE ──────────────────
#define POT_RPM_MAX         2650   // Knob full clockwise: take-off RPM
#define POT_RPM_MIN         1800   // Knob full counter-clockwise: lowest RPM
// These two are placeholders. Set them for your engine's limits.

// ── Control tuning ─────────────────────────────────────────────
#define DEADBAND_RPM        25     // No correction within ±25 RPM of setpoint
#define PROP_ZONE_RPM       75     // Proportional slowdown begins within ±75 RPM
#define MAG_TIMEOUT_MS      2000   // Relays off if no magneto pulse for 2 seconds
```

### 3.4 Flash

1. Connect Nano via USB
2. Tools → Board → Arduino AVR Boards → Arduino Nano
3. Tools → Processor → ATmega328P (Old Bootloader) — try this if standard doesn't work
4. Tools → Port → select the COM port / /dev/ttyUSBx that appeared when you plugged in
5. Click Upload (→ arrow)
6. Confirm "Done uploading" — no errors

### 3.5 Verify Firmware on Bench

With the Nano connected by USB only (not in the board):
- Open the Serial Monitor at 9600 baud
- The banner should read `350A Digital Governor v3.1`
- Type `s` for status and `?` for the command list

Firmware v3.1 starts in AUTO and waits in standby (`NO MAG SIGNAL`) until it sees engine pulses. No display is needed.

---

## Section 4: Pre-Power Inspection

Before applying any power, perform these checks.

### 4.1 Visual Inspection

- All leads trimmed flush — no solder bridges between adjacent pads
- No flux residue (clean with IPA and brush if needed)
- All ICs seated correctly in sockets with pin 1 aligned to notch
- Electrolytic capacitors: positive lead to "+" pad, stripe away from "+" pad
- LED polarity confirmed

### 4.2 LM2596 Output Voltage Verification

**Do this BEFORE plugging the module into the board.**

1. Set your bench supply to 12–14V (simulating aircraft bus)
2. Apply 12V to the LM2596 module input pins (VIN and GND)
3. Measure output voltage between VOUT and GND pins
4. Should read 5.0V ±0.2V
5. If not 5V, adjust the small trimmer pot on the module until it reads 5.0V

Applying unregulated 12V to the Nano's 5V pin will destroy it. This step is not optional.

### 4.3 Continuity Checks

With no power applied, use your multimeter in continuity/beep mode:

| Test | Expected |
|---|---|
| J1 pin B (GND) to any ground pad | Continuity (beep) |
| J1 pin A (12V) to GND | No continuity (no beep — no dead short) |
| J1 pin C (INC) to GND | No continuity |
| J1 pin D (DEC) to GND | No continuity |
| 5V rail to GND | No continuity (small resistance OK, direct short = problem) |

---

## Section 5: Bench Power Test

### 5.1 First Power Application

Use a current-limited bench supply set to 13.8V / 500mA current limit.

1. Do NOT connect the aircraft Cannon connector yet
2. Apply 12V to J1 pin A and ground to J1 pin B using clip leads
3. Current draw should be about 30–100mA (Nano idle; more with a display fitted)
4. If current limit trips immediately: **power off and find the short**

### 5.2 Voltage Rail Verification

With power applied:
| Test point | Expected |
|---|---|
| LM2596 output | 5.0V ±0.1V |
| Nano 5V pin | 5.0V (the board feeds the 5V pin directly; VIN is not connected) |
| U1 pin 8 and pin 7 | 5.0V |
| J1 POT pin (E) with nothing connected | 5.0V; drops to about 2.5V with a 10kΩ resistor from E to F |
| Nano 3.3V pin | 3.3V |

### 5.3 Optional Display Check

If you fitted J2 and plugged in an SSD1306 display before power-up, it should show the startup screen. If blank:
- Check the module pin order against J2 (5V, GND, SCL, SDA)
- Check I2C address — SSD1306 modules ship at either 0x3C or 0x3D. The firmware defaults to 0x3C. If blank, edit `#define OLED_I2C_ADDR 0x3C` to `0x3D` and reflash.
- The display is only detected at power-up. Plugging it in afterwards does nothing until the next power cycle.

---

## Section 6: Panel Control

The board uses the panel control that is already in the aircraft. There is no panel unit to build.

### 6.1 Check the Existing Control

With the 7-pin connector unplugged from the governor (original manual, Service Note 1, step 5):

1. Ohmmeter between pins E and F of the cable connector
2. Knob full clockwise: under 100Ω
3. Knob full counter-clockwise: about 10,000Ω
4. The reading should change smoothly, with no jumps or open spots, from stop to stop

A control that jumps or goes open will make the RPM setting jump. The firmware treats a reading above about 16kΩ as an open wire and stops driving the relays until it recovers.

### 6.2 If the Control Must Be Replaced

Use a 10kΩ **linear taper** panel potentiometer (marked "B10K") wired as a two-terminal rheostat: the wiper, plus whichever end terminal gives minimum resistance at full clockwise. Audio-taper ("A10K") parts are far more common and are the wrong part. Mount and torque per the original manual (shoulder nut 24 inch-pounds).

### 6.3 What the Knob Does

Full clockwise is the highest RPM (`POT_RPM_MAX`), full counter-clockwise the lowest (`POT_RPM_MIN`), linear in between. When the knob is moved the governor walks to the new RPM at 100 RPM per second rather than jumping.

---

## Section 7: Bench Test with Motor

See [BENCH_TEST.md](BENCH_TEST.md) for the complete bench test procedure using a pulse generator Arduino and your spare prop motor.

**Do not install in the aircraft until bench testing is complete and you are satisfied with operation.**

---

## Section 8: Aircraft Installation

**Have your IA present for the aircraft installation.**

### 8.1 Remove the Original 350A

1. Aircraft master OFF, magneto OFF
2. Disconnect the 7-pin Cannon connector from the 350A
3. Remove the four screws securing the 350A to its mounting spacers
4. Note the orientation — the new unit mounts the same way (connector facing down, nameplate facing cabin door)

### 8.2 Mount the New Governor Box

1. Mount the PCB in a suitable enclosure (the original die-cast box can be reused if dimensions fit, or a new aluminum project box)
2. Drill for the 7-pin Cannon connector pass-through
3. Mount on the original spacers using the same 4 screws
4. Connect the Cannon connector — verify locking ring engages

### 8.3 Panel Control

Leave the existing panel control and its wiring in place. Check it per Section 6.1 before plugging the connector into the new governor.

### 8.4 Ground Test Before First Flight

Follow the Ground Test section of the original 350A manual (reproduced in the firmware comments and in [BENCH_TEST.md](BENCH_TEST.md)). The key test:

1. Engine running at 1900 RPM
2. Prop switch to AUTO
3. Turn RPM knob full CW and CCW — observe RPM changes
4. Note: *some oscillation at low RPM on the ground is normal* — the prop aerodynamics are fundamentally different at low ground RPM than in flight. Do not tune for ground performance.

### 8.5 First Flight Test

1. Takeoff and climb in MANUAL — as normal
2. At safe altitude, level off at cruise power
3. Turn the RPM knob to about mid-range
4. Prop switch to AUTO
5. Observe: RPM should stabilize within 3–5 corrections
6. Make small throttle inputs and verify governor compensates smoothly
7. If hunting is excessive, increase DEADBAND_RPM in firmware (try 35, then 50) and reflash

---

## Torque Values and Hardware Notes

| Fastener | Torque |
|---|---|
| Governor mounting screws (original spacers) | Finger tight + 1/4 turn |
| Cannon connector locking ring | Hand tight |
| Panel pot shoulder nut | 24 inch-pounds (per original 350A spec) |

---

## Revision History

| Rev | Date | Changes |
|---|---|---|
| 1.0 | Initial release | First production version |
| 3.0 | 2026-04-27 | V3 PCB: ULN2003A (U2) replaced by discrete NPN+PNP transistor pairs (Q1–Q4); added flyback diodes D3/D4 and PTC fuse F1; J3 removed; C3 changed to 10V rating; added R5–R8 |
| 3.3 | 2026-11-14 | Panel control returned to the original 10kΩ pot on connector pins E/F (rotary switch, presets and panel unit removed; J2 is now an optional 4-pin display header). Relay driver base resistors and input pull-downs added, optocoupler enable pin corrected, fuse footprint corrected, 12V transient clamp and magneto input protection added, board re-routed with a solid ground plane. Needs firmware v3.0. Details: [REV3_3_CHANGES.md](../hardware_rev3_3/REV3_3_CHANGES.md) |
| 3.2 | 2026-07-25 | Flyback diodes D3/D4 relocated from across the 2N2907A transistors to across the relay coil circuits (cathode to relay output, anode to ground) per flight-test engineering feedback — the previous placement provided no clamping of the negative turn-off spike. 12V rail rerouted around D3. "REV 3.2" silkscreen added for board identification. Existing Rev 3/3.1 boards: see [REWORK_FLYBACK_DIODES.md](REWORK_FLYBACK_DIODES.md) |
| 3.4 | 2026-11-15 | Series switch (Q5, Q6, R22–R25) ahead of both relay drivers, with its own readback (R26, R27): the firmware opens it when a relay output is live that it did not command. Unpopulated pads for an optional fault lamp (J3, Q7, R28–R31). Needs firmware v3.1. Rev 3.3 was never built. |

---

*Questions? Open an issue on GitHub. Include your aircraft serial number range (early/late wiring variant matters), engine model, and a description of what you observe.*
