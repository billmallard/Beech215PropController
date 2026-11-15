# Hardware Rev 3.4 — Change Record and Verification Evidence

Date: 2026-11-15. Base: Rev 3.3 (`../hardware_rev3_3/`, designed 2026-11-14,
never fabricated). Tooling: KiCad 10.0 (files are KiCad 10 format).

**Status: designed and DRC-clean. Not fabricated, not bench-tested, not
flown.** Everything under "Verification" is a check on the design files. The
items under "Not yet verified" need hardware or measurements.

**Matching firmware: v3.1. Firmware v3.0 must not be run on this board.**
v3.0 sets Nano D4 and D5 to `INPUT_PULLUP` (they were rotary-switch lines on
Rev 3.2). On Rev 3.4 the Nano's internal pull-up (20 to 50k) then works
against the 10k pull-down, which leaves about 1 V on each pin: enough to turn
Q6 partly on, so the driver supply switch would sit half-closed and undefined
instead of open or closed. v3.1 drives both pins as outputs.

## Why

1. **A series switch ahead of both relay drivers.** On Rev 3.3 one shorted
   output transistor (Q1 or Q2) holds its relay energised and nothing in the
   firmware can stop it; the prop runs to that pitch stop until the pilot
   leaves AUTO. The firmware can already see the fault through the output
   readback, but had no way to act on it. Rev 3.4 puts a second switch, Q5,
   between the protected 12 V rail and both drivers. Opening it removes the
   supply from both relay outputs whatever state Q1 and Q2 are in. Bill
   approved adding it on 2026-11-15.
2. **An optional fault lamp output.** Pads only: six parts that are not
   fitted by default. If fitted, they drive an LED outside the box from Nano
   D4. It is fed from the 12 V rail ahead of Q5, so it can show a fault while
   the driver supply is switched off. Also approved 2026-11-15.

The 7-pin connector, the board outline, the mounting holes and the positions
of J1, J2, the Nano and the LM2596 module are exactly as on Rev 3.3.

## What changed — circuit

| # | Change | Detail |
|---|---|---|
| 1 | Driver supply switch | Q5 (2N2907A, same footprint as Q1/Q2). Emitter on the protected 12 V rail `12V_RAW`, collector on the new net `12V_SW`. |
| 2 | Drivers move to the switched rail | The Q1 and Q2 emitters and the R7 and R8 base pull-ups move from `12V_RAW` to `12V_SW`. Nothing else is on `12V_SW` except the readback divider: the LM2596 module, the TVS D5 and the bulk cap C2 stay on `12V_RAW`. |
| 3 | Switch drive | Q6 (2N2222A, same footprint and E-B-C pinout as Q3/Q4), emitter to GND, collector through R22 (1k) to the Q5 base. R23 (10k) from the Q5 base to `12V_RAW` holds Q5 off unless Q6 conducts. Base drive is about 11 mA, the same as Q1/Q2 get through R11/R13. |
| 4 | Control line | Nano **D5**, net `D5_DRV_EN`, **HIGH = switch closed**. R24 (1k) to the Q6 base. R25 (10k) from D5 to GND keeps the switch open while the Nano is in reset or bootloader, or if D5 is left as an input. |
| 5 | Switch readback | R26 (33k) from `12V_SW` to `SW_SENSE`, R27 (10k) from `SW_SENSE` to GND, `SW_SENSE` to Nano **A2**. Same 4.3:1 ratio as the relay output readbacks. The divider is also what pulls `12V_SW` to ground when Q5 is open. |
| 6 | Optional fault lamp (DNP) | J3 2-pin header, R28 (1k) from `12V_RAW` to J3 pin 1 (`LAMP_P`), J3 pin 2 (`LAMP_N`) through R29 (1k) to the Q7 collector, Q7 (2N2222A) emitter to GND. Nano **D4**, net `D4_FAULT`, **HIGH = lamp on**, R30 (1k) to the Q7 base, R31 (10k) from D4 to GND. All six parts carry KiCad's do-not-populate attribute in the schematic and on the board. |

Why two 1k resistors on the lamp: an LED wires directly across J3 with no
other part, and either lamp wire may be shorted to ground or to the bus
outside the box without harm. Normal lamp current is about 5 mA; a short gives
at most 14 mA and 0.2 W in one resistor.

