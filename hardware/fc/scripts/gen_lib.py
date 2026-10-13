# Erzeugt die Projektbibliothek fc_custom (Symbole + Footprints).
import os
import sys
sys.path.insert(0, os.path.dirname(__file__))
from sexp import parse, dump, find, find1, Sym

PRJ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
KISYM = '/usr/share/kicad/symbols'
KIFP = '/usr/share/kicad/footprints'


def fx(size=1.27, hide=False, justify=None):
    e = [Sym('effects'), [Sym('font'), [Sym('size'), size, size]]]
    if justify:
        e.append([Sym('justify')] + [Sym(j) for j in justify])
    if hide:
        e.append(Sym('hide'))
    return e


def prop(name, value, x=0, y=0, hide=False, size=1.27, justify=None):
    return [Sym('property'), name, value, [Sym('at'), x, y, 0], fx(size, hide, justify)]


def pin(etype, x, y, angle, name, number, length=5.08, hidden=False):
    p = [Sym('pin'), Sym(etype), Sym('line'), [Sym('at'), x, y, angle],
         [Sym('length'), length]]
    if hidden:
        p.append(Sym('hide'))
    p += [[Sym('name'), name, fx()], [Sym('number'), number, fx()]]
    return p


def icm42688():
    name = 'ICM-42688-P'
    pins = [
        # links: SPI
        ('input', -15.24, 7.62, 0, 'AP_CS', '12'),
        ('input', -15.24, 5.08, 0, 'AP_SCLK', '13'),
        ('bidirectional', -15.24, 2.54, 0, 'AP_SDI', '14'),
        ('output', -15.24, 0, 0, 'AP_SDO', '1'),
        # rechts: Interrupts
        ('output', 15.24, 0, 180, 'INT1', '4'),
        ('bidirectional', 15.24, -2.54, 180, 'INT2/FSYNC/CLKIN', '9'),
        # oben: Versorgung
        ('power_in', -2.54, 17.78, 270, 'VDD', '8'),
        ('power_in', 2.54, 17.78, 270, 'VDDIO', '5'),
        # unten: Masse und reservierte Pins
        ('passive', -5.08, -17.78, 90, 'RESV', '2'),
        ('passive', -2.54, -17.78, 90, 'RESV', '3'),
        ('power_in', 0, -17.78, 90, 'GND', '6'),
        ('passive', 2.54, -17.78, 90, 'RESV', '7'),
        ('passive', 5.08, -17.78, 90, 'RESV', '10'),
        ('passive', 7.62, -17.78, 90, 'RESV', '11'),
    ]
    body = [Sym('symbol'), name + '_0_1',
            [Sym('rectangle'), [Sym('start'), -10.16, 12.7], [Sym('end'), 10.16, -12.7],
             [Sym('stroke'), [Sym('width'), 0.254], [Sym('type'), Sym('default')]],
             [Sym('fill'), [Sym('type'), Sym('background')]]]]
    pinsym = [Sym('symbol'), name + '_1_1'] + [pin(*p) for p in pins]
    return [Sym('symbol'), name, [Sym('in_bom'), Sym('yes')], [Sym('on_board'), Sym('yes')],
            prop('Reference', 'U', -10.16, 13.97, justify=['left']),
            prop('Value', name, 10.16, 13.97, justify=['right']),
            prop('Footprint', 'Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y', 0, -21.59, hide=True),
            prop('Datasheet', 'https://invensense.tdk.com/products/motion-tracking/6-axis/icm-42688-p/', 0, 0, hide=True),
            prop('ki_keywords', 'IMU gyro accelerometer SPI', 0, 0, hide=True),
            prop('ki_description', '6-Achsen-IMU (Gyro + Beschleunigung), SPI/I2C, LGA-14 2,5x3 mm. '
                 'RESV 7/10/11 an GND, RESV 2/3 frei (Referenzdesign)', 0, 0, hide=True),
            body, pinsym]


def power_symbol(newname):
    """Kopie von power:+5V mit neuem Namen (Netzname = Wert = Pinname)."""
    t = parse(open(os.path.join(KISYM, 'power.kicad_sym')).read())[0]
    src = [s for s in find(t, 'symbol') if s[1] == '+5V'][0]

    def ren(node):
        if isinstance(node, list):
            out = [ren(x) for x in node]
            if out and out[0] == 'symbol' and isinstance(out[1], str):
                out[1] = out[1].replace('+5V', newname)
            if out and out[0] == 'property' and out[1] in ('Value',):
                out[2] = newname
            if out and out[0] == 'property' and out[1] == 'ki_description':
                out[2] = 'Power symbol creates a global label with name "%s"' % newname
            if out and out[0] == 'name' and out[1] == '+5V':
                out[1] = newname
            return out
        return node
    return ren(src)


