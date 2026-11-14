"""Rev 3.2 -> Rev 3.3 schematic edit. Text-level, cloning existing blocks so the
file format stays exactly what KiCad 10 wrote."""
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


def top_block_containing(t, idx, head='\n\t(symbol\n'):
    s = t.rfind(head, 0, idx)
    assert s >= 0, head
    s += 1  # skip the newline
    e = block_end(t, s + 1)  # s points at the tab; s+1 at '('
    return s, e


def symbol_span(t, ref):
    idx = t.find('(property "Reference" "%s"\n' % ref)
    assert idx >= 0, ref
    return top_block_containing(t, idx)


def remove_span(t, s, e):
    # remove block plus its trailing newline
    assert t[e] == '\n'
    return t[:s] + t[e + 1:]


def find_item(t, kind, at):
    """top-level label/no_connect/junction located by exact '(at x y' text"""
    pat = re.compile(r'\n\t\(%s[^\n]*\n\t\t\(at %s[ )]' % (kind, re.escape(at)))
    m = pat.search(t)
    assert m, (kind, at)
    s = m.start() + 1
    return s, block_end(t, s + 1)


def find_label(t, name, at):
    pat = re.compile(r'\n\t\(label "%s"\n\t\t\(at %s ' % (re.escape(name), re.escape(at)))
    m = pat.search(t)
    assert m, (name, at)
    s = m.start() + 1
    return s, block_end(t, s + 1)


def find_wire(t, a, b):
    pat = '\n\t(wire\n\t\t(pts\n\t\t\t(xy %s) (xy %s)\n' % (a, b)
    i = t.find(pat)
    assert i >= 0, (a, b)
    s = i + 1
    return s, block_end(t, s + 1)


def relabel(t, old, at, new):
    s, e = find_label(t, old, at)
    blk = t[s:e].replace('(label "%s"' % old, '(label "%s"' % new, 1)
    return t[:s] + blk + t[e:]


def set_prop(t, ref, prop, old, new):
    s, e = symbol_span(t, ref)
    blk = t[s:e]
    a = '(property "%s" "%s"' % (prop, old)
    assert a in blk, (ref, prop, old)
    blk = blk.replace(a, '(property "%s" "%s"' % (prop, new), 1)
    return t[:s] + blk + t[e:]


# ---------------------------------------------------------------- new items
new_items = []


def label(name, x, y, rot):
    just = 'left bottom' if rot in (0, 90) else 'right bottom'
    new_items.append(
        '\t(label "%s"\n\t\t(at %s %s %d)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1.27 1.27)\n\t\t\t)\n'
        '\t\t\t(justify %s)\n\t\t)\n\t\t(uuid "%s")\n\t)\n' % (name, fmt(x), fmt(y), rot, just, u()))


def wire(x1, y1, x2, y2):
    new_items.append(
        '\t(wire\n\t\t(pts\n\t\t\t(xy %s %s) (xy %s %s)\n\t\t)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
        '\t\t(uuid "%s")\n\t)\n' % (fmt(x1), fmt(y1), fmt(x2), fmt(y2), u()))


def junction(x, y):
    new_items.append(
        '\t(junction\n\t\t(at %s %s)\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)\n\t\t(uuid "%s")\n\t)\n' % (fmt(x), fmt(y), u()))


def no_connect(x, y):
    new_items.append('\t(no_connect\n\t\t(at %s %s)\n\t\t(uuid "%s")\n\t)\n' % (fmt(x), fmt(y), u()))


def rect(x1, y1, x2, y2):
    new_items.append(
        '\t(rectangle\n\t\t(start %s %s)\n\t\t(end %s %s)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
        '\t\t(fill\n\t\t\t(type none)\n\t\t)\n\t\t(uuid "%s")\n\t)\n' % (fmt(x1), fmt(y1), fmt(x2), fmt(y2), u()))


def note(s, x, y, size=2.54):
    new_items.append(
        '\t(text "%s"\n\t\t(exclude_from_sim no)\n\t\t(at %s %s 0)\n\t\t(effects\n\t\t\t(font\n\t\t\t\t(size %s %s)\n'
        '\t\t\t\t(thickness %s)\n\t\t\t\t(bold yes)\n\t\t\t)\n\t\t)\n\t\t(uuid "%s")\n\t)\n'
        % (s, fmt(x), fmt(y), fmt(size), fmt(size), fmt(size / 5), u()))


