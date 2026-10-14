# Kennzahlen der gerouteten Platine fuer die Dokumentation.
import math, sys, collections
import pcbnew
b = pcbnew.LoadBoard(sys.argv[1])
OX = OY = 100.0
def uv(p): return (pcbnew.ToMM(p.x) - OX, pcbnew.ToMM(p.y) - OY)
length = collections.defaultdict(float)
vias = collections.Counter()
widths = collections.defaultdict(set)
for t in b.GetTracks():
    n = t.GetNetname()
    if t.Type() == pcbnew.PCB_VIA_T:
        vias[n] += 1
    else:
        length[n] += pcbnew.ToMM(t.GetLength())
        widths[n].add(round(pcbnew.ToMM(t.GetWidth()), 2))
for n in ['USB_DP', 'USB_DM', 'SPI1_SCK', 'SPI1_MISO', 'SPI1_MOSI', 'GYRO_CS', 'GYRO_INT1',
          'M1', 'M2', 'M3', 'M4', 'CURR_IN', 'CURR_SENSE', 'OSC_IN', 'OSC_OUT', 'XTAL_OUT', 'VBUS',
          '+4V5', '+5V', '+3V3', 'GND', 'VBAT', 'VBAT_IN']:
    print('%-10s Laenge %6.1f mm  Vias %3d  Breiten %s' % (n, length[n], vias[n], sorted(widths[n])))
# Vias unter dem Gyro?
g = b.FindFootprintByReference('U2')
bb = g.GetBoundingBox(False, False)
inside = [t for t in b.GetTracks() if t.Type() == pcbnew.PCB_VIA_T and bb.Contains(t.GetPosition())]
print('Vias unter dem Gyro:', len(inside))
# minimale Leiterbahnbreite
mw = min(pcbnew.ToMM(t.GetWidth()) for t in b.GetTracks() if t.Type() != pcbnew.PCB_VIA_T)
print('kleinste Leiterbahnbreite: %.3f mm' % mw)
print('Vias gesamt:', sum(vias.values()), ' Leiterbahnsegmente:', sum(1 for t in b.GetTracks() if t.Type() != pcbnew.PCB_VIA_T))
# Leiterbahnen auf Innenlagen?
inner = [t for t in b.GetTracks() if t.Type() != pcbnew.PCB_VIA_T and t.GetLayer() in (pcbnew.In1_Cu, pcbnew.In2_Cu)]
print('Leiterbahnen auf Lage 2/3:', len(inner))
# Motorleitungen: kleinster Abstand zum Gyro (Gehaeusemitte)
gc = uv(g.GetPosition())
md = min(
    min(math.hypot(a[0] - gc[0], a[1] - gc[1]) for a in (uv(t.GetStart()), uv(t.GetEnd())))
    for t in b.GetTracks() if t.GetNetname() in ('M1', 'M2', 'M3', 'M4') and t.Type() != pcbnew.PCB_VIA_T)
print('Gyro-Mitte (u, v): (%.2f, %.2f) mm; kleinster Abstand Motorleitung-Endpunkt -> Gyro: %.1f mm'
      % (gc[0], gc[1], md))
# Bahnen schmaler als die Netzklassen-Breite
sys.path.insert(0, __import__('os').path.dirname(__file__))
from gen_pcb import NETCLASSES
cls_w = {n: c[0] for c in NETCLASSES.values() for n in c[4]}
narrow = collections.defaultdict(list)
for t in b.GetTracks():
    n = t.GetNetname()
    if t.Type() != pcbnew.PCB_VIA_T and n in cls_w and pcbnew.ToMM(t.GetWidth()) < cls_w[n] - 1e-6:
        narrow[n].append(round(pcbnew.ToMM(t.GetWidth()), 2))
for n, ws in sorted(narrow.items()):
    print('schmaler als Netzklasse (%s, Soll %.2f mm): %d Stuecke, Breiten %s'
          % (n, cls_w[n], len(ws), sorted(set(ws))))
# Abgleich Platine <-> Schaltplan-Netzliste (kicad-cli sch export netlist)
if len(sys.argv) > 2:
    from sexp import parse, find, find1
    t = parse(open(sys.argv[2]).read())[0]
    want = {}
    for net in find(find1(t, 'nets'), 'net'):
        name = find1(net, 'name')[1]
        for node in find(net, 'node'):
            want[(find1(node, 'ref')[1], find1(node, 'pin')[1])] = name
    bad = 0
    seen = set()
    for fp in b.GetFootprints():
        for pad in fp.Pads():
            key = (fp.GetReference(), pad.GetNumber())
            if not pad.GetNumber() or key in seen:
                continue
            seen.add(key)
            w = want.get(key, '')
            have = pad.GetNetname()
            norm = lambda s: '' if s.startswith('unconnected') or s.startswith('Net-(') and False else s
            if (w.startswith('unconnected') or w == '') and (have == '' or have.startswith('unconnected')):
                continue
            if w.lstrip('/') != have:
                bad += 1
                print('  Abweichung', key, 'Schaltplan', w, 'Platine', have)
    print('Abgleich Platine/Schaltplan: %d Pads, %d Abweichungen' % (len(seen), bad))
