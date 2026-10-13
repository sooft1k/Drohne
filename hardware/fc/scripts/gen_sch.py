# Erzeugt fc.kicad_sch aus design.py.
#
# Jeder beschaltete Pin bekommt ein kurzes Drahtstueck und an dessen Ende
# ein Netzlabel bzw. ein Stromversorgungssymbol. Unbeschaltete Pins bekommen
# eine No-Connect-Markierung. Bauteile sind nach Funktionsgruppen geordnet.
import math
import os
import sys
import uuid as uuidlib
sys.path.insert(0, os.path.dirname(__file__))
from sexp import parse, dump, find, find1, Sym
import design

PRJ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
KISYM = '/usr/share/kicad/symbols'
PROJECT = 'fc'
GRID = 2.54
STUB = 2.54

_ns = uuidlib.UUID('6b1f6a3c-6b55-4f5e-9a7d-2f0c1e4d9a11')


def uid(*key):
    return str(uuidlib.uuid5(_ns, '/'.join(str(k) for k in key)))


ROOT_UUID = uid('root-sheet')

# ------------------------------------------------------------------ Symbole laden
_libcache = {}


def lib_symbols(libname):
    if libname not in _libcache:
        if libname == 'fc_custom':
            path = os.path.join(PRJ, 'fc_custom.kicad_sym')
        else:
            path = os.path.join(KISYM, libname + '.kicad_sym')
        t = parse(open(path).read())[0]
        _libcache[libname] = {s[1]: s for s in find(t, 'symbol')}
    return _libcache[libname]


def rename_units(node, old, new):
    out = []
    for x in node:
        if isinstance(x, list) and x and x[0] == 'symbol' and isinstance(x[1], str) \
                and x[1].startswith(old + '_'):
            x = [x[0], new + x[1][len(old):]] + x[2:]
        out.append(x)
    return out


def flat_symbol(lib_id):
    """Symboldefinition fuer lib_symbols: aufgeloestes extends, Name Lib:Name."""
    libname, name = lib_id.split(':')
    syms = lib_symbols(libname)
    s = syms[name]
    ext = find1(s, 'extends')
    if ext:
        parent = syms[ext[1]]
        props = {p[1]: p for p in find(s, 'property')}
        out = [Sym('symbol'), lib_id]
        for x in parent[2:]:
            if isinstance(x, list) and x and x[0] == 'property':
                out.append(props.pop(x[1], x))
            elif isinstance(x, list) and x and x[0] == 'symbol':
                out.append([x[0], name + x[1][len(ext[1]):]] + x[2:])
            else:
                out.append(x)
        for p in props.values():
            out.append(p)
        return out
    out = [Sym('symbol'), lib_id] + [x for x in s[2:]]
    return out


def sym_pins(flat):
    pins = []
    for sub in find(flat, 'symbol'):
        unit = int(sub[1].split('_')[-2])
        for p in find(sub, 'pin'):
            at = find1(p, 'at')
            pins.append(dict(number=find1(p, 'number')[1], name=find1(p, 'name')[1],
                             x=float(at[1]), y=float(at[2]), angle=float(at[3]) % 360,
                             etype=str(p[1]), unit=unit))
    return pins


def sym_props(flat):
    return {p[1]: p for p in find(flat, 'property')}


# ------------------------------------------------------------------ Geometrie
def outward(angle):
    """Richtung vom Pin-Anschlusspunkt weg vom Bauteil, Schaltplan-Koordinaten."""
    a = math.radians(angle)
    return (-round(math.cos(a)), round(math.sin(a)))


def fx(size=1.27, hide=False, justify=None, bold=False):
    font = [Sym('font'), [Sym('size'), size, size]]
    if bold:
        font.append(Sym('bold'))
    e = [Sym('effects'), font]
    if justify:
        e.append([Sym('justify')] + [Sym(j) for j in justify])
    if hide:
        e.append(Sym('hide'))
    return e


