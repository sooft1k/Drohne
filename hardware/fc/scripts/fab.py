# Fertigungsdaten fuer JLCPCB (Standard-Bestueckung, beide Seiten):
#   fertigung/fc_jlcpcb_gerber.zip        Gerber (4 Lagen) + Excellon-Bohrdateien
#   fertigung/jlcpcb_bom.csv              Stueckliste aller bestueckten Teile (oben und unten)
#   fertigung/jlcpcb_cpl.csv              Bestueckkoordinaten beider Seiten, Drehung fuer JLCPCB
#   fertigung/jlcpcb_pruefliste.csv       je Bauteil: LCSC-Typ, Bestand, Drehung, Lage von Pin 1
import csv
import math
import os
import re
import shutil
import subprocess
import sys
import zipfile
sys.path.insert(0, os.path.dirname(__file__))
os.environ.setdefault('KICAD7_FOOTPRINT_DIR', '/usr/share/kicad/footprints')
import design
import lcsc

PRJ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(PRJ, 'fertigung')
GERBER = os.path.join(OUT, 'gerber')
PCB = os.path.join(PRJ, 'fc.kicad_pcb')
LAYERS = 'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts'
NOT_PLACED = ('H', 'FID', 'TP', 'JP')

# Drehkorrektur KiCad -> JLCPCB (wird zur KiCad-Drehung addiert, gegen den Uhrzeigersinn).
# Bestimmt durch Vergleich der Pad-Lagen der KiCad-Footprints mit den JLCPCB/EasyEDA-Footprints
# (github.com/CDFER/JLCPCB-Kicad-Library, Ordner Archived-Symbols-Footprints; der Namenszusatz
# BL/BR/TL nennt dort die Ecke von Pin 1) und mit den Tabellen von JLCKicadTools und KiBot.
CORRECTIONS = [
    (r'^LQFP-', 270, 'JLCKicadTools/KiBot (EasyEDA-QFP: Pin 1 unten links)'),
    (r'^SOIC-', 270, 'Pad-Vergleich SOIC-8_L5.3-W5.3-P1.27-LS8.0-BL, JLCKicadTools/KiBot'),
    (r'^SOT-223', 180, 'Pad-Vergleich SOT-223-3_L6.5-W3.4-P2.30-LS7.0-BR, JLCKicadTools/KiBot'),
    (r'^SOT-23-6', 270, 'Pad-Vergleich SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL, JLCKicadTools'),
    (r'^SOT-23$', 180, 'Pad-Vergleich SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR, KiBot'),
    # Elko: Footprint von C48970990 nicht verfuegbar; der EasyEDA-Elko CAP-SMD_BD8.0 (C2983323) hat
    # den Pluspol links wie KiCad. Die Tabellen von JLCKicadTools/KiBot nennen 180 (aeltere JLC-Lage)
    (r'^CP_Elec_', 0, 'Pad-Vergleich mit EasyEDA-Elko CAP-SMD_BD8.0 (Tabellen: 180) - POLUNG in der Vorschau pruefen'),
    (r'^USB_C_Receptacle_HRO_TYPE-C-31-M-12', 0,
     'Pad-Vergleich USB-C_SMD-TYPE-C-31-M-12_1 (C165948); Tabellen: 180 - Oeffnung in der Vorschau pruefen'),
    (r'^Texas_HSOP-8', 270, 'ANNAHME wie ESOP-8_..._BL-EP (Pin 1 unten links) - in der Vorschau pruefen'),
    (r'^LGA-14', 0, 'Pad-Vergleich LGA-14 (EasyEDA, Pin 1 oben links) - in der Vorschau pruefen'),
    (r'^JST_SH_SM', 0, 'Pad-Vergleich CONN-SMD_4P-P1.00_MEGASTAR_ZX-SH1.0-4PWT (C7430446)'),
]
POLAR = re.compile(r'^(U|Q|D|Y|J)\d|^C21$')
PIN1_NAME = {'D': 'Kathode', 'C': 'Pluspol', 'Y': 'Pin 1'}


def run(args):
    subprocess.run(args, check=True, capture_output=True)


def natkey(r):
    m = re.match(r'([A-Z]+)(\d+)', r)
    return (m.group(1), int(m.group(2))) if m else (r, 0)


