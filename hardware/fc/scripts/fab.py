# Fertigungsdaten fuer JLCPCB: Gerber (4 Lagen), Bohrdateien, ZIP,
# dazu Bestueckungsliste (BOM) und Bestueckkoordinaten (CPL) fuer die Oberseite.
import csv
import os
import shutil
import subprocess
import sys
import zipfile
sys.path.insert(0, os.path.dirname(__file__))
import design

PRJ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(PRJ, 'fertigung')
GERBER = os.path.join(OUT, 'gerber')
PCB = os.path.join(PRJ, 'fc.kicad_pcb')
LAYERS = 'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts'


def run(args):
    subprocess.run(args, check=True, capture_output=True)


def main():
    if os.path.isdir(GERBER):
        shutil.rmtree(GERBER)
    os.makedirs(GERBER)
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
    # Bestueckkoordinaten (nur Oberseite; Steckverbinder unten werden von Hand geloetet),
    # umgeschrieben ins JLCPCB-CPL-Format
    raw = os.path.join(OUT, 'pos_raw.csv')
    run(['kicad-cli', 'pcb', 'export', 'pos', '--side', 'front', '--format', 'csv', '--units', 'mm',
         '--use-drill-file-origin', '-o', raw, PCB])
    pos = os.path.join(OUT, 'fc_cpl_top.csv')
    with open(raw, newline='') as fi, open(pos, 'w', newline='') as fo:
        w = csv.writer(fo)
        w.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'])
        n = 0
        for r in csv.DictReader(fi):
            if r['Ref'].startswith(('H', 'FID', 'TP', 'JP')):
                continue
            w.writerow([r['Ref'], r['PosX'] + 'mm', r['PosY'] + 'mm', 'Top', r['Rot']])
            n += 1
    os.remove(raw)
    print('CPL:', n, 'Bauteile oben')
    # Stueckliste aus design.py (gruppiert nach Wert + Footprint)
    groups = {}
    for p in design.PARTS:
        if p['ref'].startswith(('H', 'FID', 'TP', 'JP')):
            continue
        key = (p['value'], p['fp'].split(':')[1], p['mpn'])
        groups.setdefault(key, []).append(p['ref'])
    bom = os.path.join(OUT, 'fc_bom.csv')
    with open(bom, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['Comment', 'Designator', 'Footprint', 'Hersteller-Nr.', 'Anzahl', 'Bestueckung'])
        for (val, fp, mpn), refs in sorted(groups.items(), key=lambda t: natkey(t[1][0])):
            side = 'unten, von Hand' if any(design_part(r)['side'] == 'bottom' for r in refs) \
                else 'oben (JLCPCB)'
            w.writerow([val, ','.join(sorted(refs, key=natkey)), fp, mpn, len(refs), side])
    print('Gerber:', sorted(os.listdir(GERBER)))
    print('ZIP:', zpath)


def design_part(ref):
    return next(p for p in design.PARTS if p['ref'] == ref)


def natkey(r):
    import re
    m = re.match(r'([A-Z]+)(\d+)', r)
    return (m.group(1), int(m.group(2))) if m else (r, 0)


if __name__ == '__main__':
    main()
