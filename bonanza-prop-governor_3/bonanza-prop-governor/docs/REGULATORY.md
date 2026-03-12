# Regulatory Guidance — FAA Owner-Produced Parts

## Important Disclaimer

This document provides general guidance based on publicly available FAA regulations and policy. It is not legal advice. Aviation regulations are complex and subject to interpretation. **Consult with your IA and FSDO before proceeding.** The regulatory path described here is the most direct and well-supported approach, but your specific situation may vary.

---

## The Regulatory Framework

### What Is an Owner-Produced Part?

14 CFR Part 21.303(b)(2) allows an aircraft owner to produce a replacement part for their own aircraft without holding a Parts Manufacturer Approval (PMA), provided:

1. The part is produced by the aircraft owner for installation on their own aircraft
2. The part is produced using the original manufacturer's design data (drawings, specs) or acceptable equivalent data
3. The part is airworthy when installed

FAA Order 8110.42C provides additional guidance. The key language: *"An owner-produced part may be used to maintain an aircraft without PMA if produced by the owner or operator for installation on aircraft they own or operate."*

### How This Project Fits

The Airborne Electronics 350A is:
- Out of production (company closed 2010)
- Covered by STC SA2700WE
- A "line replaceable unit" — the entire governor box is replaced as a unit
- Fully documented in the original manufacturer's documentation (which is preserved in this repository)

The new controller:
- Maintains identical external interface (7-pin Cannon connector, identical pin assignments, identical signal behaviors on all 7 pins)
- Uses the same airframe wiring, relays, prop motor, and limit switches
- Performs the same defined function: maintain selected RPM by commanding prop pitch
- Is produced by the aircraft owner for installation on their own aircraft

The original manufacturer explicitly designed this unit to be owner-serviced and documented the internal circuit at component level. The service notes state: *"Do not worry about warranty. It will be honored even though the cover has been removed."*

---

## The Path to Legal Installation

### Step 1: Find an IA

You need an Airframe & Powerplant mechanic holding an Inspection Authorization. Not every IA is comfortable with avionics or with unusual 337 work. Look for:
- IAs who have worked with Bonanzas specifically
- IAs with avionics background (A&P/IA who also holds a Repairman certificate)
- Contact the American Bonanza Society (ABS) — their maintenance clinics and member network are the best resource for finding Bonanza-experienced IAs

### Step 2: FAA Form 337

A Form 337 (Major Repair and Alteration) is required because:
- Replacing an STC'd avionics component with a non-PMA part is a major alteration
- Automatic propeller control systems are in the major alteration category

The 337 needs to document:
- The aircraft (make, model, serial number, registration)
- The work performed (description of the alteration)
- The data used to show airworthiness
- The IA's certification that the work was done in accordance with that data

### Step 3: The Data Package

The "data used to show airworthiness" is the critical part. Your data package should include:

1. **Original 350A documentation** — the complete installation instructions and service notes (in this repository under `docs/original_350A/`). This is the baseline definition of the system.

2. **This project's engineering documentation** — specifically the spec document showing:
   - Identical external interface
   - Added safety features (watchdog, mag timeout, relay timeout)
   - Identical output behaviors (relay coil drive current, signal levels)

3. **Pin-for-pin comparison table** — see [WIRING.md](WIRING.md), the "Pinout Cross-Reference" section. Print this for your 337 package.

4. **Ground test results** — completed bench test checklist and ground test results

5. **Your IA's assessment** — that the installed system performs its intended function

### Suggested 337 Description Language

Your IA will write the actual 337, but this is suggested language for the "Description of Work" block:

> *"Replaced Airborne Electronics Model 350A Solid-State Propeller Governor (per STC SA2700WE) with owner-produced equivalent unit. Replacement unit uses identical 7-pin cannon connector interface, identical pin assignments (Pins A through H), identical relay coil drive outputs on Pins C and D, identical magneto P-lead input configuration on Pin H (10kΩ isolating resistor preserved), and identical panel potentiometer interface on Pins E and F. All airframe wiring, propeller control relays, propeller pitch change motor, and limit switches are unchanged. Replacement controller provides proportional RPM control with hardware watchdog protection, magneto signal loss detection, and motor run-time limiting. System verified per functional test procedure equivalent to Airborne Electronics 350A Installation Instructions Ground Test and Flight Test procedures."*

---

## Frequently Asked Questions

**Does this void the STC?**

The STC covers the installation of the electric prop control system on the aircraft. Replacing the electronic governor box with an equivalent unit is analogous to replacing any other component with an owner-produced part. The STC doesn't cease to exist — the aircraft is still modified per STC SA2700WE. The specific governor unit has been replaced under owner-produced parts authority.

**Can I sell this to other Bonanza owners?**

As a private builder, no — owner-produced parts authority is specific to your aircraft. If you want to supply this to other owners, you would need either a PMA (expensive, lengthy) or to work with each owner/IA team on a per-aircraft 337. The practical path for the community is for each owner to order their own PCBs (5 boards for $2 at JLCPCB), build their own unit, and work with their own IA. This repository gives them everything they need to do exactly that.

**What if my FSDO doesn't agree?**

FSDO offices vary in their familiarity with owner-produced parts in the avionics context. Some general-aviation FSDOs rarely see this type of work. If you encounter resistance:
- Reference FAA Order 8110.42C directly
- Reference AC 43.13-1B (Acceptable Methods for Aircraft Inspection and Repair)
- Contact the American Bonanza Society — they have experience navigating Bonanza-specific regulatory questions
- Consider requesting a legal interpretation from your FSDO in writing before proceeding

**My IA is nervous about this. What should I tell them?**

The key points for your IA:
1. The external interface is byte-for-byte identical — no airframe changes at all
2. The original manufacturer is out of business and the part is genuinely unavailable new
3. The original manufacturer documented the design at component level and explicitly expected owners to service it
4. The added safety features (watchdog, mag timeout) make the replacement *safer* than the original, not less safe
5. The bench test procedure is comprehensive and documents that the unit performs its intended function before installation

---

## Additional References

- **14 CFR Part 21.303** — Production Approval and Related Requirements (PMA)
- **14 CFR Part 21.303(b)(2)** — Owner-produced parts provision
- **FAA Order 8110.42C** — Parts Manufacturer Approval (PMA) Procedures
- **AC 43.13-1B** — Acceptable Methods, Techniques, and Practices — Aircraft Inspection and Repair
- **FAA Form 337** — Major Repair and Alteration form
- **STC SA2700WE** — Airborne Electronics Model 350A original STC
- **American Bonanza Society** — bonanza.org — technical resources and IA referrals

---

*This document was last reviewed for regulatory accuracy in early 2026. FAA regulations and interpretive guidance change over time. Always verify current regulatory requirements before proceeding.*
