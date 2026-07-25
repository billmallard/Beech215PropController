# CLAUDE.md — Beech215PropController

Open-source replacement electronics for the Airborne Electronics 350A
solid-state propeller governor (STC SA2700WE), for Beechcraft Bonanzas
(straight 35 through G35) with the Beech 215-210 electric prop. Replaces the
original bang-bang control with proportional relay drive, an OLED panel, and
five preset RPMs. Only the electronics inside the governor box change — the
7-pin Cannon connector interface to the airframe is preserved exactly.

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
- Mag-signal-loss alarm (2s timeout) de-energizes both relays
- RPM sanity band (400–3500) triggers alarm
- 8s max continuous relay drive (`MAX_RELAY_MS`) independent of limit switches
- Setpoint ramp (100 RPM/s) so preset changes never slam full duty
- EMERGENCY/LOW preset (switch position 5) bypasses ramping by design

Never weaken one of these to fix a bug elsewhere. A control-loop change is
not done until it has a matching unit test and, where behavior on the bench
changes, a note in [docs/BENCH_TEST.md](docs/BENCH_TEST.md).

## Layout

| Path | What |
|---|---|
| `firmware/governor_350A/` | The entire firmware: one sketch (`governor_350A.ino`, v2.0), `platformio.ini`, `test/` |
| `hardware_rev3_2/` | CURRENT hardware: `REV3_2_CHANGES.md`, board renders, `hardware/` (KiCad project, `Schematic.pdf`, `BOM.md`, `gerbers/` JLCPCB zip) |
| `hardware_archive/` | rev1 / rev3 / rev3_1 / april2026 history — reference only, never edit |
| `docs/` | ASSEMBLY, BENCH_TEST, WIRING, REGULATORY, TROUBLESHOOTING |
| `docs/original_350A/` | Scanned original Airborne Electronics documentation (Richard Keiter) — reference only |
| `350A_Digital_Governor_Spec.docx` | Design spec (repo root) |
| `tools/security/scan_secrets.py` | Pre-commit secret scan (policy in `SECURITY.md`) |

## Build and test

`firmware/governor_350A/` is the PlatformIO root — run commands from there:

- `pio run` — AVR build (env `nanoatmega328`; Adafruit SSD1306 + GFX)
- `pio run -t upload` then `pio device monitor --baud 9600` — flash + console
- `pio test -e native` — 24 Unity tests, no hardware needed. `test/mocks/`
  stubs all Arduino/AVR headers and the test file `#include`s the `.ino`
  directly as one C++ translation unit.
- CI parity build: `arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old`
  (what `codeql.yml` compiles)
- Windows without MinGW: manual MSVC recipe in
  [firmware/governor_350A/README.md](firmware/governor_350A/README.md)

CI is CodeQL (c-cpp, builds the sketch) + Gitleaks, on push/PR/weekly.
Pre-commit runs the secret scan on staged text files (`pre-commit install`
after clone).

## Firmware truths

- Single-file sketch is deliberate (Arduino IDE users build this); comment
  density is high on purpose. Don't split into modules without Bill's OK.
- `PULSES_PER_REV = 3` (6-cyl Continental) is the one must-set-per-engine
  constant; 2 for 4-cylinder.
- Control: 10Hz loop; relay duty is time-proportioned within each 100ms
  window (slow on/off for relay contacts, NOT fast PWM). Deadband ±25 RPM,
  proportional zone 75 RPM, minimum duty 25%.
- Resources are tight: ~78% flash (~6KB headroom), ~67% RAM (~680B free
  including stack). The Adafruit libs are most of it — avoid new `String`
  usage and large locals.
- Serial console at 9600: `s` status, `a`/`m` auto/manual, `c` clear alarm,
  `1`–`5` simulate switch position, `?` help.
- Presets 1–4 persist in EEPROM (magic `0xAE` at addr 0); defaults
  2650 / 2450 / 2300 / 2200.

## Hardware truths

- Current line is **Rev 3.2** (2026-07-25). Relay driver is NPN (2N2222A) +
  PNP (2N2907A) high-side pairs; the ULN2003A low-side driver was removed in
  Rev 3.
- Flyback diodes D3/D4 sit **across each relay coil circuit** (cathode to
  the INC/DEC output, anode to GND). Rev 3/3.1 had them across the PNPs —
  no clamping of the negative turn-off spike; caught by flight-test review.
  All pre-3.2 physical boards need `docs/REWORK_FLYBACK_DIODES.md`. Change
  record + verification evidence: `hardware_rev3_2/REV3_2_CHANGES.md`.
- 6N137 optocoupler is speed-critical — no 4N25/4N35/PC817 substitutes.
- KiCad files are **KiCad 10 format** (9.0 and 10.0 both installed — 9.0
  cannot open them). CLI toolchain that works headless:
  `C:\Program Files\KiCad\10.0\bin\kicad-cli.exe` (erc/drc/netlist/gerbers/
  render) and the bundled `python.exe` with `pcbnew` for board edits.

## Conventions and gotchas

- Workspace standards apply: no emojis, focused signed commits.
- `main` is the only long-lived branch. `claude/relaxed-hellman`
  (local + origin) is fully merged leftover history.
- The repo moved into `MAOS\` in July 2026. If `git status` ever dies with
  `fatal: not a git repository: ... .git/worktrees/<name>`, a Claude
  worktree's absolute gitdir pointer went stale in the move — run
  `git worktree repair <worktree-path>` from the repo root (last needed
  2026-07-25; that worktree was removed).
