# Uebertraegt Werte (design.py) und LCSC-Nummern (lcsc.py) in die fertig geroutete Platine,
# ohne Platzierung oder Kupfer zu veraendern (entspricht "Platine aus Schaltplan aktualisieren"
# nur fuer die Felder).
import os
import sys
sys.path.insert(0, os.path.dirname(__file__))
os.environ.setdefault('KICAD7_FOOTPRINT_DIR', '/usr/share/kicad/footprints')
import pcbnew
import design
import lcsc

PCB = os.path.join(os.path.dirname(__file__), '..', 'fc.kicad_pcb')


def main():
    b = pcbnew.LoadBoard(PCB)
    parts = {p['ref']: p for p in design.PARTS}
    changed = []
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        p = parts.get(ref)
        if p is None:
            raise SystemExit('Bauteil nicht in design.py: ' + ref)
        if fp.GetValue() != p['value']:
            changed.append('%s: %s -> %s' % (ref, fp.GetValue(), p['value']))
            fp.SetValue(p['value'])
        want = lcsc.REF.get(ref)
        have = fp.GetProperty('LCSC') if fp.HasProperty('LCSC') else None
        if want and have != want:
            fp.SetProperty('LCSC', want)
            changed.append('%s: LCSC %s' % (ref, want))
    pcbnew.SaveBoard(PCB, b)
    print('%d Aenderungen' % len(changed))
    for c in changed:
        if 'LCSC' not in c:
            print('  ' + c)


if __name__ == '__main__':
    main()
