# Aussenkontur fuer den Rahmen-CAD: Platinenumriss, alles was ueber/nahe an die Kante reicht,
# Buchsen mit Kante, Lage und Abstand. Koordinaten: Ursprung linke untere Ecke von OBEN gesehen
# (USB-C rechts = Ost, ESC-Stecker unten = Sued), X nach Osten, Y nach Norden, mm.
# Aufruf: python3 scripts/kontur.py [fc.kicad_pcb] > docs/fc_aussenkontur.txt
import os
import sys
import pcbnew
pcb = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'fc.kicad_pcb')
b = pcbnew.LoadBoard(pcb)
ep = []   # Mittellinie der Umrisslinien (ohne Strichbreite)
for d in b.GetDrawings():
    if d.GetLayer() == pcbnew.Edge_Cuts:
        ep += [d.GetStart(), d.GetEnd()]
        if d.GetShape() == pcbnew.SHAPE_T_ARC:
            ep.append(d.GetArcMid())
X0 = pcbnew.ToMM(min(p.x for p in ep)); X1 = pcbnew.ToMM(max(p.x for p in ep))
Y0 = pcbnew.ToMM(min(p.y for p in ep)); Y1 = pcbnew.ToMM(max(p.y for p in ep))
W, H = X1 - X0, Y1 - Y0
def cad(x, y):   # KiCad (mm, y nach unten) -> CAD (Ursprung SW-Ecke, y nach oben)
    return x - X0, Y1 - y
def bbox_of(points):
    xs = [p[0] for p in points]; ys = [p[1] for p in points]
    return min(xs), min(ys), max(xs), max(ys)
def shape_pts(g):
    if g.GetShape() == pcbnew.SHAPE_T_CIRCLE:
        c = g.GetCenter(); r = g.GetRadius()
        return [(c.x - r, c.y - r), (c.x + r, c.y + r)]
    if g.GetShape() == pcbnew.SHAPE_T_ARC:
        return [(g.GetStart().x, g.GetStart().y), (g.GetEnd().x, g.GetEnd().y), (g.GetArcMid().x, g.GetArcMid().y)]
    return [(g.GetStart().x, g.GetStart().y), (g.GetEnd().x, g.GetEnd().y)]
rows = []
for fp in b.GetFootprints():
    ref = fp.GetReference()
    if ref.startswith(('H', 'FID', 'TP', 'JP')):
        continue
    bottom = fp.GetLayer() == pcbnew.B_Cu
    fab = pcbnew.B_Fab if bottom else pcbnew.F_Fab
    pts = []
    for g in fp.GraphicalItems():
        if g.GetClass() == 'MGRAPHIC' and g.GetLayer() == fab:
            pts += shape_pts(g)
    ppts = []
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        ppts += [(bb.GetLeft(), bb.GetTop()), (bb.GetRight(), bb.GetBottom())]
    pl, pt, pr, pb = [pcbnew.ToMM(v) for v in bbox_of(ppts)]
    l, t, r, btm = bbox_of(pts + ppts)
    l, t, r, btm = [pcbnew.ToMM(v) for v in (l, t, r, btm)]
    # Abstand Koerper/Pads zu jeder Kante (positiv = innen, negativ = Ueberstand)
    d = {'West': l - X0, 'Ost': X1 - r, 'Sued': Y1 - btm, 'Nord': t - Y0}
    edge, dist = min(d.items(), key=lambda kv: kv[1])
    # Steckrichtung bei Buchsen: Mitte Signalpads -> Mitte Montagepads (MP) bzw. USB-Oeffnung
    opening = ''
    sig = [p for p in fp.Pads() if p.GetNumber() not in ('MP', '', 'S1')]
    mp = [p for p in fp.Pads() if p.GetNumber() == 'MP']
    if mp and sig:
        sx = sum(p.GetPosition().x for p in sig) / len(sig); sy = sum(p.GetPosition().y for p in sig) / len(sig)
        mx = sum(p.GetPosition().x for p in mp) / len(mp); my = sum(p.GetPosition().y for p in mp) / len(mp)
        dx, dy = mx - sx, my - sy
        opening = ('Ost' if dx > 0 else 'West') if abs(dx) > abs(dy) else ('Sued' if dy > 0 else 'Nord')
    fl, ft, fr, fb = [pcbnew.ToMM(v) for v in bbox_of(pts)] if pts else (l, t, r, btm)
    rows.append(dict(ref=ref, val=fp.GetValue(), side='unten' if bottom else 'oben', l=l, t=t, r=r, b=btm,
                     fab=(fl, ft, fr, fb), pad=(pl, pt, pr, pb),
                     edge=edge, dist=dist, opening=opening))
print('Platine (Edge.Cuts): %.2f x %.2f mm, Ecken gerundet' % (W, H))
print('Koordinaten: Ursprung linke untere Ecke von OBEN gesehen (USB-C rechts/Ost, ESC-Stecker unten/Sued), X nach Ost, Y nach Nord')
print()
print('Teile bis 3 mm an einer Kante (Koerper laut Fertigungslage + Pads):')
for e in ('West', 'Ost', 'Sued', 'Nord'):
    print('== Kante %s (%s)' % (e, {'West': 'X = 0', 'Ost': 'X = %.2f' % W, 'Sued': 'Y = 0', 'Nord': 'Y = %.2f' % H}[e]))
    for r in sorted(rows, key=lambda r: r['ref']):
        d = {'West': r['l'] - X0, 'Ost': X1 - r['r'], 'Sued': Y1 - r['b'], 'Nord': r['t'] - Y0}[e]
        if d > 3.0:
            continue
        fl, ft, fr, fb = r['fab']; pl, pt, pr, pb = r['pad']
        df = {'West': fl - X0, 'Ost': X1 - fr, 'Sued': Y1 - fb, 'Nord': ft - Y0}[e]
        dp = {'West': pl - X0, 'Ost': X1 - pr, 'Sued': Y1 - pb, 'Nord': pt - Y0}[e]
        x0, y1 = cad(fl, ft); x1, y0 = cad(fr, fb)
        along = ('Y %5.2f .. %5.2f' % (y0, y1)) if e in ('West', 'Ost') else ('X %5.2f .. %5.2f' % (x0, x1))
        print('   %-4s %-10s %-5s Koerper %s  Koerper->Kante %+.2f  Kupfer->Kante %+.2f%s%s' % (
            r['ref'], r['val'][:10], r['side'], along, df, dp, '  UEBERSTAND' if df < 0 else '',
            ('  Oeffnung nach ' + r['opening']) if r['opening'] else ''))
print()
print('Buchsen innen (> 3 mm vom Rand):')
for r in sorted(rows, key=lambda r: r['ref']):
    if r['opening'] and r['dist'] > 3.0:
        x0, y1 = cad(r['fab'][0], r['fab'][1]); x1, y0 = cad(r['fab'][2], r['fab'][3])
        print('   %-4s %-8s %-5s Koerper X %.2f..%.2f, Y %.2f..%.2f, Oeffnung nach %s' % (r['ref'], r['val'], r['side'], x0, x1, y0, y1, r['opening']))
print()
print('Bohrungen:')
for fp in b.GetFootprints():
    if fp.GetReference().startswith('H'):
        p = fp.GetPosition(); x, y = cad(pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
        print('   %s  X %.2f  Y %.2f  (%s)' % (fp.GetReference(), x, y, fp.GetValue()))
