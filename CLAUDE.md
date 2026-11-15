# CLAUDE.md — Beech215PropController

Open-source replacement electronics for the Airborne Electronics 350A
solid-state propeller governor (STC SA2700WE), for Beechcraft Bonanzas
(straight 35 through G35) with the Beech 215-210 electric prop. Replaces the
original bang-bang control with proportional relay drive. Only the electronics
inside the governor box change. The 7-pin Cannon connector **and the original
panel RPM knob** (a 10k pot across pins E and F) are preserved exactly, and
nothing else connects to the box.

**Design constraint (Bill, 2026-11-14, relaying his mechanic): the unit must
be functionally identical to the 350A it replaces** — same connector, same
panel knob, same behaviour in use. The change is fully disclosed on the 337;
the point is equivalence, not concealment. That ended the rotary-switch
panel, the presets and the OLED as an aircraft feature. Do not propose adding
panel hardware, extra wiring or new pilot-facing behaviour. The one thing Bill
has allowed beyond that (2026-11-15) is the optional fault lamp on Rev 3.4:
pads only, nothing fitted or wired by default.

This is Bill's own repo (`billmallard/Beech215PropController`, MIT), not a
MakerPlane fork — the `makerplane/` pipelines (issue groomer, overnight
worker, oracle gate) do not apply here. Org ledger row lives in
`../STATE.md`.

## Safety context — read before touching firmware

This is flight hardware that controls prop pitch on a real aircraft
(owner-produced part under 14 CFR 21.303; installation is a major alteration
requiring Form 337 + IA — see [docs/REGULATORY.md](docs/REGULATORY.md)).
The safety layers ARE the product:

- 250ms hardware watchdog — `wdt_reset()` is the first line of `loop()`
- Mag-signal loss (2s timeout) de-energizes both relays. Non-latching since
  v3.0, like the original: governing resumes once valid RPM is back
- Panel-pot open circuit (reading above ~16k for 300ms) de-energizes both
  relays; non-latching
- A reading below 400 RPM de-energizes both relays; non-latching, and it
  resumes straight against the knob setting. There is deliberately **no
  upper RPM limit**: a high reading gets a decrease command, as on the
  original (Bill, 2026-11-15: an overspeed is when it must not go quiet)
- Setpoint ramp (100 RPM/s) so knob movements never slam full duty. The
  tracked setpoint is clamped between actual RPM and the knob setting, so
  the ramp can never make the governor drive away from the knob
- The pulse interrupt rejects intervals under 2ms (magneto ringing)
- Relay output readback and the series switch (Rev 3.4, firmware v3.1): a
  relay output that is live for 200ms without being commanded means an
  output transistor has failed short. The firmware opens Q5, which removes
  the supply from both relay drivers, and it stays open until power is
  cycled

Never weaken one of these to fix a bug elsewhere. A control-loop change is
not done until it has a matching unit test and, where behavior on the bench
changes, a note in [docs/BENCH_TEST.md](docs/BENCH_TEST.md).

**Nothing in the firmware latches, with one exception** (Bill, 2026-11-15).
The aircraft has no reset except the prop switch and the original has no
latched state, so every protective condition is a standby that clears by
itself. Do not add an alarm state back. The exception is the driver trip
above: Bill ruled it stays open until the prop switch is cycled, because a
shorted transistor does not heal and closing the switch again would only
re-energise the relay. Do not make it retry, and do not let anything else
latch.

There is deliberately **no relay timeout**. An 8s one existed until
2026-11-15 and was removed: the original holds its output until the RPM is
reached, the aircraft's limit switches stop the motor at the pitch stops,
and a failed limit switch costs gear wear, not control (Bill: the manual
circuit has no limit switches at all). Do not add one back.

**Only one readback result acts: a relay output live while not commanded.**
An output that is dead while commanded on, and both faults of the series
switch itself, are reported on the console and change nothing the governor
does. A false report must never be able to stop a working governor, so do
not add an action to any of them without Bill's ruling. The pilot is not
told about a trip unless the optional lamp is fitted; the sign is the
tachometer, as with any governor failure on the original.

