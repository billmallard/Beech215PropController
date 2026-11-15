"""Rev 3.3 -> Rev 3.4 schematic edit. Text-level, cloning existing blocks so the
file format stays exactly what KiCad 10 wrote.

Adds:
  - the driver supply switch: Q5 (PNP) between the protected 12 V rail and a
    new net 12V_SW that now feeds both relay drivers, Q6 + R22..R25 to drive
    it from Nano D5, R26/R27 to read 12V_SW back on Nano A2
  - the optional fault lamp output (J3, Q7, R28..R31, Nano D4), marked DNP
"""
import re
import sys
import uuid

SRC, DST = sys.argv[1], sys.argv[2]
text = open(SRC, encoding='utf-8').read()
G = 1.27


def u():
    return str(uuid.uuid4())


def fmt(v):
    s = ('%.4f' % v).rstrip('0').rstrip('.')
    return s if s not in ('-0', '') else '0'


def block_end(t, start):
    """index just past the balanced paren block starting at t[start] == '('"""
    depth = 0
    i = start
    in_str = False
    while i < len(t):
        c = t[i]
        if in_str:
            if c == '\\':
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError('unbalanced')


def symbol_span(t, ref):
    idx = t.find('(property "Reference" "%s"\n' % ref)
    assert idx >= 0, ref
    s = t.rfind('\n\t(symbol\n', 0, idx)
    assert s >= 0
    s += 1
    return s, block_end(t, s + 1)


def remove_span(t, s, e):
    assert t[e] == '\n'
    return t[:s] + t[e + 1:]


def find_label(t, name, at):
    pat = re.compile(r'\n\t\(label "%s"\n\t\t\(at %s ' % (re.escape(name), re.escape(at)))
    m = pat.search(t)
    assert m, (name, at)
    s = m.start() + 1
    return s, block_end(t, s + 1)


def relabel(t, old, at, new):
    s, e = find_label(t, old, at)
    blk = t[s:e].replace('(label "%s"' % old, '(label "%s"' % new, 1)
    return t[:s] + blk + t[e:]


def remove_no_connect(t, at):
    pat = '\n\t(no_connect\n\t\t(at %s)\n' % at
    i = t.find(pat)
    assert i >= 0, at
    s = i + 1
    return remove_span(t, s, block_end(t, s + 1))


# ---------------------------------------------------------------- new items
new_items = []


def label(name, x, y, rot):
    just = 'left bottom' if rot in (0, 90) else 'right bottom'
    new_items.append(
        '\t(label "%s"\n\t\t(at %s %s %d)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n'
        '\t\t\t(justify %s)\n\t\t)\n\t\t(uuid "%s")\n\t)\n' % (name, fmt(x), fmt(y), rot, just, u()))


def wire(*pts):
    for (x1, y1), (x2, y2) in zip(pts[:-1], pts[1:]):
        new_items.append(
            '\t(wire\n\t\t(pts\n\t\t\t(xy %s %s) (xy %s %s)\n\t\t)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
            '\t\t(uuid "%s")\n\t)\n' % (fmt(x1), fmt(y1), fmt(x2), fmt(y2), u()))


def junction(x, y):
    new_items.append(
        '\t(junction\n\t\t(at %s %s)\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)\n\t\t(uuid "%s")\n\t)\n' % (fmt(x), fmt(y), u()))


def rect(x1, y1, x2, y2):
    new_items.append(
        '\t(rectangle\n\t\t(start %s %s)\n\t\t(end %s %s)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
        '\t\t(fill\n\t\t\t(type none)\n\t\t)\n\t\t(uuid "%s")\n\t)\n' % (fmt(x1), fmt(y1), fmt(x2), fmt(y2), u()))


def note(s, x, y, size=2.54, bold=True):
    new_items.append(
        '\t(text "%s"\n\t\t(exclude_from_sim no)\n\t\t(at %s %s 0)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size %s %s)\n'
        '\t\t\t\t(thickness %s)\n\t\t\t\t(bold %s)\n\t\t\t)\n\t\t)\n\t\t(uuid "%s")\n\t)\n'
        % (s, fmt(x), fmt(y), fmt(size), fmt(size), fmt(size / 5 if bold else size / 8), 'yes' if bold else 'no', u()))


AT_RE = re.compile(r'\(at (-?[\d.]+) (-?[\d.]+) (-?\d+)\)')
UUID_RE = re.compile(r'\(uuid "[0-9a-f-]+"\)')
SRC_TEXT = text  # clone from the untouched original