class Sheet:
    def __init__(self):
        self.items = []
        self.lib = {}
        self.pwr_count = 0
        self.flag_count = 0

    def need(self, lib_id):
        if lib_id not in self.lib:
            self.lib[lib_id] = flat_symbol(lib_id)
        return self.lib[lib_id]

    def wire(self, x1, y1, x2, y2):
        self.items.append([Sym('wire'), [Sym('pts'), [Sym('xy'), x1, y1], [Sym('xy'), x2, y2]],
                           [Sym('stroke'), [Sym('width'), 0], [Sym('type'), Sym('default')]],
                           [Sym('uuid'), uid('w', x1, y1, x2, y2)]])

    def label(self, name, x, y, d):
        angle = {(-1, 0): 180, (1, 0): 0, (0, -1): 90, (0, 1): 270}[d]
        just = ['left', 'bottom'] if angle in (0, 90) else ['right', 'bottom']
        self.items.append([Sym('label'), name, [Sym('at'), x, y, angle],
                           [Sym('fields_autoplaced')], fx(justify=just),
                           [Sym('uuid'), uid('l', name, x, y)]])

    def noconnect(self, x, y):
        self.items.append([Sym('no_connect'), [Sym('at'), x, y], [Sym('uuid'), uid('nc', x, y)]])

    def text(self, s, x, y, size=1.27, bold=False):
        self.items.append([Sym('text'), s, [Sym('at'), x, y, 0],
                           fx(size, justify=['left', 'bottom'], bold=bold),
                           [Sym('uuid'), uid('t', s, x, y)]])

    def rect(self, x1, y1, x2, y2):
        self.items.append([Sym('rectangle'), [Sym('start'), x1, y1], [Sym('end'), x2, y2],
                           [Sym('stroke'), [Sym('width'), 0], [Sym('type'), Sym('dash')]],
                           [Sym('fill'), [Sym('type'), Sym('none')]],
                           [Sym('uuid'), uid('r', x1, y1)]])

    def symbol(self, lib_id, ref, value, x, y, rot=0, footprint='', fields=None,
               hide_ref=False, in_bom=True, on_board=True, extra_props=None):
        flat = self.need(lib_id)
        props = sym_props(flat)
        su = uid('s', ref)
        out = [Sym('symbol'), [Sym('lib_id'), lib_id], [Sym('at'), x, y, rot],
               [Sym('unit'), 1], [Sym('in_bom'), Sym('yes' if in_bom else 'no')],
               [Sym('on_board'), Sym('yes' if on_board else 'no')],
               [Sym('dnp'), Sym('no')], [Sym('uuid'), su]]

        def place(pname, val, hide=False):
            p = props.get(pname)
            if p is not None:
                at = find1(p, 'at')
                lx, ly = float(at[1]), float(at[2])
                la = float(at[3]) if len(at) > 3 else 0
                eff = find1(p, 'effects')
                just = find1(eff, 'justify')
                hid = hide or ('hide' in [str(e) for e in eff])
            else:
                lx, ly, la, just, hid = 0, 0, 0, None, True
            r = math.radians(rot)
            sx, sy = lx, -ly
            px = x + sx * math.cos(r) + sy * math.sin(r)
            py = y - sx * math.sin(r) + sy * math.cos(r)
            e = fx(hide=hid, justify=[str(j) for j in just[1:]] if just else None)
            ang = (la + rot) % 180
            return [Sym('property'), pname, val, [Sym('at'), round(px, 3), round(py, 3), ang], e]

        out.append(place('Reference', ref, hide=hide_ref))
        out.append(place('Value', value))
        out.append(place('Footprint', footprint, hide=True))
        out.append(place('Datasheet', props['Datasheet'][2] if 'Datasheet' in props else '',
                         hide=True))
        for k, v in (extra_props or {}).items():
            out.append([Sym('property'), k, v, [Sym('at'), x, y, 0], fx(hide=True)])
        for p in sym_pins(flat):
            out.append([Sym('pin'), p['number'], [Sym('uuid'), uid('p', ref, p['number'])]])
        out.append([Sym('instances'), [Sym('project'), PROJECT,
                    [Sym('path'), '/' + ROOT_UUID, [Sym('reference'), ref], [Sym('unit'), 1]]]])
        self.items.append(out)
        return flat

    def power(self, net, x, y, d):
        lib_id = design.POWER_SYMBOLS[net]
        down_type = (net == 'GND')
        if down_type:
            rot = {(0, 1): 0, (0, -1): 180, (-1, 0): 270, (1, 0): 90}[d]
        else:
            rot = {(0, -1): 0, (0, 1): 180, (-1, 0): 90, (1, 0): 270}[d]
        self.pwr_count += 1
        self.symbol(lib_id, '#PWR%03d' % self.pwr_count, net, x, y, rot, hide_ref=True)

    def pwr_flag(self, x, y):
        self.flag_count += 1
        self.symbol('power:PWR_FLAG', '#FLG%02d' % self.flag_count, 'PWR_FLAG', x, y, 0,
                    hide_ref=True)


