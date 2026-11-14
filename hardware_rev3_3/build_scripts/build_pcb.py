"""Build the Rev 3.3 board from the Rev 3.2 board + the Rev 3.3 netlist.
Run with KiCad's python:  build_pcb.py SRC.kicad_pcb DST.kicad_pcb net.xml Governor.pretty"""
import os
import sys
import xml.etree.ElementTree as ET
import pcbnew

SRC, DST, NETXML, PRETTY = sys.argv[1:5]
FPROOT = "C:/Program Files/KiCad/10.0/share/kicad/footprints"
mm, fm = pcbnew.ToMM, pcbnew.FromMM


def V(x, y):
    return pcbnew.VECTOR2I(fm(x), fm(y))


# ------------------------------------------------------------------ netlist
root = ET.parse(NETXML).getroot()
comps = {}
for c in root.find('components'):
    comps[c.get('ref')] = dict(value=c.findtext('value'), fp=c.findtext('footprint') or '', ts=c.findtext('tstamps'))
padnet = {}
for n in root.find('nets'):
    for node in n:
        nm = n.get('name')
        if nm.startswith('unconnected-('):
            nm = nm.replace('/', '{slash}')
        padnet[(node.get('ref'), node.get('pin'))] = nm

# ------------------------------------------------------------------ custom J1 footprint
# A 1x8 header outline with position 7 left empty: puts 5.08 mm between the
# magneto pin and its neighbour instead of 2.54 mm.
J1_NAME = 'PinHeader_1x07_P2.54mm_Vertical_Pin7Offset'
if not os.path.isdir(PRETTY):
    os.makedirs(PRETTY)
src_mod = open(FPROOT + '/Connector_PinHeader_2.54mm.pretty/PinHeader_1x08_P2.54mm_Vertical.kicad_mod', encoding='utf-8').read()


def _block(t, start):
    depth, i, in_str = 0, start, False
    while True:
        ch = t[i]
        if in_str:
            if ch == '\\':
                i += 1
            elif ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
        elif ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1


i7 = src_mod.find('(pad "7" ')
assert i7 >= 0
e7 = _block(src_mod, i7)
ls7 = src_mod.rfind('\n', 0, i7) + 1
mod = src_mod[:ls7] + src_mod[e7:].lstrip('\r\n')
assert mod.count('(pad "8" ') == 1
mod = mod.replace('(pad "8" ', '(pad "7" ', 1)
mod = mod.replace('(footprint "PinHeader_1x08_P2.54mm_Vertical"', '(footprint "%s"' % J1_NAME, 1)
assert J1_NAME in mod
open(os.path.join(PRETTY, J1_NAME + '.kicad_mod'), 'w', encoding='utf-8', newline='\n').write(mod)

# ------------------------------------------------------------------ board
# KiCad's python bindings misbehave after BOARD.Remove(), so everything that
# has to go is stripped from the file text first; the API is only used to add.
REPLACE = ('J1', 'J2', 'F1', 'C2', 'Q1', 'Q2', 'R2')
pcb = open(SRC, encoding='utf-8').read()
out, i = [], 0
kept_text = None
while True:
    j = pcb.find('\n\t(', i)
    if j < 0:
        out.append(pcb[i:])
        break
    s = j + 2
    e = _block(pcb, s)
    blk = pcb[s:e]
    head = blk.split(None, 1)[0]
    drop = False
    if head in ('(segment', '(via', '(arc'):
        drop = True
    elif head == '(zone' and '(keepout' in blk:
        drop = True
    elif head == '(footprint' and any('(property "Reference" "%s"' % r in blk for r in REPLACE):
        drop = True
    elif head == '(gr_text':
        x = float(blk.split('(at ', 1)[1].split()[0])
        if x < 90 or x > 178.5:
            drop = True
            if kept_text is None:
                kept_text = blk
    out.append(pcb[i:j])
    if not drop:
        out.append(pcb[j:e])
    i = e
pcb = ''.join(out)


