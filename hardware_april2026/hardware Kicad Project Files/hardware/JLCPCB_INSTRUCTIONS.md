# Generating Gerbers for Fabrication

The `.kicad_pcb` file contains correct component placement with full netlist.
You must route the board in KiCad before generating Gerbers.

## Steps
1. Open `350A_governor.kicad_pcb` in KiCad 7+
2. Follow `ROUTING_GUIDE.md` to route all connections (~45 min)
3. Edit → Fill All Zones
4. Inspect → Design Rules Checker (target: 0 errors)
5. File → Fabrication Outputs → Gerbers
   - Output directory: `gerbers/`
   - Layers: F.Cu, B.Cu, F.Mask, B.Mask, F.Silkscreen, Edge.Cuts
   - Also: File → Fabrication Outputs → Drill Files
6. Zip the `gerbers/` folder and upload to JLCPCB

## JLCPCB Settings
- Layers: 2
- Dimensions: 100×80 mm
- PCB thickness: 1.6 mm
- Surface finish: HASL (lead-free)
- Copper weight: 1 oz
- All other settings: default