def clone(src_ref, new_ref, x, y, value=None, footprint=None, lib_id=None, dnp=False, drop_pins=()):
    s, e = symbol_span(SRC_TEXT, src_ref)
    blk = SRC_TEXT[s:e]
    m = AT_RE.search(blk)
    sx, sy = float(m.group(1)), float(m.group(2))
    dx, dy = x - sx, y - sy

    def shift(mm):
        return '(at %s %s %s)' % (fmt(float(mm.group(1)) + dx), fmt(float(mm.group(2)) + dy), mm.group(3))
    blk = AT_RE.sub(shift, blk)
    blk = UUID_RE.sub(lambda _m: '(uuid "%s")' % u(), blk)
    blk = blk.replace('(property "Reference" "%s"' % src_ref, '(property "Reference" "%s"' % new_ref)
    blk = blk.replace('(reference "%s")' % src_ref, '(reference "%s")' % new_ref)
    if value is not None:
        blk = re.sub(r'\(property "Value" "[^"]*"', '(property "Value" "%s"' % value, blk, count=1)
    if footprint is not None:
        blk = re.sub(r'\(property "Footprint" "[^"]*"', '(property "Footprint" "%s"' % footprint, blk, count=1)
    if lib_id is not None:
        blk = re.sub(r'\(lib_id "[^"]*"\)', '(lib_id "%s")' % lib_id, blk, count=1)
    for n in drop_pins:
        blk, k = re.subn(r'\t\t\(pin "%s"\n\t\t\t\(uuid "[0-9a-f-]+"\)\n\t\t\)\n' % n, '', blk)
        assert k == 1, n
    if dnp:
        assert blk.count('(dnp no)') == 1
        blk = blk.replace('(dnp no)', '(dnp yes)')
    while '(property "Sim.' in blk:      # simulation fields have no footprint counterpart
        i = blk.index('(property "Sim.')
        ls_ = blk.rfind('\n', 0, i) + 1
        blk = blk[:ls_] + blk[block_end(blk, i) + 1:]
    new_items.append(blk + '\n')


pwr_n = [max(int(n) for n in re.findall(r'\(property "Reference" "#PWR0*(\d+)"', text))]


def gnd(x, y):
    pwr_n[0] += 1
    clone('#PWR018', '#PWR0%d' % pwr_n[0], x, y)


R_FP = 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal'
R_FP_WIDE = 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P15.24mm_Horizontal'


def gndw(x, y):                    # ground symbol on a 2.54 mm stub below a pin
    wire((x, y), (x, y + 2.54))
    gnd(x, y + 2.54)


def r_vert(ref, value, x, y, fp=R_FP, dnp=False):      # pin1 top (x, y-3.81), pin2 bottom (x, y+3.81)
    clone('R1', ref, x, y, value, fp, dnp=dnp)


def r_horiz(ref, value, x, y, fp=R_FP, dnp=False):     # pin1 left (x-3.81, y), pin2 right (x+3.81, y)
    clone('R5', ref, x, y, value, fp, dnp=dnp)


def pnp(ref, x, y):                # E (x+2.54, y-5.08)  B (x-2.54, y)  C (x+2.54, y+5.08)
    clone('Q1', ref, x, y)


def npn(ref, x, y, dnp=False):     # C (x+2.54, y-5.08)  B (x-2.54, y)  E (x+2.54, y+5.08)
    clone('Q3', ref, x, y, dnp=dnp)


def g(n):
    return round(n * G, 2)


# ---------------------------------------------------------------- edits to existing content
t = text

# title block
assert t.count('(date "2026-11-14")') == 1 and t.count('(rev "3.3")') == 1
t = t.replace('(date "2026-11-14")', '(date "2026-11-15")', 1)
t = t.replace('(rev "3.3")', '(rev "3.4")', 1)

# Both relay drivers (PNP emitters and base pull-ups) move from the protected
# 12 V rail to the switched rail. Nothing else does.
for at in ('204.47 140.97', '240.03 130.81', '203.2 200.66', '237.49 191.77'):
    t = relabel(t, '12V_RAW', at, '12V_SW')

# Nano pins that are now used: D4 (fault lamp), D5 (driver enable), A2 (switch readback)
for at, name, rot in (('179.07 64.77', 'D4_FAULT', 180), ('179.07 67.31', 'D5_DRV_EN', 180), ('204.47 74.93', 'SW_SENSE', 0)):
    t = remove_no_connect(t, at)
    x, y = (float(v) for v in at.split())
    label(name, x, y, rot)

# embedded library symbol for the 2-pin lamp header, derived from the 4-pin one
ls = t.find('\t(lib_symbols\n')
i = t.find('\t\t(symbol "Connector_Generic:Conn_01x04"\n', ls)
assert i >= 0
conn = t[i:block_end(t, i + 2)].replace('Conn_01x04', 'Conn_01x02')
for pin_at in ('(at 3.81 0 180)', '(at 3.81 -2.54 180)'):
    p = conn.find('(pin passive line\n\t\t\t\t\t' + pin_at)
    assert p >= 0, pin_at
    ps = conn.rfind('\n', 0, p) + 1
    conn = conn[:ps] + conn[block_end(conn, p) + 1:]
assert '(start -1.27 -3.81)' in conn
conn = conn.replace('(start -1.27 -3.81)', '(start -1.27 1.27)', 1)
t = t[:ls + len('\t(lib_symbols\n')] + conn + '\n' + t[ls + len('\t(lib_symbols\n'):]

# ---------------------------------------------------------------- driver supply switch
rect(g(23), g(155), g(103), g(205))
note('DRIVER SUPPLY SWITCH  (Rev 3.4)', g(63), g(158))
note('D5 HIGH closes Q5 and powers both relay drivers. R25 holds it open in reset.', g(63), g(161), 1.4, False)
X = 2          # horizontal offset of the whole block, in grid units