AT_RE = re.compile(r'\(at (-?[\d.]+) (-?[\d.]+) (-?\d+)\)')
UUID_RE = re.compile(r'\(uuid "[0-9a-f-]+"\)')
SRC_TEXT = text  # clone from the untouched original


def clone(src_ref, new_ref, x, y, value=None, footprint=None, lib_id=None):
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
        blk = re.sub(r'\(property "Datasheet" "[^"]*"', '(property "Datasheet" ""', blk, count=1)
        blk = re.sub(r'\(property "Description" "[^"]*"', '(property "Description" ""', blk, count=1)
    while '(property "Sim.' in blk:      # simulation fields have no footprint counterpart
        i = blk.index('(property "Sim.')
        ls_ = blk.rfind('\n', 0, i) + 1
        blk = blk[:ls_] + blk[block_end(blk, i) + 1:]
    new_items.append(blk + '\n')


pwr_n = [30]


def gnd(x, y):
    pwr_n[0] += 1
    clone('#PWR018', '#PWR0%d' % pwr_n[0], x, y)


R_FP = 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal'
C_FP = 'Capacitor_THT:C_Disc_D3.8mm_W2.6mm_P2.50mm'


def gndw(x, y):                    # ground symbol on a 2.54 mm stub below a pin
    wire(x, y, x, y + 2.54)
    gnd(x, y + 2.54)


def r_vert(ref, value, x, y):      # pin1 top (x, y-3.81), pin2 bottom
    clone('R1', ref, x, y, value, R_FP)


def r_horiz(ref, value, x, y):     # pin1 left (x-3.81, y), pin2 right
    clone('R5', ref, x, y, value, R_FP)


def c_vert(ref, value, x, y):      # pin1 top
    clone('C1', ref, x, y, value, C_FP)


def d_vert(ref, value, x, y, fp, lib):   # K top (x, y-3.81), A bottom
    clone('D3', ref, x, y, value, fp, lib)


# ---------------------------------------------------------------- edits to existing content
t = text

# title block
t = t.replace('(date "2026-07-25")', '(date "2026-11-14")', 1)
t = t.replace('(rev "3.2")', '(rev "3.3")', 1)

# U1 enable: was tied to GND (forces the output off). Now VCC5.
for a, b in (('110.49 167.64', '120.65 167.64'), ('120.65 167.64', '120.65 172.72')):
    s, e = find_wire(t, a, b)
    t = remove_span(t, s, e)
s, e = symbol_span(t, '#PWR07')
t = remove_span(t, s, e)
label('VCC5', 110.49, 167.64, 0)

# PNP base nets get their own names (series base resistor goes between)
t = relabel(t, 'Q3_COLL', '223.52 140.97', 'Q1_BASE')
t = relabel(t, 'Q4_COLL', '222.25 200.66', 'Q2_BASE')
# magneto series resistor split in two
t = relabel(t, 'MAG_FILT', '34.29 185.42', 'MAG_MID')
t = set_prop(t, 'R1', 'Value', '10k', '5k1')
# bulk cap moves behind the fuse
t = relabel(t, '12V_IN', '88.9 68.58', '12V_RAW')
t = set_prop(t, 'C2', 'Value', '100uF/25V', '100uF/50V')
t = set_prop(t, 'C2', 'Footprint', 'CP_Radial_D5.0mm_P2.50mm', 'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm')
t = set_prop(t, 'F1', 'Footprint', 'Fuse:Fuse_BelFuse_0ZRE0012FF_L8.3mm_W3.8mm', 'Fuse:Fuse_BelFuse_0ZRE0100FF_L18.7mm_W5.1mm')
t = set_prop(t, 'R2', 'Footprint', 'R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal',
             'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P15.24mm_Horizontal')
for led in ('D1', 'D2'):
    t = set_prop(t, led, 'Footprint', '', 'LED_THT:LED_D3.0mm')
for q in ('Q1', 'Q2'):
    t = set_prop(t, q, 'Footprint', 'Package_TO_SOT_THT:TO-92L_Inline', 'Package_TO_SOT_THT:TO-92L_Inline_Wide')

# J1: E = pot high side, F = pot return (GND)
t = relabel(t, 'GND', '48.26 74.93', 'POT_HI')
label('GND', 48.26, 77.47, 0)
t = set_prop(t, 'J1', 'Footprint', 'PinHeader_1x07_P2.54mm_Vertical', 'Governor:PinHeader_1x07_P2.54mm_Vertical_Pin7Offset')

