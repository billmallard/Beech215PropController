# Hardware Rev 3.2 — Change Record and Verification Evidence

Date: 2026-07-25. Base: Rev 3.1. Tooling: KiCad 10.0 (files are KiCad 10
format — KiCad 9 cannot open them).

## Why

Flight-test engineering review of Rev 3/3.1: the flyback diodes should sit
across each relay coil circuit — "cathode on the relay drive output, anode to
ground" — not across the 2N2907A. Analysis confirmed, and worse than
reported: with a high-side PNP sourcing +12V into a ground-returned coil, the
turn-off transient drives the output node *negative*, so a diode to the +12V
rail never conducts. Rev 3/3.1 as built had **no functional flyback clamp**;
the coil energy avalanched the 1N4001 (reverse, ≥50V) or the 2N2907A
(V_CEO 60V) on every relay turn-off and radiated a ~40V negative spike into
the aircraft harness (J1-3/4, alongside the mag P-lead sense on J1-7).

## What changed

| # | Change | Detail |
|---|---|---|
| 1 | D3 reconnected | was: A=/DEC_RELAY, K=/12V_RAW → now: **K=/DEC_RELAY, A=GND** |
| 2 | D4 reconnected | was: A=/INC_RELAY, K=/12V_RAW → now: **K=/INC_RELAY, A=GND** |
| 3 | Layout | Both diode footprints rotated 180° in place (cathode lands on the existing relay trace); anode pads ground through the B.Cu pour (thermal relief); the two 12V spur tracks feeding the old cathodes deleted |
| 4 | 12V rail reroute | On 3.1 the 12V rail routed *through* D3's old cathode pad; the rail corner now detours around that pad (45° elbow, 0.38 mm copper gap vs 0.2 mm rule) |
| 5 | Silkscreen | "REV 3.2" added at bottom center — the only visual identifier distinguishing fixed boards from Rev 3/3.1 stock |
| 6 | Title blocks | Schematic + board stamped Title/Date/Rev 3.2 |

No other component, footprint, or net was touched.

## Verification

- **Netlist diff (schematic, KiCad 10 CLI export)** — the only changes across
  the entire design:
  - `/12V_RAW`: −D3.1, −D4.1 (all other members unchanged)
  - `/DEC_RELAY`: D3.2→D3.1 (anode out, cathode in)
  - `/INC_RELAY`: D4.2→D4.1
  - `GND`: +D3.2, +D4.2
- **ERC**: 22 errors / 49 warnings baseline (3.1) → 22 / 51. Zero new
  errors; the +2 warnings are the same cosmetic embedded-library-mismatch
  class carried by every pre-existing GND symbol.
- **DRC (with `--schematic-parity`)**: **0 violations, 0 unconnected pads.**
  The 2 remaining "footprint" warnings (D1/D2 LED local overrides) are
  pre-existing and identical in 3.1.
- **Connectivity**: ratsnest fully closed after zone refill (anode pads pick
  up the B.Cu GND pour).
- Renders: `board_top.png` / `board_bottom.png`; printable schematic at
  `hardware/Schematic.pdf`.

## Latent Rev 3.1 defect cleared in passing

DRC on the Rev 3.1 files **as shipped** reports 4 clearance **errors**:
VCC5 tracks on B.Cu touching the GND pour (0.0 mm actual) near
(168–173, 60) — the stored zone fill was stale relative to those tracks. Any
gerbers cut from that file state carry a potential 5V-to-GND short. The Rev
3.2 zone refill re-poured with correct clearance; all four are gone. If any
Rev 3.1 boards were actually fabricated, check 5V-to-GND resistance before
first power-up.

## Existing boards

Every board fabricated before 3.2 (April 2026 PCBWay Rev 3 run, any Rev 3.1
builds) requires the hand rework in
[docs/REWORK_FLYBACK_DIODES.md](../docs/REWORK_FLYBACK_DIODES.md) before
flight.

## Fabrication

Upload `hardware/gerbers/350A_governor_JLCPCB.zip` per
[hardware/JLCPCB_INSTRUCTIONS.md](hardware/JLCPCB_INSTRUCTIONS.md). Same
board size, layer count, and settings as 3.1.