def silk_text(s, x, y):
    import uuid as _uuid
    import re as _re
    blk = _re.sub(r'\(gr_text "[^"]*"', '(gr_text "%s"' % s, kept_text, count=1)
    blk = _re.sub(r'\(at [-\d.]+ [-\d.]+ ', '(at %.2f %.2f ' % (x, y), blk, count=1)
    blk = _re.sub(r'\(uuid "[^"]*"\)', '(uuid "%s")' % _uuid.uuid4(), blk, count=1)
    return '\t' + blk + '\n'


labels = ''
for name, y in (('12V', 85.19), ('GND', 87.73), ('INC', 90.27), ('DEC', 92.81), ('POT', 95.35), ('RTN', 97.89), ('MAG', 102.97)):
    labels += silk_text(name, 85.6, y + 0.75)
for name, y in (('5V', 88.07), ('GND', 90.61), ('SCL', 93.15), ('SDA', 95.69)):
    labels += silk_text(name, 178.92, y + 0.75)
k = pcb.rfind('\n\t(gr_text ')
k = pcb.index('\n', _block(pcb, k + 2) - 1) + 1
pcb = pcb[:k] + labels + pcb[k:]
assert pcb.count('"REV 3.2"') == 1 and pcb.count('"350A DIGITAL GOVERNOR v2.0"') == 1
pcb = pcb.replace('"REV 3.2"', '"REV 3.3"').replace('"350A DIGITAL GOVERNOR v2.0"', '"350A DIGITAL GOVERNOR"')
pcb = pcb.replace('(date "2026-07-25")', '(date "2026-11-14")', 1).replace('(rev "3.2")', '(rev "3.3")', 1)
open(DST, 'w', encoding='utf-8', newline='\n').write(pcb)

b = pcbnew.LoadBoard(DST)
assert len(list(b.GetTracks())) == 0


def load_fp(ref):
    lib, name = comps[ref]['fp'].split(':')
    path = PRETTY if lib == 'Governor' else '%s/%s.pretty' % (FPROOT, lib)
    fp = pcbnew.FootprintLoad(path, name)
    assert fp is not None, comps[ref]['fp']
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(ref)
    b.Add(fp)
    return fp


for ref in sorted(comps):
    fp = b.FindFootprintByReference(ref)
    if fp is None:
        fp = load_fp(ref)
    fp.SetValue(comps[ref]['value'])
    fp.SetPath(pcbnew.KIID_PATH('/' + comps[ref]['ts']))
    fp.SetSheetname('/')
    fp.SetSheetfile('350A_governor.kicad_sch')
    for pad in fp.Pads():
        name = padnet.get((ref, pad.GetNumber()))
        if name is None:
            pad.SetNetCode(0)
            continue
        net = b.FindNet(name)
        if net is None:
            net = pcbnew.NETINFO_ITEM(b, name)
            b.Add(net)
        pad.SetNet(net)


def pad_of(ref, num):
    fp = b.FindFootprintByReference(ref)
    for p in fp.Pads():
        if p.GetNumber() == num:
            return p
    raise KeyError((ref, num))


def place(ref, pad, x, y, rot):
    fp = b.FindFootprintByReference(ref)
    fp.SetOrientationDegrees(rot)
    cur = pad_of(ref, pad).GetPosition()
    fp.Move(pcbnew.VECTOR2I(fm(x) - cur.x, fm(y) - cur.y))


# ------------------------------------------------------------------ placement (absolute mm; pad named is the anchor)
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'placement33.py')).read())
for ref, pad, x, y, rot in P:
    place(ref, pad, x, y, rot)

# ------------------------------------------------------------------ routing
routes = []      # (net, width, layer, [points])


def R(net, width, *pts, layer='F.Cu'):
    routes.append((net, width, layer, pts))


def xy(pt):
    if isinstance(pt, str):
        ref, num = pt.split('.')
        p = pad_of(ref, num).GetPosition()
        return p.x, p.y
    return fm(pt[0]), fm(pt[1])


exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'routes33.py')).read())

