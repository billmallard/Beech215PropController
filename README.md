# Beechcraft Bonanza Electric Prop — Digital Governor Controller
### Open-Source Replacement for the Airborne Electronics 350A

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: Arduino](https://img.shields.io/badge/Platform-Arduino%20Nano-blue)](https://www.arduino.cc/)

> **A modern, proportional-control replacement for the Airborne Electronics 350A solid-state propeller governor — for Beechcraft Bonanza models with the Beech 215-210 electric prop.**

---

## The Problem This Solves

The original Airborne Electronics 350A (covered under STC SA2700WE) uses a **bang-bang control architecture** — the prop motor runs at full voltage until the RPM crosses the target, then reverses. This causes constant RPM hunting, which is made worse by:

- Freshly overhauled prop motors with more torque than the 50-year-old electronics expected
- Aging capacitors and resistors in the original controller that shrink the deadband
- The original panel rheostat giving no indication of what RPM is actually set
- Rough magneto signal from high-time mags degrading the RPM sensing

**This project replaces only the electronics inside the governor box.** The 7-pin Cannon connector, airframe wiring, prop motor, limit switches, and relays are all unchanged. From the aircraft's perspective, the interface is identical to the original.

## What It Does Differently

| Feature | Original 350A | This Controller |
|---|---|---|
| Control type | Bang-bang (full voltage on/off) | Proportional — slows motor near setpoint |
| Deadband | Fixed by aging analog components | Software-configurable (default ±25 RPM) |
| RPM display | None — knob position only | OLED shows set RPM and actual RPM |
| Presets | None | 5 named presets (Max, Cruise Climb, Cruise, Economy, Emergency) |
| Setpoint transition | Instant jump (causes full-duty relay surge) | Soft ramp 100 RPM/sec — smooth, no lurch |
| Fail-safe | RPM signal loss → no action | Loss of mag signal → relays de-energize, alarm displayed |
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

The system consists of two units:

**Governor Box** (replaces the 350A housing, mounts in the same location):
- Arduino Nano (ATmega328P)
- 6N137 optocoupler (galvanic isolation of magneto P-lead)
- ULN2003A Darlington array (relay driver)
- LM2596S-5V DC-DC switching regulator
- Supporting passives

**Panel Unit** (replaces the knob/rheostat assembly):
- SSD1306 1.3" OLED display (128×64, I2C)
- 5-position rotary switch (one detent per preset — Lorlin CK1024 or Grayhill 56-series)
- Original prop control knob reuses directly on the switch shaft

The PCB is a 100mm × 80mm 2-layer FR4 board, designed for JLCPCB fabrication at approximately **$2 for 5 boards**. The panel cable is 10 conductors (simplified from earlier designs).

---

## Repository Structure

```
bonanza-prop-governor/
├── README.md                    ← You are here
├── docs/
│   ├── ASSEMBLY.md              ← Step-by-step build instructions
│   ├── BENCH_TEST.md            ← Ground test procedure before aircraft installation
│   ├── WIRING.md                ← Connector pinout and wiring reference
│   ├── REGULATORY.md            ← FAA owner-produced parts guidance, 337 language
│   └── TROUBLESHOOTING.md       ← Fault codes, common issues
├── firmware/
│   └── governor_350A/
│       ├── README.md            ← Flashing guide, serial console, unit tests
│       ├── governor_350A.ino    ← Arduino sketch (fully commented)
│       ├── platformio.ini       ← PlatformIO build config (AVR + native test)
│       └── test/                ← Unity unit tests (runs on desktop, no hardware)
└── hardware/
    ├── 350A_governor.kicad_pcb  ← KiCad PCB file (open in KiCad 7+)
    ├── 350A_governor_JLCPCB.zip ← Ready-to-upload Gerber package for JLCPCB
    └── BOM.md                   ← Bill of materials with part numbers and sources
```

---

## Quick Start

1. **Order the PCB** — Upload `hardware/350A_governor_JLCPCB.zip` to [jlcpcb.com](https://jlcpcb.com). Default settings (2-layer, FR4, 1.6mm, HASL, green) are fine.
2. **Order components** — See [hardware/BOM.md](hardware/BOM.md). Total ~$55–95 for a complete build.
3. **Assemble** — Follow [docs/ASSEMBLY.md](docs/ASSEMBLY.md). All through-hole components; no SMD soldering.
4. **Flash firmware** — See [firmware/governor_350A/README.md](firmware/governor_350A/README.md) for full instructions (Arduino IDE, PlatformIO, and CLI). Set `PULSES_PER_REV` for your engine before flashing.
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
- Hardware revisions (alternative component footprints, panel unit PCB)
- Flight test data and tuning recommendations

Please open an issue before starting significant work so efforts aren't duplicated.

---

## License

MIT License. See LICENSE file. This hardware and firmware may be freely built, modified, and used for personal aircraft. Commercial reproduction or sale requires separate arrangement.

---

## Acknowledgments

Richard Keiter of Airborne Electronics, Healdsburg CA, designed the original 350A and made it repairable by owners. This project is built on his philosophy. The complete original documentation is preserved in [docs/original_350A/](docs/original_350A/) for reference.

---

*This project is not affiliated with Beechcraft, Textron Aviation, or Airborne Electronics. It is an independent owner-community project. Install only with appropriate FAA authorization.*
