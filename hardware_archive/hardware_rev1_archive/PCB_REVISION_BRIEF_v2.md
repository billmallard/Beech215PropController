# PCB Revision Brief — Governor 350A, Rev 2

**Summary:** Replace U2 (ULN2003A, DIP-16) with a two-channel NPN+PNP high-side relay driver. All other components, board outline, and connector positions are unchanged.

**Background:** The Bonanza relay coil circuit is high-side driven — the governor supplies +12V to the relay coil and the other end grounds through the pitch-limit switch. The ULN2003A is an NPN low-side switch (sinks to GND) and is incompatible with this topology. The replacement circuit uses a standard NPN level-shifter + PNP high-side switch pair per channel, which is driven directly from 5V logic and switches +12V to the relay coil. No firmware changes are required.

---

## Remove

- U2 (ULN2003A, DIP-16) and its socket footprint
- All traces connected to U2 pins

---

## Add — Two-Channel High-Side Driver

Two identical channels (INC and DEC), each consisting of one NPN transistor, one PNP transistor, and two resistors.

*Channel 1 — INC (ref des: Q3, Q1, R5, R7)*
*Channel 2 — DEC (ref des: Q4, Q2, R6, R8)*

| Ref | Part | Footprint | Value |
|---|---|---|---|
| Q1, Q2 | 2N2907A | TO-92, inline (EBC pin order) | PNP — high-side switch |
| Q3, Q4 | 2N2222A | TO-92, inline (EBC pin order) | NPN — level shifter |
| R5, R6 | Resistor | Through-hole, 1/4W, 0.1" pitch | 1kΩ |
| R7, R8 | Resistor | Through-hole, 1/4W, 0.1" pitch | 10kΩ |

### Connections per channel (shown for INC; DEC is identical)

```
Nano D6 ──── R5 (1kΩ) ──── Q3 Base
                            Q3 Emitter ──── GND
                            Q3 Collector ──+──── R7 (10kΩ) ──── +12V_RAW
                                           |
                                       Q1 Base
                                       Q1 Emitter ──── +12V_RAW
                                       Q1 Collector ──── J1-C (INC_RELAY net)
```

For the DEC channel: Nano D7 → R6 → Q4; Q4 collector → R8/Q2 base; Q2 collector → J1-D (DEC_RELAY net). All other connections identical.

### How it works

- **Relay ON:** Nano drives pin HIGH (5V) → Q3/Q4 NPN turns on → collector pulled to ~GND → Q1/Q2 PNP base near GND → V_BE ≈ −12V → PNP fully on → +12V appears on J1-C or J1-D → relay energized
- **Relay OFF:** Nano drives pin LOW → NPN off → R7/R8 pulls PNP base to +12V → V_BE ≈ 0V → PNP off → relay de-energized

---

## Add — Flyback Protection Diodes

Clamp the inductive voltage spike from the relay coil when the PNP transistor switches off. Do not omit.

| Ref | Part | Footprint | Placement |
|---|---|---|---|
| D3 | 1N4001 | DO-41, through-hole | Anode → J1-C (INC_RELAY net); Cathode → +12V_RAW |
| D4 | 1N4001 | DO-41, through-hole | Anode → J1-D (DEC_RELAY net); Cathode → +12V_RAW |

---

## Add — Input Fuse Protection

The production A350 governor includes a fuse on the +12V input. This board should do the same. Add a nano (mini blade) fuse holder in series with the +12V_RAW input line, between the board's power input connector and the rest of the circuit.

| Ref | Part | Footprint | Value |
|---|---|---|---|
| F1 | Nano blade fuse holder | Through-hole, PCB-mount | 1A (fuse rating TBD — see note) |

**Note:** Select fuse rating based on relay coil current plus any other +12V loads on the board. A 1A nano fuse is a suggested starting point; confirm against the relay datasheet and total steady-state current draw before finalizing.

---

## Constraints

- Board outline, mounting holes, and all connector positions (J1, J2, J3, PS1) must remain **identical to Rev 1** — the board fits inside an existing enclosure
- All other components (U1, A1, PS1, OLED, passives) are unchanged
- Through-hole components throughout — no SMD

---

## Deliverables

- Updated KiCad schematic (`.kicad_sch`)
- Updated KiCad PCB layout (`.kicad_pcb`)
- Updated Gerber zip ready for JLCPCB (2-layer, standard settings)