Net-level result, from the KiCad netlist export of the Rev 3.4 schematic:

| Net | Pins |
|---|---|
| `12V_RAW` | C2.1 D5.1 F1.2 PS1.1 **Q5.1** **R23.1** **R28.2** |
| `12V_SW` | Q1.1 Q2.1 R7.2 R8.1 **Q5.3** **R26.1** |
| `Net-(Q5-B)` | Q5.2 R22.2 R23.2 |
| `Net-(Q6-C)` | Q6.3 R22.1 |
| `Net-(Q6-B)` | Q6.2 R24.2 |
| `D5_DRV_EN` | A1.8 (D5) R24.1 R25.1 |
| `SW_SENSE` | A1.21 (A2) R26.2 R27.1 |
| `D4_FAULT` | A1.7 (D4) R30.1 R31.1 |
| `Net-(Q7-B)` | Q7.2 R30.2 |
| `Net-(Q7-C)` | Q7.3 R29.2 |
| `LAMP_P` | J3.1 R28.1 |
| `LAMP_N` | J3.2 R29.1 |
| GND gains | Q6.1 Q7.1 R25.2 R27.2 R31.2 |

Every other net is the same as Rev 3.3. Compared with Rev 3.3, `12V_RAW`
loses exactly Q1.1, Q2.1, R7.2 and R8.1.

Nano pins newly used: **D4** (fault lamp), **D5** (driver enable), **A2**
(switch readback). D3, D10, D11, D12, D13, A0 and A1 remain free.

## What changed — layout

The layout rules are those of Rev 3.3 and none was relaxed: every track on the
top layer, the bottom layer one unbroken ground pour, no vias, the magneto
nets in the 2.5 mm class, no different-net copper closer than 0.6 mm.

Two things made room without vias.

- **Q5 sits in line on the 12 V spine.** The spine comes down under the four
  bridge resistors and now ends on the Q5 emitter; the switched rail leaves
  from the Q5 collector and continues south along the same line to the DEC
  driver, with a short branch west to the Q1 emitter. The 12 V entry rail
  moves 2.76 mm north (from y = 71.26 to y = 68.5) to meet the spine above Q5.
- **The opto output `D2_RPM` no longer runs through the channel between the
  Nano's pin rows.** In Rev 3.3 it entered the channel from the south and
  ended on pin D2 from the inside, which fenced in pins D3 to D5. It now runs
  up the west side of the Nano under the bodies of R6, R12, R26, R10 and R5.
  That frees the channel for D4 and D5 (southbound) and for the two new lines
  from the driver area (`SW_SENSE` and the Q5 base drive), which enter it
  from the north.

Parts that moved (pad 1 position, mm):

| Part | From | To | Why |
|---|---|---|---|
| R7 | (105.50, 73.26) | (105.50, 80.40), 7.14 mm south | Its row is where Q5 now sits. It is fed from the switched spine by a short branch from below. |
| R11 | (113.12, 76.46) | (113.12, 77.20), 0.74 mm south | Courtyard clearance to Q5 |
| D4 | (103.90, 84.20) | (102.90, 84.20), 1.00 mm west | Courtyard clearance to R26 |
| R2 | (127.90, 54.80) | (127.90, 56.20), 1.40 mm south | Opens two track lanes north of its lower pad |
| R15 | (119.50, 66.50) vertical | (149.40, 91.28) horizontal | Divider bottom moves to the Nano end of `INC_SENSE` (beside pin A3); frees the area east of Q5 for R22 and R23 |
| R17 | (123.50, 63.30) vertical | (149.40, 83.66) horizontal | Same, on `DEC_SENSE` (beside pin A6) |
| R20, R21 | x = 158.00, 162.00 | x = 160.00, 164.00, 2.00 mm east | Courtyard clearance to the relocated R17 |

Everything else is where Rev 3.3 has it: 39 footprints did not move, among
them J1, J2, A1, PS1, the four mounting holes, Q1 to Q4, F1, D5, C2 and U1.
The outline is identical (84.998 to 185.072 by 42.028 to 122.111 mm).

