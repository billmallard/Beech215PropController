"""Build the Rev 3.4 board from the Rev 3.3 board + the Rev 3.4 netlist.
Run with KiCad's python:  build_pcb34.py SRC.kicad_pcb DST.kicad_pcb net.xml Governor.pretty

The Rev 3.3 board supplies the outline, the mounting holes, the two ground
zones, the silkscreen legends and every footprint that already existed. This
script strips all tracks, adds the footprints that are new in Rev 3.4, puts
every pad on the net the Rev 3.4 schematic says, applies placement34.py and
lays every track in routes34.py."""
import os
import re
import sys
import uuid
import xml.etree.ElementTree as ET
import pcbnew

SRC, DST, NETXML, PRETTY = sys.argv[1:5]
FPROOT = "C:/Program Files/KiCad/10.0/share/kicad/footprints"
HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(os.path.abspath(DST))
mm, fm = pcbnew.ToMM, pcbnew.FromMM


def V(x, y):
    return pcbnew.VECTOR2I(fm(x), fm(y))


# ------------------------------------------------------------------ netlist
root = ET.parse(NETXML).getroot()
comps = {}
for c in root.find('components'):
    props = set(p.get('name') for p in c.findall('property'))
    comps[c.get('ref')] = dict(value=c.findtext('value'), fp=c.findtext('footprint') or '', ts=c.findtext('tstamps'),
                               dnp='dnp' in props)
padnet = {}
for n in root.find('nets'):
    for node in n:
        nm = n.get('name')
        if nm.startswith('unconnected-('):
            nm = nm.replace('/', '{slash}')
        padnet[(node.get('ref'), node.get('pin'))] = nm


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


# ------------------------------------------------------------------ board
# KiCad's python bindings misbehave after BOARD.Remove(), so everything that
# has to go is stripped from the file text first; the API is only used to add.
pcb = open(SRC, encoding='utf-8').read()
out, i = [], 0
label_tpl = None
while True:
    j = pcb.find('\n\t(', i)
    if j < 0:
        out.append(pcb[i:])
        break
    s = j + 2
    e = _block(pcb, s)
    blk = pcb[s:e]
    head = blk.split(None, 1)[0]
    drop = head in ('(segment', '(via', '(arc')
    if head == '(gr_text' and blk.startswith('(gr_text "SDA"'):
        label_tpl = blk
    out.append(pcb[i:j])
    if not drop:
        out.append(pcb[j:e])
    i = e
pcb = ''.join(out)
assert label_tpl is not None


def silk_text(s, x, y, size):
    blk = re.sub(r'\(gr_text "[^"]*"', '(gr_text "%s"' % s, label_tpl, count=1)
    blk = re.sub(r'\(at [-\d.]+ [-\d.]+ ', '(at %.2f %.2f ' % (x, y), blk, count=1)
    blk = re.sub(r'\(uuid "[^"]*"\)', '(uuid "%s")' % uuid.uuid4(), blk, count=1)
    blk = re.sub(r'\(size [\d.]+ [\d.]+\)', '(size %.2f %.2f)' % (size, size), blk, count=1)
    blk = re.sub(r'\(thickness [\d.]+\)', '(thickness %.3f)' % (size / 4.0), blk, count=1)
    return '\t' + blk + '\n'


exec(open(os.path.join(HERE, 'placement34.py')).read())     # defines P, SILK_TEXT, REF_AT

labels = ''.join(silk_text(s, x, y, size) for s, x, y, size in SILK_TEXT)
k = pcb.rfind('\n\t(gr_text ')
k = pcb.index('\n', _block(pcb, k + 2) - 1) + 1
pcb = pcb[:k] + labels + pcb[k:]
assert pcb.count('"REV 3.3"') == 1 and pcb.count('(rev "3.3")') == 1 and pcb.count('(date "2026-11-14")') == 1
pcb = pcb.replace('"REV 3.3"', '"REV 3.4"').replace('(rev "3.3")', '(rev "3.4")').replace('(date "2026-11-14")', '(date "2026-11-15")')
open(DST, 'w', encoding='utf-8', newline='\n').write(pcb)

b = pcbnew.LoadBoard(DST)
assert len(list(b.GetTracks())) == 0


def lib_path(lib):
    if lib == 'Governor':
        return PRETTY
    local = os.path.join(PROJ, lib)            # project-local libraries sit beside the board
    if os.path.isdir(local):
        return local
    return '%s/%s.pretty' % (FPROOT, lib)


def load_fp(ref):
    lib, name = comps[ref]['fp'].split(':')
    fp = pcbnew.FootprintLoad(lib_path(lib), name)
    assert fp is not None, comps[ref]['fp']
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(ref)
    b.Add(fp)
    return fp


added = []
for ref in sorted(comps):
    fp = b.FindFootprintByReference(ref)
    if fp is None:
        fp = load_fp(ref)
        added.append(ref)
    fp.SetValue(comps[ref]['value'])
    fp.SetPath(pcbnew.KIID_PATH('/' + comps[ref]['ts']))
    fp.SetSheetname('/')
    fp.SetSheetfile('350A_governor.kicad_sch')
    fp.SetDNP(comps[ref]['dnp'])
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
print('footprints added:', ' '.join(added))


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
placed = set()
for ref, pad, x, y, rot in P:
    place(ref, pad, x, y, rot)
    placed.add(ref)
assert not (set(added) - placed), 'new footprints without a placement: %s' % (set(added) - placed)

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


exec(open(os.path.join(HERE, 'routes34.py')).read())

for net, width, layer, pts in routes:
    assert layer == 'F.Cu', 'Rev 3.4 rule: every track on F.Cu'
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
        t.SetLayer(pcbnew.F_Cu)
        t.SetNet(ni)
        b.Add(t)

# ------------------------------------------------------------------ zones
# The two solid GND zones (F.Cu and B.Cu) come from Rev 3.3 unchanged. They are
# left unfilled here; the DRC step refills them with the project rules.
for z in b.Zones():
    z.UnFill()

# ------------------------------------------------------------------ silkscreen
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

# individual reference positions
for ref, x, y in REF_AT:
    b.FindFootprintByReference(ref).Reference().SetPosition(V(x, y))

pcbnew.SaveBoard(DST, b)

print('== pad table')
for ref in sorted(comps, key=lambda r: (r.rstrip('0123456789'), int(''.join(ch for ch in r if ch.isdigit()) or 0))):
    fp = b.FindFootprintByReference(ref)
    if ref in ('A1',):
        continue
    print('%-4s %-9s' % (ref, comps[ref]['value'][:9]), ' '.join('%s:(%.2f,%.2f)%s' % (
        p.GetNumber(), mm(p.GetPosition().x), mm(p.GetPosition().y), p.GetNetname().replace('unconnected-', 'nc-')[:11]) for p in fp.Pads()))
