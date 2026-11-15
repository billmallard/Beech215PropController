# Troubleshooting Guide

## Status and Fault Messages (firmware v3.0)

In the aircraft there is no display. These messages appear on the USB serial console (9600 baud, type `s`)
and on the optional bench display.

| Message | Type | Meaning | Action |
|---|---|---|---|
| `NO MAG SIGNAL` | Standby — clears itself | No valid magneto pulses for more than 2 seconds, or the engine is not running. Both relays are off. Governing resumes when pulses return | Normal with the engine stopped. With the engine running: check the Pin H lead and the resistor-lug assembly at the right magneto (original Service Note 8) |
| `POT OPEN` | Standby — clears itself | The panel control reads above about 16kΩ (open wire or worn control). Both relays are off | Check the control between connector pins E and F: under 100Ω full clockwise, 10kΩ full counter-clockwise, smooth in between |
| `RPM LOW` | Standby — clears itself | Measured RPM below 400. Both relays are off. Governing resumes at once when the reading comes back up | Normal while the engine is being started or stopped. With the engine running normally: suspect the magneto signal or the wrong `PULSES_PER_REV` |

The INC (red) and DEC (green) LEDs on the board follow the relay outputs.

Nothing latches: every condition above clears by itself, and there is nothing to reset. There is no
upper RPM limit: a high reading is answered with a decrease command, as on the original.

There is no time limit on a relay command. As on the original, an output stays on until the RPM reaches
the knob setting, including while the prop rests on a pitch stop with its limit switch open.

---

## Symptom-Based Troubleshooting

### Problem: optional bench display is blank at power-on

1. Verify 5V is present at LM2596 output — check with multimeter at PS1 pins 3 and 4
2. Check LM2596 is plugged into PS1 header correctly (not reversed)
3. Verify the display module pin order matches J2: 5V, GND, SCL, SDA. The display is only detected at power-up
4. Try alternate I2C address: change `#define OLED_I2C_ADDR 0x3C` to `0x3D` in firmware and reflash
5. Connect Nano via USB to a computer and open Serial Monitor — should show boot messages at 9600 baud

### Problem: no RPM reading (status stays `NO MAG SIGNAL` with the engine running)

1. Verify magneto signal is connected at J1 Pin H
2. Check R1 and R9 (5.1kΩ each) are installed in series between Pin H and U1 pin 2, and D6 (1N4148) is the right way round
3. Check U1 (6N137) is seated correctly in its socket, pin 1 matching the silkscreen notch. U1 pin 7 and pin 8 should both read 5V
4. Check R2 (4.7kΩ pull-up) installed between 5V and U1 output
5. With engine running: put scope or multimeter on U1 pin 6 — should see pulses at ~90–133 Hz range
6. Try left magneto: disconnect Pin H from right magneto P-lead, temporarily connect to left magneto P-lead (requires re-routing or a switch)

### Problem: RPM display reads wrong value (e.g., reads half of actual)

- `PULSES_PER_REV` is set incorrectly — verify engine is 6-cylinder Continental and firmware has `#define PULSES_PER_REV 3`
- If reads double: set to `PULSES_PER_REV 6` (unusual, but some magneto configurations fire twice per revolution)
- Cross-check against known panel tachometer; if tach is suspect, use the fluorescent lamp calibration method from original 350A Service Note 5

### Problem: AUTO mode does nothing (no relay activity)

1. Verify AUTO switch is in correct position and 12V present at J1 Pin A
2. Check Nano D6 and D7 with multimeter — should show 5V when error exceeds deadband
3. Check transistors Q1–Q4: verify flat-body orientation matches PCB silkscreen; inspect solder joints on all three leads of each device
4. Check J1 pins C and D with multimeter — should show 12V when governor is commanding correction
5. If C and D have 12V but motor doesn't run: airframe relay issue, not the governor

### Problem: RPM hunts / oscillates in flight

This is the problem this controller was designed to fix. If hunting persists after installation:

1. **Increase deadband first:** Change `#define DEADBAND_RPM 25` to `35`, then `50`. Reflash. Try in flight. Most hunting is cured at 35–40 RPM deadband with a fresh motor.
2. **Check for magneto noise:** If both LEDs flash erratically, the RPM signal is noisy. Switch Pin H to left magneto and compare. If better → right magneto needs service.
3. **Verify gear timing:** With engine off, time manual prop sweep. Should be 16–32 seconds. If 6–12 seconds → wrong gears in motor (see original 350A Service Note 3). Do not fly until corrected.
4. **Check brush spring:** Correct brush is Beech PN 31-408, spring wire diameter 0.027". Wrong brushes cause oscillation in both old and new governors.
5. **Increase proportional zone:** Change `#define PROP_ZONE_RPM 75` to `100` or `150` for more gradual approach to setpoint.