def correction(fp_name):
    for pat, rot, src in CORRECTIONS:
        if re.search(pat, fp_name):
            return rot, src
    return 0, ''


def direction(dx, dy):
    """Richtung von der Bauteilmitte zu Pin 1 (dy nach unten positiv)."""
    if math.hypot(dx, dy) < 0.05:
        return 'Mitte'
    a = math.degrees(math.atan2(-dy, dx)) % 360
    names = ['rechts', 'rechts oben', 'oben', 'links oben', 'links', 'links unten', 'unten',
             'rechts unten']
    return names[int(((a + 22.5) % 360) // 45)]


def placements():
    """Bestueckdaten aus der Platine: Mitte der loetbaren Pads relativ zum Bohrursprung
    (Y nach oben), Seite, KiCad-Drehung, JLCPCB-Drehung, Lage von Pin 1."""
    import pcbnew
    b = pcbnew.LoadBoard(PCB)
    org = b.GetDesignSettings().GetAuxOrigin()
    rows = {}
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith(NOT_PLACED):
            continue
        bottom = fp.GetLayer() == pcbnew.B_Cu
        pads = [p for p in fp.Pads() if p.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH]
        bb = pads[0].GetBoundingBox()
        for p in pads[1:]:
            bb.Merge(p.GetBoundingBox())
        c = bb.GetCenter()
        x, y = pcbnew.ToMM(c.x - org.x), -pcbnew.ToMM(c.y - org.y)
        off = math.hypot(pcbnew.ToMM(c.x - fp.GetPosition().x), pcbnew.ToMM(c.y - fp.GetPosition().y))
        name = fp.GetFPID().GetLibItemName().wx_str()
        kirot = fp.GetOrientationDegrees() % 360
        # Unterseite: KiCad spiegelt die Bauteile, JLCPCB nicht (wie kicad-jlcpcb-tools)
        base = (180 - kirot) % 360 if bottom else kirot
        corr, src = correction(name)
        if off > 0.05:
            src = (src + '; ' if src else '') + ('Position = Mitte der Pads, %.2f mm neben dem '
                                                 'KiCad-Ursprung - Lage in der Vorschau pruefen' % off)
        jrot = (base + corr) % 360
        where = ''
        if POLAR.match(ref):
            want = 'A1' if ref == 'J1' else '1'
            p1 = [p for p in fp.Pads() if p.GetNumber() == want]
            if p1:
                dx = pcbnew.ToMM(p1[0].GetPosition().x - fp.GetPosition().x)
                dy = pcbnew.ToMM(p1[0].GetPosition().y - fp.GetPosition().y)
                if bottom:
                    dx = -dx      # Unterseite von unten gesehen
                where = '%s %s' % (PIN1_NAME.get(ref[0], 'Pin 1'), direction(dx, dy))
        rows[ref] = dict(ref=ref, x=x, y=y, side='Bottom' if bottom else 'Top', fp=name,
                         kirot=kirot, base=base, corr=corr, src=src, jrot=jrot, where=where,
                         off=off)
    return rows


def main():
    if os.path.isdir(GERBER):
        shutil.rmtree(GERBER)
    os.makedirs(GERBER)
    for old in ('fc_bom.csv', 'fc_cpl_top.csv', 'handbestueckung_unten.csv'):
        if os.path.exists(os.path.join(OUT, old)):
            os.remove(os.path.join(OUT, old))
    # Alle Ausgaben relativ zum Bohr-/Bestueckungsursprung (linke untere Platinenecke)
    run(['kicad-cli', 'pcb', 'export', 'gerbers', '--layers', LAYERS, '--subtract-soldermask',
         '--use-drill-file-origin', '-o', GERBER + '/', PCB])
    run(['kicad-cli', 'pcb', 'export', 'drill', '--format', 'excellon', '--excellon-units', 'mm',
         '--drill-origin', 'plot',
         '--excellon-separate-th', '--generate-map', '--map-format', 'gerberx2',
         '-o', GERBER + '/', PCB])
    zpath = os.path.join(OUT, 'fc_jlcpcb_gerber.zip')
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(GERBER)):
            if f.endswith('-drl_map.gbr'):
                continue    # Bohrplaene nur zur Ansicht, nicht fuer die Fertigung
            z.write(os.path.join(GERBER, f), f)

    parts = {p['ref']: p for p in design.PARTS}
    placed = [p for p in design.PARTS if not p['ref'].startswith(NOT_PLACED)]
    missing = [p['ref'] for p in placed if p['ref'] not in lcsc.REF]
    if missing:
        raise SystemExit('ohne LCSC-Nummer: ' + ', '.join(missing))
    rows = placements()
    if set(rows) != {p['ref'] for p in placed}:
        raise SystemExit('Platine und design.py passen nicht zusammen: %s'
                         % sorted(set(rows) ^ {p['ref'] for p in placed}))
    for r in rows.values():
        want = 'Bottom' if parts[r['ref']]['side'] == 'bottom' else 'Top'
        if r['side'] != want:
            raise SystemExit('%s liegt auf %s, design.py sagt %s' % (r['ref'], r['side'], want))

    # ---- Stueckliste (JLCPCB-Format)
    groups = {}
    for p in placed:
        groups.setdefault(lcsc.REF[p['ref']], []).append(p['ref'])
    with open(os.path.join(OUT, 'jlcpcb_bom.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Comment', 'Designator', 'Footprint', 'LCSC Part #'])
        for code, refs in sorted(groups.items(), key=lambda t: natkey(min(t[1], key=natkey))):
            refs.sort(key=natkey)
            p = parts[refs[0]]
            w.writerow([p['value'], ','.join(refs), p['fp'].split(':')[1], code])

    # ---- Bestueckkoordinaten beider Seiten
    order = sorted(rows.values(), key=lambda r: (r['side'] != 'Top', natkey(r['ref'])))
    with open(os.path.join(OUT, 'jlcpcb_cpl.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'])
        for r in order:
            w.writerow([r['ref'], '%.4fmm' % r['x'], '%.4fmm' % r['y'], r['side'], '%g' % r['jrot']])

    # ---- Pruefliste
    with open(os.path.join(OUT, 'jlcpcb_pruefliste.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Designator', 'Seite', 'Wert', 'LCSC', 'Hersteller-Nr.', 'Typ', 'Bestand',
                    'Bestand Stand/Quelle', 'Footprint', 'Drehung KiCad',
                    'Drehung ungespiegelt (Unterseite: 180 - KiCad)', 'Korrektur', 'Drehung CPL',
                    'Korrektur-Quelle',
                    'Pin 1 bzw. Polaritaet (Oberseite von oben mit USB-C rechts, '
                    'Unterseite von unten mit USB-C links)'])
        for r in order:
            ref = r['ref']
            code = lcsc.REF[ref]
            mpn, mfr, desc, typ, stock, stand = lcsc.PARTS[code]
            w.writerow([ref, 'oben' if r['side'] == 'Top' else 'unten', parts[ref]['value'], code,
                        mpn, typ, '' if stock is None else stock, stand, r['fp'],
                        '%g' % r['kirot'], '%g' % r['base'],
                        '%+d' % r['corr'] if r['corr'] else '0', '%g' % r['jrot'], r['src'],
                        r['where']])

    ext = sorted({c for c in groups if lcsc.PARTS[c][3] == 'Extended'})
    pref = sorted({c for c in groups if lcsc.PARTS[c][3] == 'Preferred'})
    top = [r for r in rows.values() if r['side'] == 'Top']
    bot = [r for r in rows.values() if r['side'] == 'Bottom']
    print('bestueckt: %d oben, %d unten; %d Teilenummern (%d Extended, %d Preferred)'
          % (len(top), len(bot), len(groups), len(ext), len(pref)))
    print('Extended:', ', '.join('%s %s' % (c, lcsc.PARTS[c][0]) for c in ext))
    print('Unterseite:', ', '.join(r['ref'] for r in sorted(bot, key=lambda r: natkey(r['ref']))))
    big = ['%s %.2f' % (r['ref'], r['off']) for r in rows.values() if r['off'] > 0.05]
    print('Padmitte != Footprint-Ursprung (mm):', ', '.join(big) or '-')
    print('ZIP:', zpath)


if __name__ == '__main__':
    main()
