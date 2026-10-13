# DRC ueber die KiCad-Python-API (kicad-cli 7 hat noch kein "pcb drc").
import os, sys, re, collections
# Bibliothekspfade fuer den Abgleich Platine <-> Footprint-Bibliothek
os.environ.setdefault('KICAD7_FOOTPRINT_DIR', '/usr/share/kicad/footprints')
os.environ.setdefault('KICAD7_SYMBOL_DIR', '/usr/share/kicad/symbols')
import pcbnew
pcb = sys.argv[1]
rpt = sys.argv[2]
b = pcbnew.LoadBoard(pcb)
if '--fill' in sys.argv:
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.WriteDRCReport(b, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
txt = open(rpt).read()
cnt = collections.Counter(re.findall(r'^\[([a-z_]+)\]', txt, re.M))
print(dict(cnt))
m = re.search(r'\*\* Found (\d+) DRC violations \*\*', txt); print('violations', m.group(1) if m else '?')
m = re.search(r'\*\* Found (\d+) unconnected pads \*\*', txt); print('unconnected', m.group(1) if m else '?')
