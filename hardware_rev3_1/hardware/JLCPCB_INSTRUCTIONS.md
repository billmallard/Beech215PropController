# Ordering the PCB — JLCPCB

The board is fully routed. Use the Gerber zip in the `pcbway_production/` folder to order.

## Steps

1. Open [jlcpcb.com](https://jlcpcb.com) and click **Order Now**
2. Upload `pcbway_production/2026-04-27-04-14-28/350A_governor.kicad_pcb_gerber.zip`
3. Confirm the settings below and add to cart

## JLCPCB Settings

| Setting | Value |
|---|---|
| Layers | 2 |
| Dimensions | 100 × 80 mm (auto-detected) |
| PCB Thickness | 1.6 mm |
| Surface Finish | HASL (with lead) |
| Copper Weight | 1 oz |
| Everything else | Default |

You receive 5 boards per order; use 1, keep the rest as spares.

## If You Need to Regenerate Gerbers

Open `350A_governor.kicad_pcb` in KiCad 10, then:

1. **Edit → Fill All Zones**
2. **Inspect → Design Rules Checker** — confirm 0 errors before proceeding
3. **File → Fabrication Outputs → Gerbers**
   - Layers: F.Cu, B.Cu, F.Mask, B.Mask, F.Silkscreen, Edge.Cuts
4. **File → Fabrication Outputs → Drill Files**
5. Zip the output folder and upload
