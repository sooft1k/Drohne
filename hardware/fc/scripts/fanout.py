# Automatisches Fanout: jedes SMD-Pad eines Flaechennetzes bekommt ein kurzes
# Leiterbahnstueck und ein Via in die passende Innenlagen-Flaeche.
#   GND  -> Lage 2 (ueberall)
#   +3V3 / +5V / VBAT -> Lage 3, nur wenn das Via im richtigen Bereich liegt
# Pads, fuer die kein regelkonformer Platz gefunden wird, bleiben fuer den
# Autorouter (Leiterbahn zum naechsten Punkt desselben Netzes).
import math
import pcbnew
from shapely.geometry import Point, LineString, Polygon
import geom
import zones

VIA_R = 0.3
CLEAR = {'GND': 0.2, '+3V3': 0.2, '+5V': 0.22, 'VBAT': 0.22}
WIDTH = {'GND': 0.3, '+3V3': 0.3, '+5V': 0.4, 'VBAT': 0.4}
EDGE = 0.5
W2, H2 = 21.0, 20.5


def region(net):
    p = zones.polys().get(net)
    return Polygon(p) if p else None


BOARD = Polygon([(-W2, -H2), (W2, -H2), (W2, H2), (-W2, H2)])
HOLES = [(15.25, 15.25), (15.25, -15.25), (-15.25, -15.25), (-15.25, 15.25)]


def via_ok(obs, c, net, extra_vias=()):
    p = Point(c)
    if not BOARD.buffer(-(EDGE + VIA_R)).contains(p):
        return False
    for h in HOLES:
        if math.hypot(c[0] - h[0], c[1] - h[1]) < 3.3 + VIA_R + 0.05:
            return False
    for poly in obs.no_via:
        if poly.buffer(VIA_R).contains(p):
            return False
    reg = region(net)
    if reg is not None and not reg.buffer(-0.6).contains(p):
        return False
    shape = p.buffer(VIA_R)
    # kein Via in einem SMD-Pad (auch nicht im eigenen Netz)
    for poly, n, ref, num, side in obs.smd_pads:
        if poly.distance(p) < VIA_R + 0.08:
            return False
    # Abstand Bohrung zu Bohrung
    for (vc, vn, r) in obs.vias:
        if math.hypot(vc[0] - c[0], vc[1] - c[1]) < 0.3 / 2 + r + 0.25 + (0 if vn == net else 0.15):
            return False
    return not obs.conflicts(shape, net, geom.ALL, CLEAR.get(net, 0.15))


def stub_ok(obs, a, c, net, layer_key):
    w = WIDTH.get(net, 0.3)
    if a == c:
        return True
    line = LineString([a, c]).buffer(w / 2)
    if obs.conflicts(line, net, [layer_key], CLEAR.get(net, 0.15)):
        return False
    for poly in obs.no_copper.get(layer_key, []):
        if poly.intersects(line):
            return False
    return True


def candidates(pad_c, pad_poly, fp_c, inward=False):
    """Kandidaten fuer Via-Positionen, bevorzugt vom Bauteil weg (bzw. nach innen)."""
    dx, dy = pad_c[0] - fp_c[0], pad_c[1] - fp_c[1]
    base = math.atan2(dy, dx) if (dx or dy) else 0.0
    if inward:
        base += math.pi
    out = []
    minx, miny, maxx, maxy = pad_poly.bounds
    half = max(maxx - minx, maxy - miny) / 2
    for k, dang in enumerate([0, 45, -45, 90, -90, 135, -135, 180]):
        ang = base + math.radians(dang)
        for s in [x / 20.0 for x in range(6, 45)]:
            c = (pad_c[0] + math.cos(ang) * (s + VIA_R), pad_c[1] + math.sin(ang) * (s + VIA_R))
            if pad_poly.distance(Point(c)) < VIA_R + 0.08:
                continue
            out.append((k * 10 + s, c))
    out.sort(key=lambda t: t[0])
    return [c for _, c in out]


def fanout(bd, skip=(), inward_refs=('U1',), log=None):
    b = bd.board
    obs = geom.Obstacles(b)
    done, failed = [], []
    # Pads, die schon mit Vorverdrahtung verbunden sind, ueberspringen
    touched = set()
    for t in b.GetTracks():
        if t.Type() == pcbnew.PCB_VIA_T:
            continue
        for fp in b.GetFootprints():
            for pad in fp.Pads():
                if pad.GetNetname() == t.GetNetname() and pad.GetNetname():
                    if pad.HitTest(t.GetStart()) or pad.HitTest(t.GetEnd()):
                        touched.add((fp.GetReference(), pad.GetNumber()))
    for fp in sorted(b.GetFootprints(), key=lambda f: f.GetReference()):
        ref = fp.GetReference()
        fpc = geom.to_uv(fp.GetPosition())
        for pad in fp.Pads():
            net = pad.GetNetname()
            if net not in CLEAR:
                continue
            if pad.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
                continue
            key = (ref, pad.GetNumber())
            if key in skip or key in touched:
                continue
            side = pcbnew.F_Cu if pad.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu
            lk = geom.CU[side]
            poly = geom.pad_polygon(pad, side)
            pc = geom.to_uv(pad.GetPosition())
            placed = False
            for c in candidates(pc, poly, fpc, inward=ref in inward_refs):
                if not via_ok(obs, c, net):
                    continue
                # Leiterbahn vom Padrand aus pruefen (das Pad selbst gehoert zum Netz)
                if not stub_ok(obs, pc, c, net, lk):
                    continue
                for t in bd.track(net, [pc, c], WIDTH[net], layer=side):
                    obs.add_track(t)
                obs.add_track(bd.via(net, c[0], c[1]))
                done.append((ref, pad.GetNumber(), net))
                placed = True
                break
            if not placed:
                failed.append((ref, pad.GetNumber(), net))
    if log is not None:
        log['fanout_ok'] = done
        log['fanout_failed'] = failed
    return done, failed


def thermal_vias(bd, ref, num, net, pitch=1.0):
    """Waermevias in einem Exposed Pad, nur wo unten kein fremdes Kupfer liegt."""
    b = bd.board
    obs = geom.Obstacles(b)
    pads = [p for p in bd.fps[ref].Pads() if p.GetNumber() == num]
    poly = geom.pad_polygon(pads[0], pcbnew.F_Cu)
    minx, miny, maxx, maxy = poly.bounds
    n = 0
    y = miny + 0.55
    while y <= maxy - 0.55 + 1e-6:
        x = minx + 0.55
        while x <= maxx - 0.55 + 1e-6:
            p = Point(x, y)
            ok = True
            for shp, nn, kind, r in obs.items['B'] + obs.items['I1'] + obs.items['I2']:
                if nn != net and shp.distance(p) < VIA_R + 0.2:
                    ok = False
                    break
            for (vc, vn, r) in obs.vias:
                if math.hypot(vc[0] - x, vc[1] - y) < 0.8:
                    ok = False
            if ok:
                obs.add_track(bd.via(net, x, y))
                n += 1
            x += pitch
        y += pitch * 0.9
    return n