def gx(n):
    return g(n + X)


# Q6 level shifter, driven from Nano D5
label('D5_DRV_EN', gx(30), g(190), 180)
wire((gx(30), g(190)), (gx(34), g(190)))
r_vert('R25', '10k', gx(32), g(195))
wire((gx(32), g(192)), (gx(32), g(190)))
junction(gx(32), g(190))
gndw(gx(32), g(198))
r_horiz('R24', '1k', gx(37), g(190))
wire((gx(40), g(190)), (gx(43), g(190)))
npn('Q6', gx(45), g(190))
gndw(gx(47), g(194))
wire((gx(47), g(186)), (gx(47), g(180)), (gx(49), g(180)))
# Q5 base: R22 from the Q6 collector, R23 pull-up to the emitter rail
r_horiz('R22', '1k', gx(52), g(180))
wire((gx(55), g(180)), (gx(68), g(180)))
r_vert('R23', '10k', gx(62), g(175))
wire((gx(62), g(178)), (gx(62), g(180)))
junction(gx(62), g(180))
pnp('Q5', gx(70), g(180))
wire((gx(62), g(172)), (gx(62), g(170)), (gx(72), g(170)), (gx(72), g(176)))
wire((gx(72), g(170)), (gx(72), g(168)))
junction(gx(72), g(170))
label('12V_RAW', gx(72), g(168), 90)
# switched rail out, and its readback divider
wire((gx(72), g(184)), (gx(72), g(188)), (gx(76), g(188)))
wire((gx(72), g(188)), (gx(72), g(192)))
junction(gx(72), g(188))
label('12V_SW', gx(72), g(192), 270)
r_horiz('R26', '33k', gx(79), g(188))
wire((gx(82), g(188)), (gx(92), g(188)))
label('SW_SENSE', gx(92), g(188), 0)
r_vert('R27', '10k', gx(87), g(193))
wire((gx(87), g(190)), (gx(87), g(188)))
junction(gx(87), g(188))
gndw(gx(87), g(196))

# ---------------------------------------------------------------- optional fault lamp
rect(g(211), g(13), g(310), g(58))
note('FAULT LAMP OUTPUT  -  OPTIONAL, NOT FITTED BY DEFAULT', g(260), g(16))
note('LED across J3, anode to pin 1. D4 HIGH lights it. Fed from the 12 V rail ahead of Q5.', g(260), g(19), 1.4, False)
note('R28 and R29 limit a shorted lamp wire to about 14 mA.', g(260), g(21), 1.4, False)
LAMP_FP = 'Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical'
n_before = len(new_items)
clone('J2', 'J3', g(230), g(34), 'FAULT_LAMP', LAMP_FP, 'Connector_Generic:Conn_01x02', dnp=True, drop_pins=('3', '4'))
# J2's value text sits far below its symbol; bring J3's up beside its reference
j3 = new_items[n_before]
m = re.search(r'\(property "Value" "FAULT_LAMP"\n\t\t\t\(at [-\d.]+ [-\d.]+ 0\)', j3)
assert m
new_items[n_before] = (j3[:m.start()] + '(property "Value" "FAULT_LAMP"\n\t\t\t(at %s %s 0)' % (fmt(g(230) - 5), fmt(g(34) - 10.9))
                       + j3[m.end():])
# pin 1 (g233, g30) -> R28 -> 12 V rail
wire((g(233), g(30)), (g(240), g(30)))
label('LAMP_P', g(234), g(30), 0)
r_horiz('R28', '1k', g(243), g(30), R_FP_WIDE, dnp=True)
wire((g(246), g(30)), (g(250), g(30)))
label('12V_RAW', g(250), g(30), 0)
# pin 2 (g233, g32) -> R29 -> Q7 collector
wire((g(233), g(32)), (g(235), g(32)), (g(235), g(38)), (g(240), g(38)))
label('LAMP_N', g(235), g(38), 180)
r_horiz('R29', '1k', g(243), g(38), dnp=True)
wire((g(246), g(38)), (g(255), g(38)), (g(255), g(42)))
npn('Q7', g(253), g(46), dnp=True)
gndw(g(255), g(50))
# D4 -> R30 -> Q7 base, R31 pull-down
label('D4_FAULT', g(233), g(46), 180)
wire((g(233), g(46)), (g(242), g(46)))
r_vert('R31', '10k', g(237), g(51), dnp=True)
wire((g(237), g(48)), (g(237), g(46)))
junction(g(237), g(46))
gndw(g(237), g(54))
r_horiz('R30', '1k', g(245), g(46), dnp=True)
wire((g(248), g(46)), (g(251), g(46)))

# append the new items just before (sheet_instances
k = t.find('\n\t(sheet_instances')
assert k >= 0
t = t[:k + 1] + ''.join(new_items) + t[k + 1:]
open(DST, 'w', encoding='utf-8', newline='\n').write(t)
print('written', DST, len(t), 'bytes;', len(new_items), 'new items')