def write_symbols():
    lib = [Sym('kicad_symbol_lib'), [Sym('version'), 20220914],
           [Sym('generator'), Sym('kicad_symbol_editor')],
           icm42688(), power_symbol('VBAT'), power_symbol('+3V3A')]
    with open(os.path.join(PRJ, 'fc_custom.kicad_sym'), 'w') as f:
        f.write(dump(lib) + '\n')


def write_footprints():
    d = os.path.join(PRJ, 'fc_custom.pretty')
    os.makedirs(d, exist_ok=True)
    # LMR33630 HSOP-8: Waermeabfuhr ueber Durchkontaktierungen in die
    # Massefläche (Lage 2); kein Kupferpad auf der Unterseite, weil dort
    # Steckverbinder sitzen.
    src = 'Package_SO.pretty/Texas_HSOP-8-1EP_3.9x4.9mm_P1.27mm_ThermalVias.kicad_mod'
    t = parse(open(os.path.join(KIFP, src)).read())[0]
    newname = 'Texas_HSOP-8-1EP_3.9x4.9mm_P1.27mm_ThermalVias_TopOnly'
    t[1] = newname
    keep = []
    for x in t:
        if isinstance(x, list) and x and x[0] == 'pad':
            layers = [str(v) for v in find1(x, 'layers')[1:]]
            if layers == ['B.Cu'] or layers == ['*.Cu']:
                continue
        if isinstance(x, list) and x and x[0] == 'descr':
            x = [Sym('descr'), x[1] + ' - ohne Kupferpad und Vias auf B.Cu, Vias setzt das Layout (FC-Projekt)']
        keep.append(x)
    with open(os.path.join(d, newname + '.kicad_mod'), 'w') as f:
        f.write(dump(keep) + '\n')

    # USB-C-Buchse an der Platinenkante: Die Steckoeffnung ragt 0,5 mm ueber die Kante
    # (Footprint-y = +3,15 ist die Platinenkante). Bestueckungsdruck dahinter entfernen bzw.
    # bis 0,2 mm vor die Kante kuerzen.
    src = 'Connector_USB.pretty/USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod'
    t = parse(open(os.path.join(KIFP, src)).read())[0]
    newname = 'USB_C_Receptacle_HRO_TYPE-C-31-M-12_Kante'
    t[1] = newname
    y_max = 3.15 - 0.2
    keep = []
    for x in t:
        if isinstance(x, list) and x and x[0] == 'fp_line' and str(find1(x, 'layer')[1]) == 'F.SilkS':
            st, en = find1(x, 'start'), find1(x, 'end')
            ys, ye = float(st[2]), float(en[2])
            if ys > y_max and ye > y_max:
                continue
            if ye > y_max:
                en[2] = y_max
            if ys > y_max:
                st[2] = y_max
        if isinstance(x, list) and x and x[0] == 'descr':
            x = [Sym('descr'), x[1] + ' - Bestueckungsdruck an der Platinenkante gekuerzt (FC-Projekt)']
        keep.append(x)
    with open(os.path.join(d, newname + '.kicad_mod'), 'w') as f:
        f.write(dump(keep) + '\n')


def write_tables():
    with open(os.path.join(PRJ, 'sym-lib-table'), 'w') as f:
        f.write('(sym_lib_table\n  (version 7)\n'
                '  (lib (name "fc_custom")(type "KiCad")(uri "${KIPRJMOD}/fc_custom.kicad_sym")'
                '(options "")(descr "Projektbibliothek FC"))\n)\n')
    with open(os.path.join(PRJ, 'fp-lib-table'), 'w') as f:
        f.write('(fp_lib_table\n  (version 7)\n'
                '  (lib (name "fc_custom")(type "KiCad")(uri "${KIPRJMOD}/fc_custom.pretty")'
                '(options "")(descr "Projektbibliothek FC"))\n)\n')


if __name__ == '__main__':
    write_symbols()
    write_footprints()
    write_tables()
    print('ok')