New parts (pad 1 position): Q5 (108.70, 69.30), R23 (120.42, 69.90) and R22
(125.12, 73.10) east of Q5; R26 (115.78, 86.60) between the two driver cells;
R27 (149.40, 94.70) beside Nano pin A2; Q6 (pad 2 at 151.20, 111.14), R24
(154.60, 118.22) and R25 (158.00, 118.22) south-east of the Nano; the optional
lamp parts R31 (161.40, 116.20), R30 (164.80, 116.20), Q7 (pad 2 at 169.10,
108.58), R29 (174.10, 103.44), R28 (173.50, 99.24) and J3 (177.03, 100.90),
directly below J2, 8 mm in from the right-hand board edge.

Layout notes:

- Three series resistors double as crossings, in the same way Rev 3.3 uses
  its bridge resistors: R26 passes over `D2_RPM`, R22 passes over `SW_SENSE`,
  and R28 passes over the three lines to J2 (5 V, SCL, SDA).
- R28 uses the 15.24 mm pitch resistor footprint (the one R2 has had since
  Rev 3.3) because it has to span those three lines. It is the only new
  resistor that does not use the standard 7.62 mm footprint.
- `12V_SW` is routed at 1.0 mm, like `12V_RAW`. Its two branches that carry
  only a pull-up or the readback divider (to R7 and to R26) are 0.5 mm.
- The fault lamp's 12 V feed is taken from the regulator header pin (PS1.1)
  and runs 0.5 mm wide down the right-hand side of the board to R28. R28 is
  the first part on it, so everything beyond R28 is current-limited.
- `12V_SW` is added to the POWER net class in the project file.
- Silkscreen: "REV 3.4"; "FAULT LAMP" below J3 and "+" beside its pin 1.

## Verification

All run on the committed files with KiCad 10.0.1 `kicad-cli` and its bundled
python, after the final run of `build_scripts/rebuild.sh`.

