# Fertigungsdaten fuer JLCPCB:
#   fertigung/fc_jlcpcb_gerber.zip        Gerber (4 Lagen) + Excellon-Bohrdateien
#   fertigung/jlcpcb_bom.csv              Stueckliste nur Oberseite (maschinelle Bestueckung)
#   fertigung/jlcpcb_cpl.csv              Bestueckkoordinaten Oberseite, Drehung fuer JLCPCB korrigiert
#   fertigung/jlcpcb_pruefliste.csv       je Bauteil: LCSC-Typ, Bestand, Drehung, Lage von Pin 1
#   fertigung/handbestueckung_unten.csv   Steckverbinder der Unterseite (selbst loeten)
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

# Drehkorrektur KiCad -> JLCPCB (wird zur KiCad-Drehung addiert).
# Quelle der Tabellenwerte: JLCKicadTools, cpl_rotations_db.csv (Erfahrungswerte der Community
# mit den JLCPCB/LCSC-Footprints). Ohne Tabelleneintrag: keine Korrektur, in der Vorschau pruefen.
CORRECTIONS = [
    (r'^LQFP-', 270, 'Tabelle JLCKicadTools'),
    (r'^SOIC-', 270, 'Tabelle JLCKicadTools'),
    (r'^SOT-223', 180, 'Tabelle JLCKicadTools'),
    (r'^SOT-23', -90, 'Tabelle JLCKicadTools (^SOT-23, gilt auch fuer SOT-23-6)'),
    (r'^CP_Elec_10x10', 180, 'Tabelle JLCKicadTools'),
    (r'^USB_C_Receptacle_HRO_TYPE-C-31-M-12', 180, 'Tabelle JLCKicadTools'),
    (r'^Texas_HSOP-8', 0, 'KEIN Tabelleneintrag - in der JLCPCB-Vorschau pruefen'),
    (r'^LGA-14', 0, 'KEIN Tabelleneintrag - in der JLCPCB-Vorschau pruefen'),
]
POLAR = re.compile(r'^(U|Q|D|Y|J)\d|^C21$')
PIN1_NAME = {'D': 'Kathode', 'C': 'Pluspol', 'J': 'Pin A1', 'Y': 'Pin 1'}


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
    """Richtung von der Bauteilmitte zu Pin 1, Platine von oben (USB-C rechts, ESC-Stecker unten)."""
    if math.hypot(dx, dy) < 0.05:
        return 'Mitte'
    a = math.degrees(math.atan2(-dy, dx)) % 360
    names = ['rechts', 'rechts oben', 'oben', 'links oben', 'links', 'links unten', 'unten',
             'rechts unten']
    return names[int(((a + 22.5) % 360) // 45)]


def pin1_positions():
    import pcbnew
    b = pcbnew.LoadBoard(PCB)
    out = {}
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        if not POLAR.match(ref):
            continue
        want = 'A1' if ref.startswith('J') else '1'
        pads = [p for p in fp.Pads() if p.GetNumber() == want]
        if not pads:
            continue
        c, p = fp.GetPosition(), pads[0].GetPosition()
        out[ref] = direction(pcbnew.ToMM(p.x - c.x), pcbnew.ToMM(p.y - c.y))
    return out


def main():
    if os.path.isdir(GERBER):
        shutil.rmtree(GERBER)
    os.makedirs(GERBER)
    for old in ('fc_bom.csv', 'fc_cpl_top.csv'):
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
    top = [p for p in design.PARTS if p['side'] == 'top' and not p['ref'].startswith(NOT_PLACED)]
    bottom = [p for p in design.PARTS if p['side'] == 'bottom' and not p['ref'].startswith(NOT_PLACED)]
    missing = [p['ref'] for p in top if p['ref'] not in lcsc.REF]
    if missing:
        raise SystemExit('ohne LCSC-Nummer: ' + ', '.join(missing))

    # ---- Stueckliste Oberseite (JLCPCB-Format)
    groups = {}
    for p in top:
        groups.setdefault(lcsc.REF[p['ref']], []).append(p['ref'])
    with open(os.path.join(OUT, 'jlcpcb_bom.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Comment', 'Designator', 'Footprint', 'LCSC Part #'])
        for code, refs in sorted(groups.items(), key=lambda t: natkey(min(t[1], key=natkey))):
            refs.sort(key=natkey)
            p = parts[refs[0]]
            w.writerow([p['value'], ','.join(refs), p['fp'].split(':')[1], code])

    # ---- Bestueckkoordinaten Oberseite mit Drehkorrektur
    raw = os.path.join(OUT, 'pos_raw.csv')
    run(['kicad-cli', 'pcb', 'export', 'pos', '--side', 'front', '--format', 'csv', '--units', 'mm',
         '--use-drill-file-origin', '-o', raw, PCB])
    pin1 = pin1_positions()
    rows = []
    with open(raw, newline='') as fi:
        for r in csv.DictReader(fi):
            if r['Ref'].startswith(NOT_PLACED):
                continue
            corr, src = correction(r['Package'])
            rot = (float(r['Rot']) + corr) % 360
            rows.append((r, rot, corr, src))
    os.remove(raw)
    rows.sort(key=lambda t: natkey(t[0]['Ref']))
    with open(os.path.join(OUT, 'jlcpcb_cpl.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'])
        for r, rot, corr, src in rows:
            w.writerow([r['Ref'], '%.4fmm' % float(r['PosX']), '%.4fmm' % float(r['PosY']), 'Top',
                        '%g' % rot])

    # ---- Pruefliste
    with open(os.path.join(OUT, 'jlcpcb_pruefliste.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Designator', 'Wert', 'LCSC', 'Hersteller-Nr.', 'Typ', 'Bestand', 'Bestand Stand/Quelle',
                    'Footprint', 'Drehung KiCad', 'Korrektur', 'Drehung CPL', 'Korrektur-Quelle',
                    'Pin 1 bzw. Polaritaet auf der Platine (von oben, USB-C rechts)'])
        for r, rot, corr, src in rows:
            ref = r['Ref']
            code = lcsc.REF[ref]
            mpn, mfr, desc, typ, stock, stand = lcsc.PARTS[code]
            where = ''
            if ref in pin1:
                where = '%s %s' % (PIN1_NAME.get(ref[0], 'Pin 1'), pin1[ref])
            w.writerow([ref, parts[ref]['value'], code, mpn, typ, '' if stock is None else stock, stand,
                        r['Package'], '%g' % float(r['Rot']), '%+d' % corr if corr else '0', '%g' % rot,
                        src, where])

    # ---- Handbestueckung Unterseite
    with open(os.path.join(OUT, 'handbestueckung_unten.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Designator', 'Funktion', 'Footprint', 'Bauteilvorschlag'])
        hand = {}
        for refs, txt in lcsc.HAND.items():
            for r in refs.split():
                hand[r] = txt
        for p in sorted(bottom, key=lambda p: natkey(p['ref'])):
            w.writerow([p['ref'], p['value'], p['fp'].split(':')[1], hand.get(p['ref'], '')])

    ext = sorted({c for c in groups if lcsc.PARTS[c][3] == 'Extended'})
    pref = sorted({c for c in groups if lcsc.PARTS[c][3] == 'Preferred'})
    print('Oberseite: %d Bauteile, %d Teilenummern (%d Extended, %d Preferred)'
          % (len(top), len(groups), len(ext), len(pref)))
    print('Extended:', ', '.join('%s %s' % (c, lcsc.PARTS[c][0]) for c in ext))
    print('Unterseite (Hand):', ', '.join(p['ref'] for p in sorted(bottom, key=lambda p: natkey(p['ref']))))
    print('ZIP:', zpath)


if __name__ == '__main__':
    main()