def snap(v):
    return round(round(v / GRID) * GRID, 3)


# ------------------------------------------------------------------ Bauteil-Ausdehnung
def part_extent(p, flat):
    pins = [q for q in sym_pins(flat) if q['unit'] in (0, 1)]
    xs, ys = [0.0], [0.0]
    for g in find(flat, 'symbol'):
        for r in find(g, 'rectangle'):
            s, e = find1(r, 'start'), find1(r, 'end')
            xs += [float(s[1]), float(e[1])]
            ys += [-float(s[2]), -float(e[2])]
    for q in pins:
        net = p['pins'].get(q['number'])
        d = outward(q['angle'])
        px, py = q['x'], -q['y']
        if net in design.POWER_SYMBOLS:
            ext = STUB + 10.16 + 6
        elif net:
            ext = STUB + 1.0 * len(net) + 2
        else:
            ext = 2
        xs += [px, px + d[0] * ext]
        ys += [py, py + d[1] * ext]
    # Platz fuer Referenz/Wert rechts von kleinen Bauteilen
    xs.append(max(xs) + 13)
    return min(xs), min(ys), max(xs), max(ys)


def draw_part(sh, p, ox, oy):
    flat = sh.symbol(p['sym'], p['ref'], p['value'], ox, oy, 0, footprint=p['fp'],
                     extra_props={'Beschreibung': p['desc']} if p['desc'] else None)
    pins = [q for q in sym_pins(flat) if q['unit'] in (0, 1)]
    done = {}
    for q in pins:
        px, py = round(ox + q['x'], 3), round(oy - q['y'], 3)
        net = p['pins'].get(q['number'], 'MISSING')
        assert net != 'MISSING', (p['ref'], q['number'])
        key = (px, py)
        if key in done:
            assert done[key] == net, ('gestapelte Pins mit verschiedenen Netzen', p['ref'], q)
            continue
        done[key] = net
        if net is None:
            sh.noconnect(px, py)
            continue
        d = outward(q['angle'])
        stub = STUB
        if net in design.POWER_SYMBOLS:
            along = q['x'] if d[0] == 0 else q['y']
            stub = STUB + 5.08 * (int(round(along / GRID)) % 3)
        ex, ey = round(px + d[0] * stub, 3), round(py + d[1] * stub, 3)
        sh.wire(px, py, ex, ey)
        if net in design.POWER_SYMBOLS:
            sh.power(net, ex, ey, d)
        else:
            sh.label(net, ex, ey, d)


# ------------------------------------------------------------------ Layout
GROUP_ORDER = ['mcu', 'gyro', 'flash', 'meas', 'pwr_in', 'buck', 'ldo', 'usb',
               'led', 'buz', 'i2c', 'conn', 'mech']
GROUP_WIDTH = {'mcu': 120, 'conn': 330, 'mech': 200}


def layout_group(sh, parts, gx, gy, maxw):
    """Reihenweise Anordnung; liefert Breite/Hoehe des belegten Bereichs."""
    x, y, rowh, used_w = 0.0, 0.0, 0.0, 0.0
    placed = []
    for p in parts:
        flat = sh.need(p['sym'])
        x0, y0, x1, y1 = part_extent(p, flat)
        w, h = x1 - x0 + 4, y1 - y0 + 4
        if x > 0 and x + w > maxw:
            x, y, rowh = 0.0, y + rowh, 0.0
        placed.append((p, gx + x - x0 + 2, gy + y - y0 + 2))
        x += w
        rowh = max(rowh, h)
        used_w = max(used_w, x)
    return placed, used_w, y + rowh


