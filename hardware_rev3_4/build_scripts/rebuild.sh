#!/bin/sh
# Regenerate hardware_rev3_4 from hardware_rev3_3.
#
# Rev 3.4 is Rev 3.3 plus the driver supply switch (Q5/Q6) and the optional
# fault-lamp output (J3/Q7). Like Rev 3.3 it is produced by script, not by
# hand in the KiCad GUI. This is the whole recipe:
#
#   edit_sch34.py    Rev 3.3 schematic -> Rev 3.4 schematic (text-level edit)
#   build_pcb34.py   Rev 3.3 board + Rev 3.4 netlist -> Rev 3.4 board
#   placement34.py   where every moved or new part sits
#   routes34.py      every track (all on F.Cu; B.Cu is an unbroken ground plane)
#   drcsum.py        one-screen summary of the KiCad DRC report
#
# Inputs are the two Rev 3.3 design files, which this script never writes:
#   hardware_rev3_3/hardware/350A_governor.kicad_sch
#   hardware_rev3_3/hardware/350A_governor.kicad_pcb
#
# Needs KiCad 10 (kicad-cli and its bundled python) and any Python 3 on PATH.
# Run from Git Bash:  sh hardware_rev3_4/build_scripts/rebuild.sh
# It rewrites the Rev 3.4 .kicad_sch and .kicad_pcb, refills zones, runs ERC
# and DRC with schematic parity, then regenerates the gerbers, drill file,
# JLCPCB zip, Schematic.pdf, DRC.rpt and the two board renders.
#
# If you later edit the board by hand in KiCad, stop using this script:
# it starts again from Rev 3.3 every time and will overwrite hand edits.
set -e
S="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$S/../.." && pwd)"
K="${KICAD_CLI:-/c/Program Files/KiCad/10.0/bin/kicad-cli.exe}"
KPY="${KICAD_PYTHON:-/c/Program Files/KiCad/10.0/bin/python.exe}"
TMP="${TMPDIR:-/tmp}/rev34_build"
mkdir -p "$TMP"
SRC="$REPO/hardware_rev3_3/hardware"
HW="$REPO/hardware_rev3_4/hardware"

cd "$REPO"
python "$S/edit_sch34.py" hardware_rev3_3/hardware/350A_governor.kicad_sch hardware_rev3_4/hardware/350A_governor.kicad_sch
cd "$HW"
"$K" sch erc --severity-all --format report -o "$TMP/erc34.rpt" 350A_governor.kicad_sch 2>&1 | grep -v "^Saved" || true
"$K" sch export netlist --format kicadxml -o "$TMP/net34.xml" 350A_governor.kicad_sch >/dev/null
cd "$REPO"
"$KPY" "$S/build_pcb34.py" hardware_rev3_3/hardware/350A_governor.kicad_pcb hardware_rev3_4/hardware/350A_governor.kicad_pcb \
    "$TMP/net34.xml" "$(cd "$HW" && pwd -W 2>/dev/null || pwd)/Governor.pretty" 2>&1 | grep -v "memory leak" > "$TMP/build.log" \
    || { tail -20 "$TMP/build.log"; exit 1; }
if grep -i -E "traceback|assert" "$TMP/build.log"; then tail -15 "$TMP/build.log"; exit 1; fi

cd "$HW"
"$K" pcb drc --severity-all --schematic-parity --refill-zones --save-board --format report -o DRC.rpt 350A_governor.kicad_pcb 2>&1 | grep -v "^Saved" || true
python "$S/drcsum.py" DRC.rpt 40

# fabrication and review outputs
rm -rf gerbers && mkdir gerbers
"$K" pcb export gerbers --subtract-soldermask -l "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts" -o gerbers/ 350A_governor.kicad_pcb >/dev/null
"$K" pcb export drill --format excellon --excellon-units mm --drill-origin absolute -o gerbers/ 350A_governor.kicad_pcb >/dev/null
(cd gerbers && python -c "
import glob, zipfile
z = zipfile.ZipFile('350A_governor_JLCPCB.zip', 'w', zipfile.ZIP_DEFLATED)
for f in sorted(glob.glob('350A_governor*')):
    if not f.endswith('.zip'):
        z.write(f)
z.close()
print('JLCPCB zip:', len(zipfile.ZipFile('350A_governor_JLCPCB.zip').namelist()), 'files')")
"$K" sch export pdf -o Schematic.pdf 350A_governor.kicad_sch >/dev/null
"$K" pcb render --side top --quality high -w 1566 -h 1280 -o ../board_top.png 350A_governor.kicad_pcb >/dev/null
"$K" pcb render --side bottom --quality high -w 1566 -h 1280 -o ../board_bottom.png 350A_governor.kicad_pcb >/dev/null
echo "outputs regenerated"