## Layout

| Path | What |
|---|---|
| `firmware/governor_350A/` | The entire firmware: one sketch (`governor_350A.ino`, v3.1, needs Rev 3.4), `platformio.ini`, `test/` |
| `hardware_rev3_4/` | CURRENT hardware (designed 2026-11-15, **not yet built or tested**): `REV3_4_CHANGES.md`, board renders, `build_scripts/`, `hardware/` (KiCad project, `Schematic.pdf`, `BOM.md`, `DRC.rpt`, `gerbers/` JLCPCB zip) |
| `hardware_rev3_3/` | Rev 3.3, superseded by 3.4 before any board was made. `REV3_3_CHANGES.md` is still the record of the 3.2 to 3.3 changes, and it is the input to the 3.4 build scripts: do not edit its design files |
| `hardware_rev3_2/` | Previous revision (rotary-switch panel). Has known faults; reference only |
| `hardware_archive/` | rev1 / rev3 / rev3_1 / april2026 history — reference only, never edit |
| `docs/` | ASSEMBLY, BENCH_TEST, WIRING, REGULATORY, TROUBLESHOOTING |
| `docs/original_350A/` | Scanned original Airborne Electronics documentation (Richard Keiter) — reference only. `gov_man.pdf` is text-extractable and is the authority on the original interface |
| `350A_Digital_Governor_Spec.docx` | Design spec (repo root; predates Rev 3.3 and 3.4) |
| `tools/security/scan_secrets.py` | Pre-commit secret scan (policy in `SECURITY.md`) |

## Build and test

`firmware/governor_350A/` is the PlatformIO root — run commands from there:

- `pio run` — AVR build (env `nanoatmega328`; Adafruit SSD1306 + GFX)
- `pio run -t upload` then `pio device monitor --baud 9600` — flash + console
- `pio test -e native` — 88 Unity tests, no hardware needed. Needs `g++` on
  PATH, which this Windows machine does not have: use the MSVC recipe in
  [firmware/governor_350A/README.md](firmware/governor_350A/README.md).
  `test/mocks/` stubs all Arduino/AVR headers and the test file `#include`s
  the `.ino` directly as one C++ translation unit. Trap: a bare `pio run`
  (no `-e nanoatmega328`) prunes Unity from `.pio/libdeps/native`; the test
  build then fails while the old `test_firmware.exe` still runs and says OK.
  Delete the exe before rebuilding, and restore Unity with
  `pio test -e native` (it errors at link here, which is expected).
- CI parity build: `arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old`
  (what `codeql.yml` compiles)

CI is CodeQL (c-cpp, builds the sketch) + Gitleaks, on push/PR/weekly.
Pre-commit runs the secret scan on staged text files (`pre-commit install`
after clone).

## Firmware truths

- Single-file sketch is deliberate (Arduino IDE users build this); comment
  density is high on purpose. Don't split into modules without Bill's OK.
- `PULSES_PER_REV = 3` (6-cyl Continental) is the must-set-per-engine
  constant; 2 for 4-cylinder. `POT_RPM_MAX` (2650, knob full clockwise) and
  `POT_RPM_MIN` (1800, full counter-clockwise) are the real range for
  Bill's aircraft (confirmed 2026-11-15).
- Setpoint comes only from the panel pot on A7: `Rpot = 10k * adc / (1023 -
  adc)`, linear in resistance. No presets, no EEPROM use.
