# 350A Governor PCB — Rev 3 Routing Specification
# High-Side Relay Driver (NPN + PNP)

This document gives **explicit pad-to-net assignments** for all new Rev 2 components.
Use it to assign nets in the PCB editor before routing traces.

---

## Footprint Pin Reference

### Q3, Q4 — 2N2222A NPN (footprint: `2N2222A:TO92254P470H750-3`)

Three inline pads. Square pad (pad 1) is marked with a silkscreen dot.
Looking at the **flat face** of the transistor body:

```
[Pad 1 / E]  [Pad 2 / B]  [Pad 3 / C]
  Emitter      Base        Collector
  (square)     (round)     (round)
```

### Q1, Q2 — 2N2907A PNP (footprint: `2N2907A_:TO18`)

> **Note:** The revision brief specified TO-92. The delivered footprint is TO-18 (metal can,
> triangular pin arrangement). Electrically compatible but physically different. If you wish
> to change to TO-92 for consistency with the brief, use the standard KiCad
> `Package_TO_SOT_THT:TO-92_Inline` footprint (EBC left-to-right). The pad assignments
> below are for the TO-18 footprint as placed.

Pin layout (view from bottom, leads facing you):

```
Pad 1 (E)   Pad 2 (B)
  (-1.27,    (+1.27,
   +1.27)     +1.27)

             Pad 3 (C)
             (+1.27,
              -1.27)
```

| Pad | Function |
|-----|----------|
| 1   | Emitter  |
| 2   | Base     |
| 3   | Collector|

---

## Required Net Assignment — Per Pad

### Channel 1 — INC (Q3, Q1, R5, R7, D4)

| Ref | Pad | Net | Notes |
|-----|-----|-----|-------|
| Q3 | 1 (E) | `GND` | NPN emitter to ground |
| Q3 | 2 (B) | `Q3_BASE` | Receives drive signal via R5 |
| Q3 | 3 (C) | `Q3_COLL` | Shared node: R7 pin 1 + Q1 pad 2 |
| R5 | 1 | `Q3_BASE` | Connects to Q3 base |
| R5 | 2 | `D6_INC` | ✅ Already routed — do not change |
| R7 | 1 | `Q3_COLL` | Shared node with Q3 collector and Q1 base |
| R7 | 2 | `12V_RAW` | Pull-up to +12V — keeps PNP off when NPN is off |
| Q1 | 1 (E) | `12V_RAW` | PNP emitter to +12V supply rail |
| Q1 | 2 (B) | `Q3_COLL` | Shared node: Q3 collector + R7 pin 1 |
| Q1 | 3 (C) | `INC_RELAY` | Drives INC relay coil via J1 |
| D4 | 1 (cathode) | `12V_RAW` | ⚠️ Currently unrouted — must connect to +12V |
| D4 | 2 (anode)   | `INC_RELAY` | ✅ Already routed — do not change |

**`Q3_BASE` and `Q3_COLL` are internal nets** — name them whatever you like in KiCad,
but they must connect *only* the pads listed above.

---

### Channel 2 — DEC (Q4, Q2, R6, R8, D3)

| Ref | Pad | Net | Notes |
|-----|-----|-----|-------|
| Q4 | 1 (E) | `GND` | NPN emitter to ground |
| Q4 | 2 (B) | `Q4_BASE` | Receives drive signal via R6 |
| Q4 | 3 (C) | `Q4_COLL` | Shared node: R8 pin 1 + Q2 pad 2 |
| R6 | 1 | `Q4_BASE` | Connects to Q4 base |
| R6 | 2 | `D7_DEC` | ✅ Already routed — do not change |
| R8 | 1 | `Q4_COLL` | Shared node with Q4 collector and Q2 base |
| R8 | 2 | `12V_RAW` | Pull-up to +12V |
| Q2 | 1 (E) | `12V_RAW` | PNP emitter to +12V supply rail |
| Q2 | 2 (B) | `Q4_COLL` | Shared node: Q4 collector + R8 pin 1 |
| Q2 | 3 (C) | `DEC_RELAY` | Drives DEC relay coil via J1 |
| D3 | 1 (cathode) | `12V_RAW` | ✅ Already routed — do not change |
| D3 | 2 (anode)   | `DEC_RELAY` | ✅ Already routed — do not change |

---

### Fuse — F1

> **Important:** F1 must be in *series* on the +12V input, not connected pad-to-pad on
> the same net. This requires splitting the existing `12V_RAW` net at the power input.

| Ref | Pad | Net | Notes |
|-----|-----|-----|-------|
| F1 | 1 | `12V_IN` | From power input connector (upstream of fuse) |
| F1 | 2 | `12V_RAW` | Protected rail (downstream — feeds Q1/Q2 emitters, R7/R8) |

**How to implement in KiCad:**
1. Identify the pad on J1 (or PS1) that is the +12V input to the board.
2. Break the existing `12V_RAW` trace between that pad and the rest of the board.
3. Rename the power-input pad's net to `12V_IN`.
4. Route: connector `12V_IN` pad → F1 pad 1 → F1 pad 2 → existing `12V_RAW` bus.

---

## Errors to Correct in Current Revision

The following incorrect connections from the current PCB must be **removed** before re-routing:

| Ref | Pad | Current (wrong) net | Correct net |
|-----|-----|---------------------|-------------|
| Q3  | 2   | `GND`               | `Q3_BASE`   |
| Q4  | 2   | `GND`               | `Q4_BASE`   |
| Q1  | 1   | `DEC_RELAY`         | `12V_RAW`   |
| Q2  | 1   | `INC_RELAY`         | `12V_RAW`   |
| F1  | 1   | `12V_RAW`           | `12V_IN`    |
| F1  | 2   | `12V_RAW`           | `12V_RAW` (unchanged but pad 1 must change) |

---

## What Was Correct (do not change)

| Ref | Pad | Net | Status |
|-----|-----|-----|--------|
| R5 | 2 | `D6_INC` | ✅ |
| R6 | 2 | `D7_DEC` | ✅ |
| D3 | 1 | `12V_RAW` | ✅ |
| D3 | 2 | `DEC_RELAY` | ✅ |
| D4 | 2 | `INC_RELAY` | ✅ |

---

## Re-run DRC After Routing

The existing DRC report (`DRC.rpt`) is dated **2026-03-12** and does not reflect the
Rev 2 components. After completing all net assignments and routing:

1. **Edit → Fill All Zones** (refresh GND pour)
2. **Inspect → Design Rules Checker** — must show 0 errors, 0 unconnected pads
3. Save a new `DRC.rpt`
4. Regenerate Gerbers: **File → Fabrication Outputs → Gerbers**