| Check | Result |
|---|---|
| ERC | 1 error, the same one Rev 3.3 has: `power_pin_not_driven` on the Nano's GND pin 4 (the schematic has no power-flag symbol; the pin is on the GND net). Warnings rose from 69 to 88, all in two cosmetic classes that every cloned symbol raises (`lib_symbol_mismatch`, `lib_symbol_issues`). No new class of message. |
| DRC with schematic parity and zone refill | **0 violations, 0 unconnected pads, 0 footprint errors**, 0 schematic parity issues (`hardware/DRC.rpt`) |
| Netlist | As tabulated above. Q1.1, Q2.1, R7.2 and R8.1 are on `12V_SW`; the only other pins on it are Q5.3 and R26.1. The board's pad nets were read back and match. |
| Do-not-populate | J3, Q7, R28, R29, R30, R31 carry the DNP attribute in the schematic and on the board; no other part does. |
| Copper to copper, different nets, pours excluded | Minimum **0.600 mm** (the three analog taps up the left margin, unchanged from Rev 3.3). The closest new copper is 0.650 mm (D4's ground pad to the `12V_SW` branch feeding R7). |
| Pour to anything | 0.5005 mm (zone clearance 0.5 mm, unchanged) |
| Magneto nets | 2.5005 mm to the pours; **2.81 mm** to the nearest other copper (R9 pad 1 to D6's ground pad). Both unchanged from Rev 3.3; no new copper is near them. |
| Tracks | 176 segments, all on F.Cu (Rev 3.3: 129). Widths 0.4, 0.5, 0.8, 1.0 mm. |
| Vias | **0** |
| Bottom pour | **One piece**, 6879 mm² (Rev 3.3: 7028 mm²; the difference is the 31 new holes) |
| Holes | 169 plated (Rev 3.3: 138), 8 drill sizes, largest 3.2 mm |
| Rebuild | `sh hardware_rev3_4/build_scripts/rebuild.sh` regenerates the schematic, the board and every output from the Rev 3.3 design files and ends with the DRC result above. |

The clearance figures come from a probe DRC on a scratch copy of the project
with temporary strict rules (1.2 mm between non-pour copper, 0.56 mm to pours,
4.0 mm and 2.6 mm for the magneto class), reading the "actual" value KiCad
reports for each pair. The pour figures were read without refilling, because a
refill would apply the temporary rule.

## Firmware (v3.1)

Written after the board, in the main firmware tree, and covered by its unit
tests.

- **D5** (`PIN_DRV_EN`): output. HIGH closes Q5 and powers both relay drivers;
  LOW opens it, and so does a pin left floating (plain `INPUT`, reset,
  bootloader) because of R25. **Never `INPUT_PULLUP`**; the same goes for D4.
- **A2** (`PIN_SW_SENSE`): `12V_SW` divided by 4.3, same scaling as A3 and A6.
  About 2.8 V with the switch closed on a 12 V bus, 0 V with it open.
- **D4** (`PIN_FAULT_LAMP`): output, HIGH lights the lamp if the lamp parts
  are fitted. Harmless to drive when they are not.
- **At power-up** the firmware reads A2 with the switch still open. If
  `12V_SW` is already live, Q5 has failed short and can no longer protect
  anything: that is reported on the console (`DRIVER SW: STUCK_ON`) and the
  governor runs on, as it would have on Rev 3.3. Then it closes the switch,
  and it stays closed in normal operation.
- **Trip.** A relay output that reads live for 200 ms while it is commanded
  off opens the switch, which stops the prop where it is. From fault to open
  switch is at most about 0.4 s.
- **A trip stays until power is cycled** (prop switch out of AUTO and back).
  Bill's ruling, 2026-11-15: a shorted transistor does not heal, and closing
  the switch again would only re-energise the relay. It is the one latched
  state in the firmware. If the short is still there after the power cycle,
  the relay is energised for under half a second and the switch opens again.
- With Q5 open both relay outputs read low on A3/A6 whatever Q1 and Q2 are
  doing, so the outputs are only judged while the switch is closed.
- **Fault lamp**, if fitted: steady after a trip, and lit for one second at
  power-up as a lamp test.
- An output that is dead while commanded on, and a switch that reads dead
  while closed, are reported on the console only. Neither changes what the
  governor does.

## Not yet verified

1. **Nothing has been built or powered.** No board of this revision exists.
2. **Voltage at the relay coil.** Q5 adds a second saturated transistor in
   series with the coil, roughly 0.2 to 0.5 V more drop on top of Q1/Q2's.
   Whether the relays still pull in reliably at low bus voltage depends on
   the coil current, which has still not been measured (it was already open
   on Rev 3.3: R11/R13 and now R22 are sized for coils up to about 250 mA).
3. **Q5 carries the same current as the driver it feeds.** A fault that
   destroys Q1 or Q2 by overcurrent (an output shorted to ground) passes the
   same current through Q5. The switch protects against a driver that has
   failed short into a normal relay coil; it is not independent of an
   overcurrent event.
4. **`D2_RPM` has a new route.** The opto output now passes through the
   driver area (under R5, R10, R26, R12 and R6, about 2 mm from the D6 and D7
   lines) instead of through the Nano channel. It is a 5 V logic line with a
   4.7k pull-up and the neighbours switch at 10 Hz, so no trouble is expected,
   but it has not been looked at on a scope.
5. **Long, thin lines.** The Q5 base drive (`Net-(Q6-C)`, 69 mm) and
   `SW_SENSE` (70 mm) run from the driver area through the Nano channel. Both
   are behind a resistor at the 12 V end (R22 1k with R23 10k to the rail;
   R26 33k).
6. **Relocated divider bottoms.** R15 and R17 now sit at the Nano end of
   their sense lines instead of the driver end. Electrically the same net;
   not tested.
7. Everything listed as not yet verified in `../hardware_rev3_3/REV3_3_CHANGES.md`
   still applies (relay coil current, P-lead level at pin H, F1 fit,
   transistor pinout, early-wiring back-feed).

## Files

- `hardware/350A_governor.kicad_sch`, `.kicad_pcb`, `.kicad_pro`, `.kicad_prl`
  — the design (KiCad 10)
- `hardware/Schematic.pdf`, `hardware/DRC.rpt`, `board_top.png`,
  `board_bottom.png` — review outputs
- `hardware/gerbers/` — Gerbers, drill file and `350A_governor_JLCPCB.zip`
  (same Protel file naming as Rev 3.3)
- `hardware/BOM.md` — parts list; the optional lamp parts are in their own
  section
- `build_scripts/` — `rebuild.sh` and the four scripts it runs
  (`edit_sch34.py`, `build_pcb34.py`, `placement34.py`, `routes34.py`) plus
  `drcsum.py`. The inputs are the two Rev 3.3 design files, which the scripts
  never write.