def build():
    sh = Sheet()
    sheet_w, sheet_h = 841.0, 594.0
    margin = 15.0
    cx, cy, colh = margin, margin + 10, 0.0
    sh.text('Flight Controller 5 Zoll - STM32F405 / ICM-42688-P / W25Q128', margin, margin + 2,
            size=3.5, bold=True)
    for g in GROUP_ORDER:
        parts = [p for p in design.PARTS if p['group'] == g]
        maxw = GROUP_WIDTH.get(g, 150)
        placed, w, h = layout_group(sh, parts, 0, 0, maxw)
        w, h = w + 6, h + 10
        if cx + w > sheet_w - margin:
            cx, cy, colh = margin, cy + colh + 8, 0.0
        sh.rect(snap(cx), snap(cy), snap(cx + w), snap(cy + h))
        sh.text(design.GROUP_TITLES[g], snap(cx) + 2, snap(cy) + 5, size=2.5, bold=True)
        for p, px, py in placed:
            draw_part(sh, p, snap(cx + px + 2), snap(cy + py + 8))
        cx += w + 8
        colh = max(colh, h)
    # PWR_FLAG-Reihe unten links
    fy = snap(cy + colh + 15)
    sh.text('Versorgungsmarkierungen (PWR_FLAG) fuer die elektrische Regelpruefung', margin, fy - 8,
            size=1.8, bold=True)
    for i, net in enumerate(design.PWR_FLAG_NETS):
        x = snap(margin + 10 + i * 25.4)
        sh.pwr_flag(x, fy)
        sh.wire(x, fy, x, fy + 5.08)
        if net in design.POWER_SYMBOLS:
            sh.power(net, x, fy + 5.08, (0, 1))
        else:
            sh.label(net, x, fy + 5.08, (0, 1))
    notes = [
        'Hinweise:',
        '- Pinbelegung MCU siehe README.md (verbindlich: TIM3 Motoren PB0/PB1/PA6/PA7, PC0 Akku, PA13/PA14 SWD, PA11/PA12 USB).',
        '- Q1: Si2319CDS (40 V / +-20 V) statt SI2301 (nur 20 V / +-8 V, fuer 6S ungeeignet). Gleicher SOT-23-Footprint.',
        '- U4: LMR33630B (36 V Eingang) statt MP1584/TPS54331 (28 V, zu knapp fuer 6S).',
        '- J2 ESC: Pinfolge 1 VBAT, 2 GND, 3 CURR, 4 5V, 5-8 M1-M4 - vor dem Bestellen mit dem ESC-Kabel vergleichen!',
        '- JP2 offen: ESC-5V-Pin ist nicht mit dem FC-5V verbunden (verhindert zwei Regler gegeneinander).',
        '- U2 Pin 2/3 RESV laut Referenzdesign frei, 7/10/11 an GND. C19 an Pin 2 nur wegen der Spezifikation.',
    ]
    for i, n in enumerate(notes):
        sh.text(n, snap(margin + 200), fy - 8 + i * 4, size=1.5, bold=(i == 0))
    return sh


def write():
    sh = build()
    sch = [Sym('kicad_sch'), [Sym('version'), 20230121], [Sym('generator'), Sym('eeschema')],
           [Sym('uuid'), ROOT_UUID], [Sym('paper'), 'A1'],
           [Sym('title_block'), [Sym('title'), 'Flight Controller 5 Zoll'],
            [Sym('date'), '2026-10-13'], [Sym('rev'), '1.0'],
            [Sym('comment'), 1, 'Erzeugt aus hardware/fc/scripts/design.py'],
            [Sym('comment'), 2, '42 x 41 mm, 4 Lagen, Lochbild 30,5 mm']],
           [Sym('lib_symbols')] + list(sh.lib.values())]
    sch += sh.items
    sch.append([Sym('sheet_instances'), [Sym('path'), '/', [Sym('page'), '1']]])
    with open(os.path.join(PRJ, 'fc.kicad_sch'), 'w') as f:
        f.write(dump(sch) + '\n')
    print('Symbole:', sum(1 for i in sh.items if i[0] == 'symbol'),
          'Elemente:', len(sh.items))


if __name__ == '__main__':
    write()