### Problem: Prop goes to coarse when it should go fine (wrong direction)

- INC and DEC relay outputs are swapped
- Two ways to fix:
  1. Swap J1 pins C and D in the aircraft connector
  2. Edit firmware: swap `D6_INC_RELAY` and `D7_DEC_RELAY` pin definitions and reflash

### Problem: RPM does not follow the panel knob

1. Type `s` on the serial console. If it reports `POT_OPEN`, the control or its wiring is open
2. With the connector unplugged from the governor, measure between pins E and F while turning the knob (see the table above)
3. On the board, the POT pin of J1 should read 5V with nothing connected and fall toward 0V as the knob goes clockwise
4. Remember the setpoint ramps at 100 RPM per second: a full sweep of the knob takes several seconds to arrive

### Problem: governed RPM disagrees with the panel tachometer

- Check panel tachometer calibration — see original 350A Service Note 5 (fluorescent lamp test)
- Check `PULSES_PER_REV` setting
- If ACT reads consistently 10–20 RPM high or low, add a `#define RPM_OFFSET 10` calibration constant to firmware

---

## Diagnostic Jumper Tests

These are equivalent to the original 350A's airframe systems test (Service Note 1). Use safety wire formed into a "U" shape.

**Test INC relay drive:**
1. Master ON, prop switch AUTO, engine OFF
2. Connect J1 Pin A to J1 Pin C with jumper wire
3. Motor should run toward fine pitch (high RPM direction)
4. Motor should stop when full fine limit switch opens
5. Remove jumper

**Test DEC relay drive:**
1. Master ON, prop switch AUTO, engine OFF
2. Connect J1 Pin A to J1 Pin D with jumper wire
3. Motor should run toward coarse pitch (low RPM direction)
4. Motor should stop when full coarse limit switch opens
5. Remove jumper

If motor fails these tests: limit switch wiring, relay, or motor issue — not the governor.

If motor runs but doesn't stop at limit: limit switches not in circuit, or wired backwards. See aircraft wiring diagrams.

---

## Checking the Magneto Signal Quality

Poor magneto signal quality (weak impulse springs, worn rubber couplings, deteriorated P-lead filter) causes the RPM reading to jitter, which causes the governor to overcorrect.

**Quick check:** Watch the actual RPM on the serial console (or the bench display) during a ground run, or the panel tachometer in flight. At a steady power setting:
- **Good signal:** ACT reading holds within ±15 RPM
- **Marginal signal:** ACT reading bounces ±25–40 RPM despite stable flight
- **Bad signal:** ACT reading swings ±50+ RPM or shows erratic jumps

**Test:** Switch J1 Pin H from right magneto to left magneto and compare. If left is steadier, the right magneto needs attention.

**Firmware response to noisy signal:** The 8-sample rolling average in the RPM calculation rejects single-pulse outliers. If your magneto is very noisy, increasing the average to 16 samples (`#define RPM_AVG_SAMPLES 8` → `16`) will smooth the reading at the cost of slightly slower response to genuine RPM changes.

---

## Firmware Defaults (v3.0)

Firmware v3.0 stores nothing in EEPROM. These are compile-time constants at the top of the sketch:

| Parameter | Default |
|---|---|
| PULSES_PER_REV | 3 |
| POT_RPM_MAX (knob full clockwise) | 2650 RPM — placeholder, set for your engine |
| POT_RPM_MIN (knob full counter-clockwise) | 1800 RPM — placeholder, set for your engine |
| Setpoint ramp | 100 RPM/sec |
| Deadband | 25 RPM |
| Proportional zone | 75 RPM |
| Mode at power-on | AUTO (standby until engine pulses are seen) |

---

## Getting Help

1. Check this document first
2. Review the original 350A documentation in `docs/original_350A/` — many airframe issues are covered there and still apply
3. Open a GitHub issue with:
   - Aircraft serial number range (D-XXXX)
   - Engine model
   - Firmware version (first line printed on the serial console at power-up)
   - Description of symptom
   - What you've already tried
4. American Bonanza Society forums — experienced members may have seen the same issue
