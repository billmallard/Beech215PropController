# Hardware Rev 3.3 — Change Record and Verification Evidence

> **Superseded 2026-11-15 by Rev 3.4** before any Rev 3.3 board was made.
> Rev 3.4 is this design plus a series switch ahead of the relay drivers and
> pads for an optional fault lamp: see
> [hardware_rev3_4/REV3_4_CHANGES.md](../hardware_rev3_4/REV3_4_CHANGES.md).
> Build Rev 3.4, with firmware v3.1. This file stays as the record of what
> changed between Rev 3.2 and 3.3, all of which Rev 3.4 carries.

Date: 2026-11-14. Base: Rev 3.2. Tooling: KiCad 10.0 (files are KiCad 10
format). Matching firmware: v3.0.

**Status: designed and DRC-clean. Not fabricated, not bench-tested, not
flown.** Everything under "Verification" is a check on the design files. The
items under "Not yet verified" need hardware or measurements.

## Why

Two things drove this revision.

1. **The panel control goes back to the original potentiometer.** The
   installation has to be functionally identical to the 350A it replaces. The
   original panel control is a 10k linear control wired as a two-terminal
   rheostat across connector pins E and F (original manual, Service Note 1
   step 5: under 100 ohms full clockwise, 10 000 ohms full counter-clockwise;
   full clockwise is the take-off RPM). Rev 3.3 reads that control directly.
   The 5-position rotary switch, its five wires and the presets are gone. The
   box now needs nothing but the 7-pin connector.
2. **A design review of Rev 3.2 found faults that DRC cannot see.** Three of
   them would have stopped the board working or damaged it (items 3, 5 and 6
   below). They are inherited from Rev 3 and 3.1.

## What changed — circuit

| # | Change | Detail |
|---|---|---|
| 1 | Pot input added | J1-5 (E) = `POT_HI`, J1-6 (F) = GND. R18 10k pulls `POT_HI` up to 5 V; R19 10k and C6 100 nF feed Nano A7. Reading is ratiometric: `adc = 1023 * Rpot / (Rpot + 10k)`. An open wire reads full scale and is detected; a shorted wire reads as full clockwise, the same as the original. A short of pin E to the 14 V bus puts under 1 mA into the Nano. |
| 2 | Rotary switch removed | Nano D3, D4, D5, D10, D11 are now unused. J2 shrinks from 10 pins to a 4-pin optional service-display header: **5V, GND, SCL, SDA** (SCL and SDA swapped relative to Rev 3.2 so the traces do not cross). |
| 3 | PNP base resistors added | R11 and R13 (1k) between each 2N2222A collector and its 2N2907A base. Rev 3–3.2 tied them directly together, so the NPN tried to sink several hundred mA through the PNP's base junction with ~11 V across itself. Base drive is now about 11 mA. |
| 4 | Driver input pull-downs | R10 and R12 (10k) from D6/D7 to ground hold both drivers off while the Nano's pins float during reset and bootloader. |
| 5 | 6N137 enable corrected | U1 pin 7 was tied to GND, which forces the output off. It is now tied to 5 V. |
| 6 | Fuse footprint corrected | F1 was drawn on the 0.12 A 0ZRE0012FF footprint. It is now the 1 A 0ZRE0100FF footprint (10.2 mm lead spacing, 1.0 mm holes). |
| 7 | Magneto input hardened | The 10k series resistor is split into R1 + R9 (5.1k each) so each sees half the voltage. D6 (1N4148) across the opto LED clamps reverse voltage (the LED is rated 5 V reverse). The two nets ahead of R9 are in a 2.5 mm clearance class. J1 pin 7 moves one position down the header (see layout). |
| 8 | 12 V input protection | D5 (P6KE22A unidirectional TVS) after the fuse clamps bus transients and, with the fuse, covers reversed polarity. C2 moves to the protected side of the fuse and goes from 25 V to 50 V. |
| 9 | Relay output readback | R14/R15 and R16/R17 (33k/10k) divide the INC and DEC outputs to Nano A3 and A6. Since 2026-11-15 the firmware reads them every control tick and reports a driver that is live when it should be off or dead when it should be on. Report only. |
| 10 | I2C pull-ups | R20, R21 (4.7k) on SCL and SDA. The documents said these existed; they did not. |
| 11 | Footprints | Q1/Q2 move to the 2.54 mm wide TO-92 footprint (Rev 3.2 had 0.22 mm between the +12 V, base and output pads). R2 uses the 15.24 mm pitch resistor footprint (same part, leads bent wider). D1/D2 get a proper library link. |

Net-level result, from the KiCad netlist export:

- New nets: `POT_HI`, `POT_SENSE`, `INC_SENSE`, `DEC_SENSE`, `MAG_MID`,
  `Q1_BASE`, `Q2_BASE`.
- Removed nets: `D3_SW1`, `D4_SW2`, `D5_SW3`, `D10_SW4`, `D11_SW5`.
- New parts: R9–R21, C6, D5, D6. Changed value: R1 (10k → 5.1k), C2 (25 V →
  50 V). Changed footprint: F1, C2, Q1, Q2, R2, J1, J2.

