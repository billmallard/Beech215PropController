# 350A Governor PCB — Routing Guide
## Open the board in KiCad 7+ and route these connections

Board: 100×80 mm, 2-layer (F.Cu signal + B.Cu GND pour)

### Before Routing
1. Open `350A_governor.kicad_pcb` in KiCad PCB Editor
2. **File → Board Setup → Constraints**: set Copper clearance 0.2mm, Track min 0.25mm
3. **Edit → Fill All Zones** (creates B.Cu GND pour)
4. Press **N** to show ratsnest (all unrouted connections)

---

### Design Rule Summary
| Net type | Width | Layer |
|---|---|---|
| Power (12V_RAW, VCC5) | 0.8 mm | B.Cu preferred |
| Relay outputs (INC/DEC) | 0.5 mm | B.Cu preferred |
| Signal | 0.25 mm | F.Cu preferred |
| GND stitching vias | 0.8mm drill, 1.6mm ann | F.Cu–B.Cu |

---

### Route in This Order

#### 1. Power rails (B.Cu — back copper, unobstructed)

**12V_RAW** (0.8mm track):
- J1 pin1 (5, 43) → B.Cu horizontal at y=4 → PS1 pin1 (88, 8) via at (88, 4)
- Also tap at (12, 4) → via → C2 pin1 (12, 28) using F.Cu stub

**VCC5** (0.6mm track):
- PS1 pin3 (88, 13.08) → F.Cu stub up → via at (88, 6) → B.Cu at y=6 left to x=14
- Tap off B.Cu y=6 bus with vias at:
  - (22, 6) → F.Cu up to R2 pin1 (24, 10) [4.7k pull-up]
  - (52, 6) → F.Cu up to C4 pin1 (52, 22), and down to Nano pad27 (50.24, 35.62) and pad30 (50.24, 28)
  - (80, 6) → F.Cu to C3 pin1 (80, 10)
  - (12, 6) via → F.Cu right to U1 pin5 (19.62, 19.62) and pin8 (19.62, 12)

**GND**: Handled by B.Cu pour. Add F.Cu–B.Cu stitching vias at:
- (12, 35), (28, 68), (60, 68), (76, 68), (92, 68)

---

#### 2. MAG signal path (F.Cu, 0.25mm)

- J1 pin7 (5, 58.24) → F.Cu right → R1 pin1 (10, 58.24)
- R1 pin2 (17.62, 58.24) → B.Cu via at (17.62, 58.24) → B.Cu at x=17.62 going UP to y=14.54 → via at (17.62, 14.54) → F.Cu right to U1 pin2 (12, 14.54)
  - *(x=17.62 is safely right of U1 right col at x=19.62; no — wait: U1 right col is at x=12+7.62=19.62, left col at x=12)*
  - *Route at x=11 instead: (17.62,58.24)→(11,58.24)→(11,14.54)→(12,14.54) all on B.Cu*
- C1 pin1 (12, 22) already on MAG_FILT — short F.Cu stub to U1 pin2 region

---

#### 3. RPM pulse: U1 output → Nano D2

- U1 pin6 (19.62, 17.08) → F.Cu right → R2 pin2 (31.62, 10)
  - Route: (19.62, 17.08) → (19.62, 10) → (31.62, 10)
- R2 pin2 (31.62, 10) → B.Cu via at (31.62, 10) → B.Cu down at x=31.62 to y=38.16 → via at (31.62, 38.16) → F.Cu right to Nano pad5 (35, 38.16)
  - *(x=31.62 is 3.38mm left of Nano left col at x=35 — safe)*

---

#### 4. INC/DEC relay signals: Nano → ULN2003A

Nano pad9 (D6_INC) at (35, 48.32):
- F.Cu right → via at (58, 48.32) → B.Cu right to U2 pin1 (78, 46)
  - Route: (35,48.32)→(58,48.32) F.Cu, via, (58,48.32)→(78,46) B.Cu *(slight angle or L-route)*