- Boots into AUTO and waits in standby until magneto pulses appear (the
  aircraft's prop switch is what powers the box). Serial `m`/`a` are bench
  overrides.
- Control: 10Hz loop; relay duty is time-proportioned within each 100ms
  window (slow on/off for relay contacts, NOT fast PWM). Deadband ±25 RPM,
  proportional zone 75 RPM, minimum duty 25%.
- Resources (v3.1, `pio run -e nanoatmega328`): 78% flash, 35% static RAM
  (720 bytes). The Adafruit display buffer takes another 1KB of heap when a
  display is fitted, which leaves about 300 bytes of stack on the bench, so
  keep print strings in flash with `F()` and avoid `String`.
- Serial console at 9600: `s` status, `a`/`m` auto/manual,
  `?` help.
- The display is optional and only probed at power-up.
- D5 (series switch) and D4 (fault lamp) must be driven as outputs. Never
  `INPUT_PULLUP`: against the board's 10k pull-downs that leaves about 1V on
  the pin and half-turns-on the transistor behind it.

## Hardware truths

- Current line is **Rev 3.4** (2026-11-15) = Rev 3.3 plus the series switch
  and pads for an optional fault lamp. No board of this revision exists yet,
  and Rev 3.3 was never made. Rev 3–3.2 boards must not be built: the PNP
  bases had no current limit, the 6N137 enable pin was grounded, and the fuse
  footprint was for a 0.12A part. Full list:
  `hardware_rev3_3/REV3_3_CHANGES.md`; the 3.4 additions:
  `hardware_rev3_4/REV3_4_CHANGES.md`.
- J1: pin 5 (E) = pot sense with 10k pull-up to 5V, pin 6 (F) = ground,
  pin 7 (H) = magneto, offset one header position for clearance. Nano A7 =
  pot, A3/A6 = relay output readback, A2 = series switch readback, D2 = RPM,
  D6/D7 = relay drive, D5 = series switch (HIGH = closed), D4 = fault lamp.
- The magneto isolating resistor (10k) is in the aircraft harness at the
  magneto, not in the box; the board adds R1 + R9 (5.1k each). The opto needs
  about 5mA, so it depends on P-lead peaks of roughly 100V. **Not yet measured
  on the aircraft.** Relay coil current is also unmeasured.
- Relay driver is NPN (2N2222A) + PNP (2N2907A) high-side pairs with 1k base
  resistors. Flyback diodes D3/D4 sit across each relay coil circuit
  (cathode to the INC/DEC output, anode to GND). A third pair, Q6 + Q5,
  switches the 12V feed to both (`12V_SW`). The fault-lamp parts (J3, Q7,
  R28–R31) are do-not-populate.
- Aircraft wiring that matters to the design (figures in
  `docs/original_350A/`): the two relays feed the two motor fields; the 10A
  prop breaker feeds manual and automatic control alike; the box and the
  relay contacts are powered only with the prop switch in AUTO (later
  wiring, D-1821 on). On early wiring manual operation puts bus voltage on
  leads C and D with the box unpowered.
- Layout rule for this board: all tracks on the top layer, bottom layer an
  unbroken ground plane. Magneto nets are in netclass `MAG_HV` (2.5mm).
- 6N137 optocoupler is speed-critical — no 4N25/4N35/PC817 substitutes.
- KiCad files are **KiCad 10 format** (9.0 and 10.0 both installed — 9.0
  cannot open them). CLI toolchain that works headless:
  `C:\Program Files\KiCad\10.0\bin\kicad-cli.exe` (erc/drc/netlist/gerbers/
  render; `pcb drc --refill-zones --save-board` refills pours with project
  rules) and the bundled `python.exe` with `pcbnew` for board edits.
- Rev 3.4 is generated by `hardware_rev3_4/build_scripts/rebuild.sh` from
  the Rev 3.3 design files (Rev 3.3 itself by its own `rebuild.sh`). Two
  scripting gotchas: `pcbnew` return types break after `BOARD.Remove()` (strip
  items from the file text before loading instead), and Git Bash heredocs
  halve backslashes in inline Python (write scripts to files).

## Conventions and gotchas

- Workspace standards apply: no emojis, focused signed commits.
- `main` is the only long-lived branch. `claude/relaxed-hellman`
  (local + origin) is fully merged leftover history.
- The repo moved into `MAOS\` in July 2026. If `git status` ever dies with
  `fatal: not a git repository: ... .git/worktrees/<name>`, a Claude
  worktree's absolute gitdir pointer went stale in the move — run
  `git worktree repair <worktree-path>` from the repo root (last needed
  2026-07-25; that worktree was removed).
