# Mandatory Rework — Flyback Diodes D3/D4 (Rev 3 and Rev 3.1 Boards)

> **Superseded 2026-11-14.** A design review of Rev 3.2 found further faults
> shared by every Rev 3, 3.1 and 3.2 board: no current limit on the PNP bases,
> the optocoupler enable pin tied to ground, and a fuse footprint for the wrong
> part. This rework fixes the flyback clamp only. **It does not make a
> pre-3.3 board fit to fly.** See
> [hardware_rev3_3/REV3_3_CHANGES.md](../hardware_rev3_3/REV3_3_CHANGES.md).

**Applies to:** every board fabricated before Rev 3.2 — the April 2026 PCBWay
run (Rev 3) and any board built from the Rev 3.1 JLCPCB package. Rev 3.2
boards are marked **"REV 3.2"** on the front silkscreen at bottom center; if
your board has that marking, this rework does not apply.

**Status:** required before flight. Strongly recommended before further bench
running with real relays attached.

---

## Why

On Rev 3/3.1 boards, D3 and D4 are connected from each relay output to the
+12V rail — across the 2N2907A drive transistor, not across the relay coil.

The drive is a high-side PNP sourcing +12V into a relay coil whose other end
is grounded in the airframe. When the transistor turns off, the coil pulls
the output node **negative**, and a diode pointing at the +12V rail is
reverse-biased for that entire transient — it never conducts. As built,
there is **no functional flyback clamping at all**. The coil's energy
instead drives the output node ~40V or more below ground until the diode or
the 2N2907A breaks down. Every relay turn-off (up to ~10 per second in the
proportional control zone):

- avalanches semiconductors that are not rated for repetitive avalanche,
- radiates a large negative spike down the aircraft harness on the INC/DEC
  lines, in the same 7-pin connector as the magneto P-lead sense line, and
- will eventually fail D3/D4 or Q1/Q2.

The fix places each diode across the relay coil circuit: **cathode to the
relay output, anode to ground.** The coil then freewheels through the diode
at turn-off and the spike disappears.

## Parts

- 2 × 1N4001 (or any 1N400x) — do not reuse the removed diodes; they have
  been avalanched an unknown number of times
- ~2 inches of 22 AWG insulated wire (if a ground point is not adjacent)
- Solder, desoldering braid/pump, flush cutters, multimeter

## Procedure (board on the bench, powered off, out of the aircraft)

1. **Remove D3 and D4** completely. Clean both pads of each footprint.

2. **Identify the relay-output pad of each footprint** with a continuity
   meter against the 7-pin interface header J1:
   - the D4 pad that beeps to **J1 pin 3 (INC_RELAY)** is the INC output pad
   - the D3 pad that beeps to **J1 pin 4 (DEC_RELAY)** is the DEC output pad

   Do not go by silkscreen or by which pad "looks right" — the Rev 3 and
   Rev 3.1 layouts differ, and the old silkscreen cathode marks point the
   wrong way by design.

3. **Confirm the other pad of each footprint is +12V** (beeps to J1 pin 1 /
   the fuse). ⚠️ **This vacated pad stays connected to the 12V rail even
   with the diode removed** — on Rev 3.1 the rail actually routes *through*
   the D3 pad. Leave it empty. Nothing may touch it.

4. **Install each new 1N4001:**
   - **Cathode (banded end) into the relay-output pad** identified in step 2.
   - **Anode lead to ground** — do not put it in the old 12V pad. Bend the
     lead over (insulate with heat-shrink if it passes near other copper)
     and solder it to a convenient ground: J1 pin 2 or pin 5, the ground leg
     of C2, or a PS1 GND pin. Keep the loop short.

5. **Verify with the meter (diode mode):**
   - Red probe on ground, black probe on J1 pin 3 → ~0.5–0.7V (D4 forward).
     Same for J1 pin 4 (D3). Swapped probes → open.
   - Resistance from the vacated +12V pads to the new diode bodies → open.
   - +12V rail to ground → no short (>1kΩ; the LM2596 input is there).

6. **Re-run the bench test** ([BENCH_TEST.md](BENCH_TEST.md)) with real
   relays before reinstalling in the aircraft. If you have a scope, watch
   J1 pin 3/4 during relay drop-out: the negative spike should now clamp at
   about −0.7V.

## Sign-off

This rework changes the governor's protection circuit. Log it against the
unit, and if the governor was already installed under your Form 337, have
your IA note the rework at the next inspection.
