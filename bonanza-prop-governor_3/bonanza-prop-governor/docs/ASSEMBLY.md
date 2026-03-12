# Assembly Instructions

## Overview

This guide covers the complete build from bare PCB to installed system. Estimated time for an experienced solderer: 3–4 hours for the governor PCB, 1 hour for the panel unit. First-time builders should budget a full day and not rush.

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

Verify all parts against [BOM.md](../hardware/BOM.md) before starting. Lay them out and identify each one. The most common build error is swapping similar-looking components (e.g., installing a 330Ω where a 10kΩ belongs). Resistor color bands are your friend — confirm each one with your multimeter before soldering.

---

## Section 1: PCB Preparation

### 1.1 Inspect the Board

When your JLCPCB boards arrive (you'll receive 5 — use one, keep the rest):

- Check for any obvious defects: scratches through copper traces, missing pads, solder bridges from fabrication
- Hold it up to light and look for pinholes in the FR4 (rare but check)
- Verify the board dimensions are 100mm × 80mm
- Confirm 4 mounting holes in the corners

### 1.2 One-Time KiCad Zone Fill Step

The Gerbers were generated without the GND copper pour filled (a limitation of the scripted generation). Before your first build, you should open `hardware/350A_governor.kicad_pcb` in KiCad 7, go to **Edit → Fill All Zones** (shortcut: B), run DRC to confirm no violations, then regenerate Gerbers for any future orders. For your first board, the unfilled version works — the GND pour just adds noise immunity and heat sinking; the explicit GND traces and vias handle connectivity.

---

## Section 2: Governor PCB Assembly

**Assembly order matters.** Always install lowest-profile components first so the board lies flat while soldering subsequent components.

### 2.1 Resistors (install first — lowest profile)

| Ref | Value | Color Code | Location |
|---|---|---|---|
| R1 | 10kΩ | Brn-Blk-Org-Gold | Magneto input current limit |
| R2 | 4.7kΩ | Yel-Vio-Red-Gold | Optocoupler pull-up |
| R3 | 330Ω | Org-Org-Brn-Gold | INC LED current limit |
| R4 | 330Ω | Org-Org-Brn-Gold | DEC LED current limit |

Resistors are non-polarized — orientation doesn't matter. Bend leads at 90°, insert, tape or bend leads slightly to hold in place, solder, trim leads flush.

**Verify each with your multimeter before soldering.**

### 2.2 Ceramic Capacitors (non-polarized, install next)

| Ref | Value | Marking | Notes |
|---|---|---|---|
| C1 | 100nF | 104 | Magneto noise bypass |
| C4 | 100nF | 104 | VIN_5V supply bypass |
| C5 | 100nF | 104 | 5V supply bypass |

Ceramic caps are non-polarized. The "104" marking means 10 × 10⁴ pF = 100nF = 0.1µF. These are all the same part.

### 2.3 Electrolytic Capacitors (POLARIZED — observe polarity)

| Ref | Value | Location | Notes |
|---|---|---|---|
| C2 | 100µF / 25V | LM2596 input | Longer lead = POSITIVE. Stripe on body = NEGATIVE. |
| C3 | 47µF / 16V | LM2596 output | Same polarity convention. |

⚠️ **Electrolytic capacitors installed backwards will fail, possibly violently.** The PCB has a "+" marking on the positive pad. The longer lead goes to "+". The stripe on the capacitor body marks the NEGATIVE lead.

### 2.4 LEDs (POLARIZED)

| Ref | Color | Notes |
|---|---|---|
| D1 | Red | INC RPM activity indicator |
| D2 | Green | DEC RPM activity indicator |

LEDs are polarized. The **longer lead is the anode (+)**. The flat side of the LED body is the cathode (−). The PCB silkscreen shows a triangle pointing toward a bar — the bar is the cathode.

Install these so the lens is just above the PCB surface. Do not push them all the way flush — they will be visible indicators during testing.

### 2.5 IC Sockets (install before ICs)

**Install sockets, not the ICs directly.** This allows replacement without desoldering.

| Ref | Socket | Notes |
|---|---|---|
| U1 | DIP-8 socket | For 6N137 optocoupler |
| U2 | DIP-16 socket | For ULN2003A |

Align the notch on the socket with the notch shown on the PCB silkscreen. Solder one corner pin first, verify alignment, then solder remaining pins.

### 2.6 Connectors and Headers

Install these before the Arduino socket — they need the iron to get in from the sides.

**J1 — 7-pin Governor Cannon Interface Header:**
- 1×7 2.54mm pin header
- This is the most critical connector on the board — carries 12V, relay outputs, and the magneto signal
- Solder with care; cold joints here will cause intermittent failures
- Label: A B C D E F H (per silkscreen)

**PS1 — LM2596 Module Header:**
- 1×5 2.54mm pin header
- The LM2596 breakout module plugs in here

**J2 — Panel Connector (10-pin):**
- 1×10 2.54mm pin header or socket
- Connects to the panel unit cable

**J3 — Panel Connector 2 (5-pin):**
- 1×5 2.54mm pin header or socket

### 2.7 Arduino Nano Socket

Use two 15-pin female socket strips (or a 30-pin strip cut in half). This lets you remove the Nano for firmware updates without disturbing the installation.

- Install female sockets on the PCB pads
- The Nano plugs in from above
- Verify orientation: the USB port end of the Nano faces toward the top of the board (toward J1)

### 2.8 IC Installation

Once all soldering is complete and you've done the pre-power inspection (Section 4.1):

- **U1 (6N137):** Align pin 1 (notch end) with pin 1 on socket. Press firmly and evenly.
- **U2 (ULN2003A):** Same — notch toward top of board.

### 2.9 LM2596 5V Module

The LM2596S-5V is a pre-built DC-DC converter module. It should arrive pre-set to 5V output, but **verify this before plugging it in** (see Section 4.2).

Plug into PS1 header. The module sits about 10mm above the PCB.

### 2.10 Arduino Nano

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

// ── Preset RPM targets ─────────────────────────────────────────
#define PRESET_MAX          2650   // Full fine pitch / takeoff
#define PRESET_CRUISE_CLIMB 2450   // Best-power climb
#define PRESET_CRUISE       2300   // Normal cruise (default on AUTO engage)
#define PRESET_ECONOMY      2200   // Long-range economy
// Preset 5 (Emergency/Low) drives to full coarse limit — no RPM target

// ── Control tuning ─────────────────────────────────────────────
#define DEADBAND_RPM        25     // No correction within ±25 RPM of setpoint
#define PROP_ZONE_RPM       75     // Proportional slowdown begins within ±75 RPM
#define MAG_TIMEOUT_MS      2000   // Alarm if no magneto pulse for 2 seconds
#define MAX_RELAY_MS        8000   // Safety cutoff: max relay on-time in ms
```

### 3.4 Flash

1. Connect Nano via USB
2. Tools → Board → Arduino AVR Boards → Arduino Nano
3. Tools → Processor → ATmega328P (Old Bootloader) — try this if standard doesn't work
4. Tools → Port → select the COM port / /dev/ttyUSBx that appeared when you plugged in
5. Click Upload (→ arrow)
6. Confirm "Done uploading" — no errors

### 3.5 Verify Firmware on Bench

With the Nano connected by USB and powered (no aircraft connection yet):
- The OLED should light up showing "MANUAL" mode
- Rotating the encoder should not change anything in MANUAL
- Pressing a preset button should change the setpoint display but not engage AUTO
- No fault codes should be showing

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
3. Current draw should be 50–150mA (OLED + Nano idle)
4. If current limit trips immediately: **power off and find the short**

### 5.2 Voltage Rail Verification

With power applied:
| Test point | Expected |
|---|---|
| LM2596 output | 5.0V ±0.1V |
| Nano Vin pin | 5.0V |
| Nano 5V pin | ~4.9–5.0V |
| Nano 3.3V pin | 3.3V |

### 5.3 OLED Display Check

The OLED should be showing the startup screen. If blank:
- Check I2C address — SSD1306 modules ship at either 0x3C or 0x3D. The firmware defaults to 0x3C. If blank, edit `#define OLED_I2C_ADDR 0x3C` to `0x3D` and reflash.
- Check the J2 connector — confirm SDA (pin 3) and SCL (pin 4) are wired to Nano A4 and A5

---

## Section 6: Panel Unit Assembly

The panel unit mounts in the existing prop control knob opening on the instrument panel.

### 6.1 Components

- SSD1306 1.3" OLED module (I2C, 128×64)
- 5-position single-pole rotary switch (Lorlin CK1024 or Grayhill 56-series recommended)
- Small panel plate or project enclosure

### 6.2 Layout Suggestion

```
┌─────────────────────────────────────────────┐
│  [OLED DISPLAY — 1.3"]                      │
│  CRUISE                              AUTO   │
│  CRUISE                                     │
│  ─────────────────────────────              │
│  SET: 2300 RPM                              │
│  ACT: 2287 RPM                              │
│  HOLD ──────────────────                    │
├─────────────────────────────────────────────┤
│         MAX  CLB  CRS  ECO  LOW             │
│        [ 1 - 2 - 3 - 4 - 5 ]               │
│              [  KNOB  ]                     │
└─────────────────────────────────────────────┘
```

The 5-position rotary switch provides tactile, positive detent selection of each preset. There is no ambiguous "between" state in normal use — each detent corresponds to a specific RPM.

### 6.3 Rotary Switch Wiring

The switch has one COMMON terminal and five position terminals:

```
Switch COMMON  ──── GND  (J2 pin 2 or pin 10)
Position 1     ──── J2 pin 5  (MAX 2650)
Position 2     ──── J2 pin 6  (CRUISE CLIMB 2450)
Position 3     ──── J2 pin 7  (CRUISE 2300)
Position 4     ──── J2 pin 8  (ECONOMY 2200)
Position 5     ──── J2 pin 9  (LOW/EMERGENCY COARSE)
```

No resistors needed. The Nano's internal pull-ups (activated in firmware) keep all position lines HIGH. The selected position pulls its line LOW through the switch.

### 6.4 Panel Cable

10-conductor shielded cable, J2 connector at both ends (or bare wires at the switch end). Total wires: 5V, GND, SDA, SCL, positions 1–5, switch common = 10. The original installation's cable routing can be reused.

### 6.5 Reusing the Original Knob

The original prop control knob fits any ¼" D-shaft rotary switch. The Lorlin CK1024 and Grayhill 56-series both use standard ¼" shafts. If the original knob has a different bore, knob adapters are available from Mouser for ~$1.

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

### 8.3 Install the Panel Unit

1. Remove the original prop control knob assembly
2. Feed the panel unit cable through the control column per the original cable routing
3. Install the panel unit in the knob opening
4. Route cable clear of all control cables and moving parts — secure with tie wraps
5. The original knob can be reused on the new linear pot shaft

### 8.4 Ground Test Before First Flight

Follow the Ground Test section of the original 350A manual (reproduced in the firmware comments and in [BENCH_TEST.md](BENCH_TEST.md)). The key test:

1. Engine running at 1900 RPM
2. Prop switch to AUTO
3. Turn RPM knob full CW and CCW — observe RPM changes
4. Note: *some oscillation at low RPM on the ground is normal* — the prop aerodynamics are fundamentally different at low ground RPM than in flight. Do not tune for ground performance.

### 8.5 First Flight Test

1. Takeoff and climb in MANUAL — as normal
2. At safe altitude, level off at cruise power
3. Select CRUISE preset (2300)
4. Engage AUTO
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

---

*Questions? Open an issue on GitHub. Include your aircraft serial number range (early/late wiring variant matters), engine model, and a description of what you observe.*
