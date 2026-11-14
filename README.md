# Beechcraft Bonanza Electric Prop — Digital Governor Controller
### Open-Source Replacement for the Airborne Electronics 350A

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: Arduino](https://img.shields.io/badge/Platform-Arduino%20Nano-blue)](https://www.arduino.cc/)

> **A modern, proportional-control replacement for the Airborne Electronics 350A solid-state propeller governor — for Beechcraft Bonanza models with the Beech 215-210 electric prop.**

> **Status, 2026-11-14:** hardware Rev 3.3 and firmware v3.0 are design-complete and unit-tested. **No Rev 3.3 board has been built, bench-tested or flown.** Earlier board revisions have known faults and must not be built. See [hardware_rev3_3/REV3_3_CHANGES.md](hardware_rev3_3/REV3_3_CHANGES.md).

---

## The Problem This Solves

The original Airborne Electronics 350A (covered under STC SA2700WE) uses a **bang-bang control architecture** — the prop motor runs at full voltage until the RPM crosses the target, then reverses. This causes constant RPM hunting, which is made worse by:

- Freshly overhauled prop motors with more torque than the 50-year-old electronics expected
- Aging capacitors and resistors in the original controller that shrink the deadband
- Rough magneto signal from high-time mags degrading the RPM sensing

**This project replaces only the electronics inside the governor box.** The 7-pin Cannon connector, airframe wiring, panel RPM knob, prop motor, limit switches, and relays are all unchanged. From the aircraft's perspective, the interface is identical to the original.

## What It Does Differently

| Feature | Original 350A | This Controller |
|---|---|---|
| Control type | Bang-bang (full voltage on/off) | Proportional — slows motor near setpoint |
| Deadband | Fixed by aging analog components | Software-configurable (default ±25 RPM) |
| Panel control | 10kΩ knob on connector pins E and F | The same knob on the same two wires. Nothing is added to the panel |
| RPM readout | None | None in the aircraft. An optional display can be plugged into the board on the bench |
| Setpoint transition | Follows the knob directly | Walks to the new knob setting at 100 RPM/sec — smooth, no lurch |
| Fail-safe | RPM signal loss → no action | Same: no magneto signal for 2 seconds → both relays off; governing resumes when the signal returns |
| Watchdog | None | Hardware watchdog resets MCU in 250ms if firmware stalls |
| Motor runaway protection | Limit switches only | 8-second relay timeout regardless of limit switch state |

## Who This Is For

Any Beechcraft Bonanza (straight 35 through G35, A35–G35) equipped with:
- The Beech 215-210 electric prop
- An existing Airborne Electronics 350A installation (STC SA2700WE)

If you have the stock mechanical governor, this is not applicable.

---

## Regulatory Note

This project is designed for **owner-produced parts** under 14 CFR Part 21.303. The STC SA2700WE defines the system by its external interface — the 7-pin connector, wire assignments, and functional behavior — all of which are preserved exactly. The electronics inside the governor housing are replaced. **This is a major alteration and requires FAA Form 337 approval signed by an IA.** See [docs/REGULATORY.md](docs/REGULATORY.md) for a detailed discussion and suggested 337 language.

---

## Hardware Overview

The system is one unit: the governor box. The panel knob and all aircraft wiring stay as they are.

**Governor Box** (replaces the 350A housing, mounts in the same location):
- Arduino Nano (ATmega328P)
- 6N137 optocoupler on the magneto P-lead input
- 2N2222A + 2N2907A high-side relay drivers (NPN level shifter + PNP switch per channel)
- 1N4001 flyback diodes across each relay coil circuit
- Panel-control input for the original 10kΩ knob (connector pins E and F), with open-wire detection
- 12V input protection: resettable fuse and transient clamp
- LM2596S-5V DC-DC switching regulator
- Optional 4-pin header for an SSD1306 display, for bench work only

Current hardware revision: **3.3** (2026-11-14; boards are marked "REV 3.3" on the silkscreen). Rev 3.3 returns the panel control to the original potentiometer and fixes faults found in a design review of Rev 3.2. It needs firmware v3.0. **Rev 3, 3.1 and 3.2 boards must not be built or flown**; the reasons are in [hardware_rev3_3/REV3_3_CHANGES.md](hardware_rev3_3/REV3_3_CHANGES.md).

The PCB is a 100mm × 80mm 2-layer FR4 board, designed for JLCPCB fabrication at approximately **$2 for 5 boards**.

---

## Repository Structure

```
Beech215PropController/
├── README.md                        ← You are here
├── docs/
│   ├── ASSEMBLY.md                  ← Step-by-step build instructions
│   ├── BENCH_TEST.md                ← Ground test procedure before aircraft installation
│   ├── WIRING.md                    ← Connector pinout and wiring reference
│   ├── REGULATORY.md                ← FAA owner-produced parts guidance, 337 language
│   ├── TROUBLESHOOTING.md           ← Fault codes, common issues
│   ├── REWORK_FLYBACK_DIODES.md     ← Mandatory rework for Rev 3/3.1 boards
│   └── original_350A/               ← Scanned original Airborne Electronics documentation
├── firmware/
│   └── governor_350A/
│       ├── README.md                ← Flashing guide, serial console, unit tests
│       ├── governor_350A.ino        ← Arduino sketch (fully commented)
│       ├── platformio.ini           ← PlatformIO build config (AVR + native test)
│       └── test/                    ← Unity unit tests (runs on desktop, no hardware)
├── hardware_rev3_3/                 ← CURRENT hardware revision (designed, not yet built)
│   ├── REV3_3_CHANGES.md            ← What changed in 3.3 and why, verification evidence, open items
│   ├── board_top.png / board_bottom.png
│   ├── build_scripts/               ← Scripts that generated the 3.3 schematic, board and outputs
│   └── hardware/
│       ├── 350A_governor.kicad_sch  ← Schematic (KiCad 10)
│       ├── 350A_governor.kicad_pcb  ← PCB layout (KiCad 10)
│       ├── Schematic.pdf            ← Printable schematic
│       ├── BOM.md                   ← Bill of materials with part numbers and sources
│       ├── JLCPCB_INSTRUCTIONS.md   ← PCB ordering steps
│       └── gerbers/
│           └── 350A_governor_JLCPCB.zip  ← Ready-to-upload Gerber package
├── hardware_rev3_2/                 ← Previous revision (rotary-switch panel). Reference only; do not build
└── hardware_archive/                ← Superseded revisions (rev1, rev3, rev3_1, april2026)
```

---

## Quick Start

1. **Order the PCB** — Upload `hardware_rev3_3/hardware/gerbers/350A_governor_JLCPCB.zip` to [jlcpcb.com](https://jlcpcb.com). Default settings (2-layer, FR4, 1.6mm, HASL, green) are fine.
2. **Order components** — See [hardware_rev3_3/hardware/BOM.md](hardware_rev3_3/hardware/BOM.md). Total ~$50–90 for a complete build.
3. **Assemble** — Follow [docs/ASSEMBLY.md](docs/ASSEMBLY.md). All through-hole components; no SMD soldering.
4. **Flash firmware** — See [firmware/governor_350A/README.md](firmware/governor_350A/README.md) for full instructions (Arduino IDE, PlatformIO, and CLI). Set `PULSES_PER_REV`, `POT_RPM_MAX` and `POT_RPM_MIN` for your engine before flashing.
5. **Bench test** — Build the test harness described in [docs/BENCH_TEST.md](docs/BENCH_TEST.md) and verify operation before touching the aircraft.
6. **Install** — Follow [docs/ASSEMBLY.md](docs/ASSEMBLY.md) aircraft installation section. Have your IA present.

---

## Engine Compatibility

Set `PULSES_PER_REV` in the firmware before flashing:

| Engine | Cylinders | PULSES_PER_REV |
|---|---|---|
| Continental IO-470 | 6 | 3 |
| Continental IO-520 | 6 | 3 |
| Continental E-185/E-225 | 6 | 3 |
| Lycoming O-435 | 6 | 3 |

All early Bonanzas use Continental 6-cylinder engines. `PULSES_PER_REV = 3` is correct for all of them.

---

## Contributing

This project is maintained by the Bonanza owner community. Pull requests welcome for:
- Firmware improvements (better control algorithms, additional safety checks)
- Documentation corrections
- Hardware revisions (alternative component footprints)
- Flight test data and tuning recommendations

Please open an issue before starting significant work so efforts aren't duplicated.

---

## Security

This repository uses layered secret-prevention controls. See [SECURITY.md](SECURITY.md) for the full policy including vulnerability reporting and incident response steps.

| Layer | Tool | Trigger |
|---|---|---|
| Local pre-commit | `tools/security/scan_secrets.py` | Every `git commit` (staged files) |
| CI — SAST | CodeQL (C/C++ + Python) | Push, PR, weekly |
| CI — Secret scan | Gitleaks | Push, PR, weekly (full history) |

**Quick setup after cloning:**
```
pip install pre-commit
pre-commit install
```

Recommended GitHub repository settings: enable **Secret scanning**, **Push protection**, and **Dependabot** (Settings → Security).

---

## License

MIT License. See LICENSE file. This hardware and firmware may be freely built, modified, and used for personal aircraft. Commercial reproduction or sale requires separate arrangement.

---

## Acknowledgments

Richard Keiter of Airborne Electronics, Healdsburg CA, designed the original 350A and made it repairable by owners. This project is built on his philosophy. The complete original documentation is preserved in [docs/original_350A/](docs/original_350A/) for reference.

---

*This project is not affiliated with Beechcraft, Textron Aviation, or Airborne Electronics. It is an independent owner-community project. Install only with appropriate FAA authorization.*