Nano pad10 (D7_DEC) at (35, 50.86):
- F.Cu right → via at (58, 50.86) → B.Cu right to U2 pin2 (78, 48.54)

ULN2003A outputs → J1 relay pins:
- U2 pin15 (INC_RELAY) at (85.62, 48.54) → B.Cu at y=48.54 left to (5, 48.08) → via → J1 pin3
- U2 pin14 (DEC_RELAY) at (85.62, 51.08) → B.Cu at y=51.08 left to (5, 50.62) → via → J1 pin4
  - *(route relay lines on B.Cu near y=48–51, below components)*

12V_COM to ULN2003A:
- U2 pin16 (85.62, 46) and pin9 (85.62, 63.78): connect via F.Cu stub at x=85.62
- Tap 12V_RAW bus: via at (85.62, 4) → F.Cu down to (85.62, 46) *(or stub right from 12V bus)*

---

#### 5. LED signals: Nano → R3/R4 → D1/D2

Nano pad11 (D8_INC_LED) at (35, 53.4) → F.Cu straight right to R3 pin1 (58, 53.4)
Nano pad12 (D9_DEC_LED) at (35, 55.94) → F.Cu straight right to R4 pin1 (58, 55.94)
R3 pin2 (65.62, 53.4) → F.Cu right to D1 pin2/anode (70, 53.4)  *(LEDs rotated 180°)*
R4 pin2 (65.62, 55.94) → F.Cu right to D2 pin2/anode (70, 55.94)
D1 pin1/GND (72.54, 53.4) and D2 pin1/GND (72.54, 55.94): short F.Cu stubs to GND pour via

---

#### 6. I²C: Nano SDA/SCL → J2

Nano pad23 (SDA) at (50.24, 45.78):
- F.Cu right → (56, 45.78) → B.Cu at y=45.78 right to (92, 51.08) → via → J2 pin3 (92, 51.08)

Nano pad24 (SCL) at (50.24, 43.24):
- F.Cu right → (56, 43.24) → B.Cu at y=43.24 right → via at (92, 53.62) → J2 pin4
  - *(Use F.Cu stubs at J2 end if B.Cu pour conflicts)*

---

#### 7. Switch signals: Nano D3-D5, D10-D11 → J2

All go F.Cu RIGHT from Nano left col → via → B.Cu → right to J2.
Use unique Y-coordinates for B.Cu runs (no crossing):

| Nano pad | Net | Nano pos | B.Cu y | J2 pin |
|---|---|---|---|---|
| pad6 | D3_SW1 | (35, 40.7) | y=40.7 | 5 (92, 56.16) |
| pad7 | D4_SW2 | (35, 43.24) | y=43.24 | 6 (92, 58.7) |
| pad8 | D5_SW3 | (35, 45.78) | y=45.78 | 7 (92, 61.24) |
| pad13 | D10_SW4 | (35, 58.48) | y=58.48 | 8 (92, 63.78) |
| pad14 | D11_SW5 | (35, 61.02) | y=61.02 | 9 (92, 66.32) |

Route each as: F.Cu from Nano pad right to x=56 → via → B.Cu horizontal at same y → via at (92, J2_pin_y) → short F.Cu stub to J2 pin.
*(All 5 B.Cu tracks run parallel at different y values, y=40.7 to y=61.02 — no crossings)*

---

#### 8. J2 power/GND

J2 pin1 (VCC5) at (92, 46): short F.Cu stub right from VCC5 bus via at (92, 6) → F.Cu stub down
J2 pin2, pin10 (GND): connect to GND pour via F.Cu stubs + stitching via

---

### Final Steps
1. **Edit → Fill All Zones**
2. **Inspect → Board Statistics** — check ratsnest shows 0 unrouted
3. **Inspect → Design Rules Checker** — should show 0 errors
4. **File → Fabrication Outputs → Gerbers** — output to `gerbers/` subfolder
5. Upload `gerbers/` folder to JLCPCB or PCBWay as zip