# rotary switch nets go away
for name, ats in (('D3_SW1', ('114.3 100.33', '179.07 62.23')), ('D4_SW2', ('114.3 102.87', '179.07 64.77')),
                  ('D5_SW3', ('114.3 105.41', '179.07 67.31')), ('D10_SW4', ('114.3 107.95', '179.07 80.01')),
                  ('D11_SW5', ('114.3 110.49', '179.07 82.55')), ('GND', ('114.3 113.03',))):
    for at in ats:
        s, e = find_label(t, name, at)
        t = remove_span(t, s, e)

# J2 becomes a 4-pin display header: 5V, GND, SCL, SDA
t = relabel(t, 'SDA', '114.3 95.25', 'TMP_SCL')
t = relabel(t, 'SCL', '114.3 97.79', 'SDA')
t = relabel(t, 'TMP_SCL', '114.3 95.25', 'SCL')
s, e = symbol_span(t, 'J2')
blk = t[s:e]
blk = blk.replace('(lib_id "Connector_Generic:Conn_01x10")', '(lib_id "Connector_Generic:Conn_01x04")', 1)
blk = blk.replace('(at 110.49 102.87 0)', '(at 110.49 95.25 0)', 1)
blk = blk.replace('(property "Value" "PANEL_10PIN"', '(property "Value" "OLED_4PIN"', 1)
blk = blk.replace('(property "Footprint" "PinHeader_1x10_P2.54mm_Vertical"',
                  '(property "Footprint" "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"', 1)
for n in range(5, 11):
    blk, k = re.subn(r'\t\t\(pin "%d"\n\t\t\t\(uuid "[0-9a-f-]+"\)\n\t\t\)\n' % n, '', blk)
    assert k == 1, n
t = t[:s] + blk + t[e:]

# embedded library symbols: 4-pin connector, small-signal diode, TVS
ls = t.find('\t(lib_symbols\n')


def lib_block(t, name):
    i = t.find('\t\t(symbol "%s"\n' % name, ls)
    assert i >= 0, name
    return i, block_end(t, i + 2)


i, j = lib_block(t, 'Connector_Generic:Conn_01x05')
conn = t[i:j].replace('Conn_01x05', 'Conn_01x04')
p = conn.find('(pin passive line\n\t\t\t\t\t(at 3.81 -5.08 180)')
assert p >= 0
ps = conn.rfind('\n', 0, p) + 1
pe = block_end(conn, p)
conn = conn[:ps] + conn[pe + 1:]
assert '(start -1.27 -6.35)' in conn
conn = conn.replace('(start -1.27 -6.35)', '(start -1.27 -3.81)', 1)
i, j = lib_block(t, 'Diode:1N4001')
d = t[i:j]
d4148 = d.replace('"Diode:1N4001"', '"Diode:1N4148"').replace('"1N4001_', '"1N4148_').replace('(property "Value" "1N4001"', '(property "Value" "1N4148"')
dtvs = d.replace('"Diode:1N4001"', '"Custom:D_TVS"').replace('"1N4001_', '"D_TVS_').replace('(property "Value" "1N4001"', '(property "Value" "D_TVS"')
t = t[:ls + len('\t(lib_symbols\n')] + conn + '\n' + d4148 + '\n' + dtvs + '\n' + t[ls + len('\t(lib_symbols\n'):]

# ---------------------------------------------------------------- new circuitry
# pull-downs on the driver inputs (hold the NPNs off while the Nano pins float in reset)
r_vert('R10', '10k', 147.32, 147.32)
wire(147.32, 143.51, 147.32, 139.7)
junction(147.32, 139.7)
gndw(147.32, 151.13)
r_vert('R12', '10k', 147.32, 208.28)
wire(147.32, 204.47, 147.32, 200.66)
junction(147.32, 200.66)
gndw(147.32, 212.09)
# PNP base series resistors (the missing current limit)
r_horiz('R11', '1k', 223.52, 152.4)
wire(219.71, 152.4, 214.63, 152.4)
label('Q3_COLL', 214.63, 152.4, 180)
wire(227.33, 152.4, 227.33, 140.97)
junction(227.33, 140.97)
r_horiz('R13', '1k', 223.52, 212.09)
wire(219.71, 212.09, 214.63, 212.09)
label('Q4_COLL', 214.63, 212.09, 180)
wire(227.33, 212.09, 227.33, 200.66)
junction(227.33, 200.66)