LAYER = {'F.Cu': pcbnew.F_Cu, 'B.Cu': pcbnew.B_Cu}
for net, width, layer, pts in routes:
    ni = b.FindNet(net)
    assert ni is not None, net
    for a, c in zip(pts[:-1], pts[1:]):
        (x1, y1), (x2, y2) = xy(a), xy(c)
        if (x1, y1) == (x2, y2):
            continue
        for p in (a, c):
            if isinstance(p, str):
                assert padnet.get(tuple(p.split('.'))) == net, 'route %s touches %s (%s)' % (net, p, padnet.get(tuple(p.split('.'))))
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(pcbnew.VECTOR2I(x1, y1))
        t.SetEnd(pcbnew.VECTOR2I(x2, y2))
        t.SetWidth(fm(width))
        t.SetLayer(LAYER[layer])
        t.SetNet(ni)
        b.Add(t)

# ------------------------------------------------------------------ zones: solid GND on both layers
gz = [z for z in b.Zones() if z.GetNetname() == 'GND'][0]


def setup(z, layer):
    z.SetLayer(layer)
    o = z.Outline()
    o.RemoveAllContours()
    o.NewOutline()
    for x, y in ((84.0, 41.0), (186.0, 41.0), (186.0, 123.0), (84.0, 123.0)):
        o.Append(fm(x), fm(y))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefSpokeWidth(fm(0.8))
    z.SetThermalReliefGap(fm(0.5))
    z.SetLocalClearance(fm(0.5))
    z.SetMinThickness(fm(0.3))
    z.UnFill()


setup(gz, pcbnew.B_Cu)
tz = pcbnew.ZONE(b)
tz.SetNet(gz.GetNet())
tz.SetAssignedPriority(0)
b.Add(tz)
setup(tz, pcbnew.F_Cu)

# ------------------------------------------------------------------ silkscreen (connector labels were rebuilt in the text pass)
# reference designators: resistors and diodes carry theirs inside the body outline
for fp in b.GetFootprints():
    ref = fp.GetReference()
    if ref[0] in 'RD' and ref[1:].isdigit() and ref not in ('D1', 'D2'):
        pads = list(fp.Pads())
        a, c = pads[0].GetPosition(), pads[1].GetPosition()
        r = fp.Reference()
        r.SetPosition(pcbnew.VECTOR2I((a.x + c.x) // 2, (a.y + c.y) // 2))
        r.SetTextHeight(fm(0.9))
        r.SetTextWidth(fm(0.9))
        r.SetTextThickness(fm(0.15))
        r.SetTextAngleDegrees(90 if abs(a.x - c.x) < abs(a.y - c.y) else 0)
        r.SetKeepUpright(True)

# individual silkscreen nudges
for ref, x, y in (('J1', 92.2, 106.0), ('Q1', 98.6, 76.46), ('Q2', 98.6, 91.66)):
    b.FindFootprintByReference(ref).Reference().SetPosition(V(x, y))
for item in b.FindFootprintByReference('D5').GraphicalItems():
    if item.GetClass() == 'PCB_TEXT' and item.GetText() == 'K':
        item.SetPosition(V(101.9, 68.9))
# the LEDs never had a library link; give them one that matches the schematic
for ref in ('D1', 'D2'):
    b.FindFootprintByReference(ref).SetFPID(pcbnew.LIB_ID('LED_THT', 'LED_D3.0mm'))

pcbnew.SaveBoard(DST, b)

print('== pad table')
for ref in sorted(comps, key=lambda r: (r.rstrip('0123456789'), int(''.join(ch for ch in r if ch.isdigit()) or 0))):
    fp = b.FindFootprintByReference(ref)
    if ref in ('A1',):
        continue
    print('%-4s %-9s' % (ref, comps[ref]['value'][:9]), ' '.join('%s:(%.2f,%.2f)%s' % (
        p.GetNumber(), mm(p.GetPosition().x), mm(p.GetPosition().y), p.GetNetname().replace('unconnected-', 'nc-')[:11]) for p in fp.Pads()))