## What changed — layout

The board was re-routed completely. Outline, mounting holes, J1 pin 1, J2
pin 1, PS1, the Nano and the two LEDs are where they were on Rev 3.2.

| # | Change | Detail |
|---|---|---|
| 1 | Bottom layer is a solid ground plane | All 129 track segments are on the top layer. There are no tracks and no vias on the bottom. Rev 3.2 ran both power rails round the perimeter on the bottom and cut the plane into pieces joined by necks under 1 mm wide. |
| 2 | Top-layer ground pour | Fills the unused top copper. Unconnected islands are removed automatically. |
| 3 | Via fence and keep-out removed | The 62-via fence along the magneto trace had no top copper to stitch. The opto now sits near J1, so that net is under 20 mm of trace instead of 71 mm. |
| 4 | Magneto clearance | `MAG_LEAD` and `MAG_MID` hold 2.5 mm to everything, including both pours. J1 uses a 1x8 header outline with position 7 left empty, so the magneto pin is 5.08 mm from its neighbour instead of 2.54 mm. |
| 5 | Crossings use components, not vias | The 12 V spine passes under the bodies of R14, R16, R18, R19, R7, R11 and D4. The three analog lines pass under R2, R15 and R17. The opto output and the three analog lines reach the Nano through the gap between its two pin rows. |
| 6 | Track widths | 12 V and relay outputs 1.0 mm, 5 V 0.8 mm, driver internals and magneto 0.5 mm, signals 0.4 mm (Rev 3.2 signals were 0.25 mm). |
| 7 | Silkscreen | "REV 3.3" at bottom centre. Title no longer carries "v2.0". J1 labels: 12V GND INC DEC POT RTN MAG. J2 labels: 5V GND SCL SDA. Resistor and diode designators sit inside the body outlines. |

## Verification

All of these were run on the files in this folder.

- **Netlist** (KiCad 10 CLI export from the schematic): every net matches the
  intended design; the list is in the section above.
- **ERC**: 22 errors on Rev 3.2 → 1 on Rev 3.3. The one left is "power input
  not driven" on a Nano GND pin, which is a symbol-library artefact. Unused
  pins now carry explicit no-connect flags. The remaining warnings are
  library-mismatch notices carried over from Rev 3.2.
- **DRC with schematic parity**, after zone refill: **0 violations, 0
  unconnected pads, 0 footprint errors.** Report: `hardware/DRC.rpt`. Rev 3.2
  carried two footprint warnings on D1/D2; those are fixed.
- **Spacing, measured with temporary stricter rules on a scratch copy**:
  - No copper of two different nets is closer than 0.6 mm, pours aside.
    Pours keep 0.5 mm.
  - `MAG_LEAD` and `MAG_MID` are 2.50 mm from both ground pours and 2.81 mm
    from the nearest pad of another net. Rev 3.2 was 0.50 mm and 0.84 mm.
  - Nearest track to the board edge is 0.96 mm; pours stop 0.5 mm from it.
- **Ground**: the bottom pour fills as one piece, 7028 mm².
- **Drill file**: 138 holes for 138 through-hole pads; no via drills.
- **Reproducibility**: `build_scripts/rebuild.sh` regenerates the schematic,
  board and every output in this folder from the Rev 3.2 files and ends on
  the same clean DRC.

## Not yet verified — needs hardware or a measurement

1. **Relay coil current.** It is not documented anywhere. The original 350A
   ran its whole supply through a 1/2 A fuse, so the coil draws less than
   that. With 1k base resistors the 2N2907A is comfortably saturated to about
   250 mA. Measure both coils. If either is above ~250 mA, lower R11/R13 (and
   R22 on Rev 3.4) or change the output transistors before flight.
   **How, with a multimeter, engine off, 7-pin connector unplugged from the
   governor, working on the aircraft-side connector:**
   - *By resistance (no power).* Master off, prop at an intermediate pitch so
     both limit switches are closed. Ohmmeter from pin C to airframe ground,
     then pin D to ground. Each reading is one relay coil. Coil current at
     14 V is 14 divided by that resistance: 56 ohms or more is 250 mA or
     less.
   - *By current (confirms it).* This is the original manual's relay test
     (Service Note 1, step 7) with the meter in place of its U-shaped jumper.
     Meter on DC amps, 10 A input, one probe on pin A and the other on pin C.
     Battery switch on, prop switch to AUTO: the relay pulls in, the prop runs
     to full fine and stops. The meter reads the coil current only (the motor
     is fed through the relay contacts, not through the meter) and falls to
     zero when the limit switch opens. Repeat from pin A to pin D, which runs
     the prop to full coarse. Note the bus voltage; with the alternator
     charging the current is about a tenth higher than on the battery. Leave
     the prop at full fine. Never put a meter set to amps between pin A and
     pin B or ground: that is a dead short across the bus.