def g(n):
    return round(n * G, 2)


# --- POT INPUT box
rect(g(211), g(115), g(262), g(150))
note('POT INPUT  (J1 E / F)', g(236), g(118))
r_vert('R18', '10k', g(230), g(127))
wire(g(230), g(124), g(230), g(122))
label('VCC5', g(230), g(122), 90)
wire(g(230), g(130), g(230), g(132))
junction(g(230), g(132))
wire(g(230), g(132), g(222), g(132))
label('POT_HI', g(222), g(132), 180)
wire(g(230), g(132), g(235), g(132))
r_horiz('R19', '10k', g(238), g(132))
wire(g(241), g(132), g(250), g(132))
label('POT_SENSE', g(250), g(132), 0)
c_vert('C6', '100nF', g(246), g(137))
wire(g(246), g(134), g(246), g(132))
junction(g(246), g(132))
gndw(g(246), g(140))

# --- RELAY READBACK box
rect(g(211), g(155), g(262), g(205))
note('RELAY OUTPUT READBACK', g(236), g(158))
for name, sense, y, ra, rb in (('INC_RELAY', 'INC_SENSE', 168, 'R14', 'R15'), ('DEC_RELAY', 'DEC_SENSE', 188, 'R16', 'R17')):
    label(name, g(224), g(y), 180)
    wire(g(224), g(y), g(229), g(y))
    r_horiz(ra, '33k', g(232), g(y))
    wire(g(235), g(y), g(250), g(y))
    label(sense, g(250), g(y), 0)
    r_vert(rb, '10k', g(242), g(y + 5))
    wire(g(242), g(y + 2), g(242), g(y))
    junction(g(242), g(y))
    gndw(g(242), g(y + 8))

# --- I2C pull-ups box
rect(g(266), g(61), g(310), g(110))
note('I2C PULL-UPS', g(288), g(64))
for ref, name, x in (('R20', 'SCL', 278), ('R21', 'SDA', 294)):
    r_vert(ref, '4k7', g(x), g(84))
    wire(g(x), g(81), g(x), g(79))
    label('VCC5', g(x), g(79), 90)
    wire(g(x), g(87), g(x), g(89))
    label(name, g(x), g(89), 270)

# --- 12 V input protection box
rect(g(266), g(115), g(310), g(150))
note('12V TRANSIENT CLAMP', g(288), g(118))
d_vert('D5', 'P6KE22A', g(288), g(134), 'Diode_THT:D_DO-15_P10.16mm_Horizontal', 'Custom:D_TVS')
wire(g(288), g(131), g(288), g(129))
label('12V_RAW', g(288), g(129), 90)
gndw(g(288), g(137))

# --- magneto input box
rect(g(266), g(155), g(310), g(205))
note('MAG INPUT (2nd series R, LED reverse clamp)', g(288), g(158), 1.6)
label('MAG_MID', g(274), g(172), 180)
wire(g(274), g(172), g(279), g(172))
r_horiz('R9', '5k1', g(282), g(172))
wire(g(285), g(172), g(300), g(172))
label('MAG_FILT', g(300), g(172), 0)
d_vert('D6', '1N4148', g(292), g(177), 'Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal', 'Diode:1N4148')
wire(g(292), g(174), g(292), g(172))
junction(g(292), g(172))
gndw(g(292), g(180))

# Nano analog inputs
label('INC_SENSE', 204.47, 77.47, 0)
label('DEC_SENSE', 204.47, 85.09, 0)
label('POT_SENSE', 204.47, 87.63, 0)

# explicit no-connects on pins that are intentionally unused
for xy in ((179.07, 54.61), (179.07, 57.15), (179.07, 62.23), (179.07, 64.77), (179.07, 67.31), (179.07, 80.01),
           (179.07, 82.55), (179.07, 85.09), (179.07, 87.63), (204.47, 54.61), (204.47, 57.15), (204.47, 64.77),
           (204.47, 69.85), (204.47, 72.39), (204.47, 74.93), (194.31, 44.45), (189.23, 44.45), (113.03, 54.61)):
    no_connect(*xy)

# append the new items just before (sheet_instances
k = t.find('\n\t(sheet_instances')
assert k >= 0
t = t[:k + 1] + ''.join(new_items) + t[k + 1:]
open(DST, 'w', encoding='utf-8', newline='\n').write(t)
print('written', DST, len(t), 'bytes;', len(new_items), 'new items')