2. **Magneto signal level.** The original harness has its own 10k isolating
   resistor at the magneto (manual: pin H reads 10 000 ohms to ground), so the
   opto LED sees the P-lead through about 20k in total. The 6N137 needs about
   5 mA, which means P-lead peaks of roughly 100 V or more. That is typical of
   a magneto primary but has not been measured on this aircraft. R1 and R9
   can be changed without a board change if more current is needed, so this
   does not have to be known before boards are ordered.
   **How to check it:**
   - *The test that counts needs no instrument:* with a built board in the
     aircraft and a laptop on the Nano's USB port, run the engine and compare
     `ACT=` on the status line with the panel tachometer at idle and at
     run-up RPM. A steady, correct reading at both is a pass. It does not
     show how much margin there is.
   - *Margin, without an oscilloscope:* a multimeter cannot read these
     pulses directly (they last well under a millisecond), but it can read a
     peak detector. Connector unplugged from the governor; a 1N4007 in series
     from pin H into a 0.1 uF film capacitor rated 630 V or more, the
     capacitor's other lead to airframe ground; meter on DC volts across the
     capacitor; engine running. Take the reading, then turn the diode round
     and take it again for the other polarity. At least one of the two should
     be well above 105 V (5 mA through 20.2k plus the LED). The capacitor
     holds that voltage afterwards: discharge it through a resistor.
   - *The full picture:* an oscilloscope on pin H with a x10 probe rated for
     the voltage (expect a few hundred volts peak) shows the pulse shape and
     the ringing after it, which the peak detector cannot. Worth doing if the
     RPM reading is ever erratic; not required otherwise.
   Measuring at pin H cannot ground the magneto: the 10k isolating resistor
   is between pin H and the P-lead.
3. **F1 fit.** The footprint is the KiCad library part for the 0ZRE0100FF.
   Check it against the part in hand.
4. **Transistor pinout.** The board expects E-B-C viewed from the flat face.
   The ON Semi P2N2222A is C-B-E and must not be used.
5. **Nothing has been powered.** No board of this revision exists.
6. **Early-wiring aircraft (through D-1820).** There the panel switch
   energises the relay coils directly in manual, on the same wires as leads C
   and D, so bus voltage appears on both outputs while the governor is
   unpowered. On this board that should back-feed about a milliamp through
   each output transistor and under half a milliamp into a Nano input, which
   is harmless on paper, and the original lived with the same condition. It
   has not been tested. Later aircraft (D-1821 on) never see it.

## Decisions still open

- **Pot range: decided 2026-11-15.** Full clockwise is 2650 RPM and full
  counter-clockwise is 1800 RPM (`POT_RPM_MAX` / `POT_RPM_MIN`). Bill
  confirmed these as the real range for his aircraft.
- **Latched alarms: decided 2026-11-15, both removed.** The firmware now has
  no latched state at all, like the original. The 8-second relay timeout is
  gone: it stopped the governor after any 8 seconds of continuous command,
  which happens whenever the prop rests on a pitch stop, and the original
  holds its output until the RPM is reached and leaves the stops to the limit
  switches. A reading below 400 RPM is now a standby: relays off while it
  lasts, governing resumes by itself. The 3500 RPM upper limit is gone
  altogether: like the original, the governor answers any high reading by
  commanding a decrease, which is what a real overspeed needs.
- **Setpoint ramp: decided 2026-11-15.** The 100 RPM/s ramp may no longer
  make the governor drive away from the knob setting. Before, an engine
  accelerating faster than the ramp (start, in-flight restart, throttle
  advance after a signal dropout) was answered with a decrease command,
  toward coarse pitch, until the ramp caught up.
- **Relay readback: done 2026-11-15, monitoring only.** The firmware reports
  an output that is live while commanded off or dead while commanded on. It
  takes no action on either.
- **Stuck-driver protection: done in Rev 3.4.** A series switch ahead of
  both drivers, opened by the firmware when readback shows a relay output
  live that was not commanded. It stays open until power is cycled (Bill,
  2026-11-15). Rev 3.3 has no such switch.

## Existing boards

Rev 3, 3.1 and 3.2 boards share faults 3, 5 and 6 above and have no pot
input. The flyback rework in
[docs/REWORK_FLYBACK_DIODES.md](../docs/REWORK_FLYBACK_DIODES.md) does not
make them airworthy. Do not fly one.

## Fabrication

Upload `hardware/gerbers/350A_governor_JLCPCB.zip` per
[hardware/JLCPCB_INSTRUCTIONS.md](hardware/JLCPCB_INSTRUCTIONS.md). Same
board size, layer count and settings as Rev 3.2. Parts:
[hardware/BOM.md](hardware/BOM.md).

## How this revision was produced

By script, not in the KiCad GUI. `build_scripts/` holds the schematic edit,
the placement table, the routing table and the board builder; `rebuild.sh`
runs them and regenerates every output. The scripts start from the Rev 3.2
files each time, so once anyone edits this board by hand in KiCad the scripts
should be retired.
