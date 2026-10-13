# Nachrouter: schliesst Verbindungen, die der Autorouter offen gelassen hat.
# A*-Suche auf einem 0,1-mm-Raster ueber F.Cu und B.Cu mit Vias, Abstaende
# gegen alle fremden Kupferobjekte, Bohrungen, Sperrflaechen und den Platinenrand.
# Flaechen (Zonen) werden ignoriert, sie werden danach neu gefuellt.
#
# Findet sich kein freier Weg, darf die Suche Autorouter-Bahnen anderer Netze gegen
# Strafkosten durchqueren (Rip-up). Diese Stuecke werden entfernt und die betroffenen
# Netze anschliessend neu verbunden. Handverdrahtung (gesperrte Bahnen) und Pads sind
# immer harte Hindernisse.
import heapq
import math
import os
import sys
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import pcbnew
import shapely
from shapely.geometry import Point, LineString, Polygon
from shapely.ops import unary_union
import geom
from gen_pcb import NETCLASSES

CLEAR_OF = {n: c[1] for c in NETCLASSES.values() for n in c[4]}

STEP = 0.1
MARGIN = 0.03        # Sicherheitszuschlag auf die Abstandsregel (Rasterfehler)
HOLE_CLEAR = 0.2     # Kupfer-Bohrung
VIA_R = 0.3
VIA_COST = 25.0
RIPUP_COST = 120.0   # Zusatzkosten je Rasterschritt durch fremde Autorouter-Bahnen
W2, H2 = 21.0, 20.5
EDGE = 0.55
LAYERS = {'F': pcbnew.F_Cu, 'B': pcbnew.B_Cu}


def clear(a, b):
    """Abstandsregel zwischen zwei Netzen (groesserer Wert der beiden Netzklassen)."""
    return max(CLEAR_OF.get(a, 0.15), CLEAR_OF.get(b, 0.15))


def track_shape(t):
    if t.Type() == pcbnew.PCB_VIA_T:
        return Point(geom.to_uv(t.GetPosition())).buffer(pcbnew.ToMM(t.GetWidth()) / 2)
    return LineString([geom.to_uv(t.GetStart()), geom.to_uv(t.GetEnd())]).buffer(
        pcbnew.ToMM(t.GetWidth()) / 2)


def net_items(b, net):
    """Kupferobjekte eines Netzes: Liste (shape, layer, kind, obj)."""
    out = []
    for fp in b.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetname() != net:
                continue
            for key, lay in LAYERS.items():
                if pad.IsOnLayer(lay):
                    out.append((geom.pad_polygon(pad, lay), key, 'pad', (fp.GetReference(), pad.GetNumber())))
    for t in b.GetTracks():
        if t.GetNetname() != net:
            continue
        if t.Type() == pcbnew.PCB_VIA_T:
            for key in LAYERS:
                out.append((track_shape(t), key, 'via', t))
        else:
            key = geom.CU.get(t.GetLayer())
            if key in LAYERS:
                out.append((track_shape(t), key, 'track', t))
    return out


def clusters(items):
    n = len(items)
    parent = list(range(n))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i in range(n):
        for j in range(i):
            si, li, ki, oi = items[i]
            sj, lj, kj, oj = items[j]
            # Vias und THT-Pads verbinden Lagen; sonst nur auf derselben Lage
            same_obj = (oi is oj) or (ki == kj == 'pad' and oi == oj)
            if same_obj or (li == lj and si.intersects(sj)):
                parent[find(i)] = find(j)
    groups = {}
    for i in range(n):
        groups.setdefault(find(i), []).append(i)
    return list(groups.values())


class Grid:
    """Belegungsraster fuer ein Netz: harte und weiche (aufreissbare) Hindernisse je Lage."""

    def __init__(self, b, net, width):
        self.b = b
        self.net = net
        self.w = width
        self.nx = int(2 * W2 / STEP) + 1
        self.ny = int(2 * H2 / STEP) + 1
        xs = -W2 + np.arange(self.nx) * STEP
        ys = -H2 + np.arange(self.ny) * STEP
        self.X, self.Y = np.meshgrid(xs, ys, indexing='ij')
        obs = geom.Obstacles(b)
        board = Polygon([(-W2, -H2), (W2, -H2), (W2, H2), (-W2, H2)]).buffer(-(EDGE + width / 2))
        inside = shapely.contains_xy(board, self.X, self.Y)
        self.hard, self.soft = {}, {}
        for key in LAYERS:
            self.hard[key] = ~inside
            self.soft[key] = np.zeros_like(inside)
            for poly in obs.no_copper.get(key, []):
                self.hard[key] |= self._raster(poly.buffer(width / 2))
        vb_hard = ~shapely.contains_xy(board.buffer(-(VIA_R - width / 2) - 0.3), self.X, self.Y)
        vb_soft = np.zeros_like(inside)
        for poly in obs.no_via:
            vb_hard |= self._raster(poly.buffer(VIA_R))
        # Pads (alle Kupferlagen; Innenlagen nur fuer Vias relevant)
        for fp in b.GetFootprints():
            for pad in fp.Pads():
                n = pad.GetNetname()
                own = bool(n) and n == net
                for layer, key in geom.CU.items():
                    if not pad.IsOnLayer(layer):
                        continue
                    shp = geom.pad_polygon(pad, layer) if pad.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH \
                        else Point(geom.to_uv(pad.GetPosition())).buffer(pcbnew.ToMM(pad.GetDrillSize().x) / 2)
                    if not own:
                        if key in LAYERS:
                            self.hard[key] |= self._raster(shp.buffer(clear(net, n) + width / 2 + MARGIN))
                        vb_hard |= self._raster(shp.buffer(clear(net, n) + VIA_R + MARGIN))
                    elif pad.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
                        # keine Vias in SMD-Pads, auch nicht im eigenen Netz
                        vb_hard |= self._raster(shp.buffer(VIA_R + 0.05))
                if pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                    hole = Point(geom.to_uv(pad.GetPosition())).buffer(pcbnew.ToMM(pad.GetDrillSize().x) / 2)
                    if not own:
                        for key in LAYERS:
                            self.hard[key] |= self._raster(hole.buffer(HOLE_CLEAR + width / 2 + MARGIN))
                    vb_hard |= self._raster(hole.buffer(0.25 + 0.15 + 0.05))
        # Bahnen und Vias: gesperrte = hart, Autorouter-Bahnen = weich
        self.soft_items = []
        for t in b.GetTracks():
            n = t.GetNetname()
            if n == net and n:
                continue
            shp = track_shape(t)
            is_via = t.Type() == pcbnew.PCB_VIA_T
            keys = list(LAYERS) if is_via else [geom.CU.get(t.GetLayer())]
            tgt = self.hard if t.IsLocked() else self.soft
            for key in keys:
                if key in LAYERS:
                    tgt[key] |= self._raster(shp.buffer(clear(net, n) + width / 2 + MARGIN))
            if is_via or keys[0] in LAYERS:
                m = self._raster(shp.buffer(clear(net, n) + VIA_R + MARGIN))
                if is_via:
                    hole = Point(geom.to_uv(t.GetPosition())).buffer(pcbnew.ToMM(t.GetDrill()) / 2)
                    m |= self._raster(hole.buffer(0.25 + 0.15 + 0.05))
                    for key in LAYERS:
                        tgt[key] |= self._raster(hole.buffer(HOLE_CLEAR + width / 2 + MARGIN))
                if t.IsLocked():
                    vb_hard |= m
                else:
                    vb_soft |= m
            if not t.IsLocked():
                self.soft_items.append(t)
        # eigene Vias: Bohrungsabstand
        for t in b.GetTracks():
            if t.Type() == pcbnew.PCB_VIA_T and t.GetNetname() == net:
                hole = Point(geom.to_uv(t.GetPosition())).buffer(pcbnew.ToMM(t.GetDrill()) / 2)
                vb_hard |= self._raster(hole.buffer(0.25 + 0.15 + 0.05))
        self.via_hard, self.via_soft = vb_hard, vb_soft

    def _raster(self, shp):
        minx, miny, maxx, maxy = shp.bounds
        i0 = max(0, int((minx + W2) / STEP) - 1)
        i1 = min(self.nx, int((maxx + W2) / STEP) + 2)
        j0 = max(0, int((miny + H2) / STEP) - 1)
        j1 = min(self.ny, int((maxy + H2) / STEP) + 2)
        m = np.zeros((self.nx, self.ny), dtype=bool)
        if i1 > i0 and j1 > j0:
            m[i0:i1, j0:j1] = shapely.contains_xy(shp, self.X[i0:i1, j0:j1], self.Y[i0:i1, j0:j1])
        return m

    def cells(self, shp, key):
        m = self._raster(shp)
        idx = np.argwhere(m)
        return {(int(i), int(j), key) for i, j in idx}

    def astar(self, starts, goals, ripup=False):
        gi = [i for i, j, k in goals]
        gj = [j for i, j, k in goals]
        i0, i1, j0, j1 = min(gi), max(gi), min(gj), max(gj)

        def h(i, j):
            # Abstand zum umschliessenden Rechteck der Ziele (zulaessig, billig)
            di = max(i0 - i, 0, i - i1)
            dj = max(j0 - j, 0, j - j1)
            return math.hypot(di, dj)

        def step_cost(k, i, j):
            """None = gesperrt, sonst Zusatzkosten."""
            if self.hard[k][i, j]:
                return None
            if self.soft[k][i, j]:
                return RIPUP_COST if ripup else None
            return 0.0

        def via_cost(i, j):
            if self.via_hard[i, j]:
                return None
            if self.via_soft[i, j]:
                return RIPUP_COST * 4 if ripup else None
            return 0.0
        openq = []
        g = {}
        came = {}
        for s in starts:
            g[s] = 0.0
            heapq.heappush(openq, (h(s[0], s[1]), s))
        dirs = [(1, 0, 1), (-1, 0, 1), (0, 1, 1), (0, -1, 1),
                (1, 1, math.sqrt(2)), (1, -1, math.sqrt(2)), (-1, 1, math.sqrt(2)), (-1, -1, math.sqrt(2))]
        n = 0
        while openq:
            f, cur = heapq.heappop(openq)
            if cur in goals:
                path = [cur]
                while path[-1] in came:
                    path.append(came[path[-1]])
                return path[::-1]
            n += 1
            if n > 1500000:
                return None
            i, j, k = cur
            gc = g[cur]
            if f > gc + h(i, j) + 1e-9 and gc > g.get(cur, 1e18):
                continue
            for di, dj, c in dirs:
                ni, nj = i + di, j + dj
                if not (0 <= ni < self.nx and 0 <= nj < self.ny):
                    continue
                nb = (ni, nj, k)
                extra = 0.0 if (nb in goals or nb in starts) else step_cost(k, ni, nj)
                if extra is None:
                    continue
                ng = gc + c + extra
                if ng < g.get(nb, 1e18):
                    g[nb] = ng
                    came[nb] = cur
                    heapq.heappush(openq, (ng + h(ni, nj), nb))
            vc = via_cost(i, j)
            if vc is not None:
                other = 'B' if k == 'F' else 'F'
                nb = (i, j, other)
                extra = 0.0 if nb in goals else step_cost(other, i, j)
                if extra is not None:
                    ng = gc + VIA_COST + vc + extra
                    if ng < g.get(nb, 1e18):
                        g[nb] = ng
                        came[nb] = cur
                        heapq.heappush(openq, (ng + h(i, j), nb))
        return None


def smooth(grid, cells, free):
    """Sichtlinien-Glaettung: Rasterpfad (eine Lage) auf moeglichst wenige gerade Stuecke
    reduzieren. Ein Stueck gilt als frei, wenn alle Rasterzellen um jeden Abtastpunkt frei
    sind (oder zum Pfad bzw. zu Start/Ziel gehoeren)."""
    key = cells[0][2]
    blk = grid.hard[key] | grid.soft[key]
    own = set(free) | set(cells)

    def ok(a, b):
        n = max(2, int(math.hypot(b[0] - a[0], b[1] - a[1]) * 4) + 1)
        for t in np.linspace(0.0, 1.0, n):
            fi, fj = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            for ci in {math.floor(fi), math.ceil(fi)}:
                for cj in {math.floor(fj), math.ceil(fj)}:
                    if blk[ci, cj] and (ci, cj, key) not in own:
                        return False
        return True
    out = [cells[0]]
    i = 0
    while i < len(cells) - 1:
        j = len(cells) - 1
        while j > i + 1 and not ok(cells[i], cells[j]):
            j -= 1
        out.append(cells[j])
        i = j
    return out


def to_uv(i, j):
    return (round(-W2 + i * STEP, 4), round(-H2 + j * STEP, 4))


def add_path(bd, net, path, width, grid=None, free=(), locked=False):
    """Pfad (Rasterzellen) in Leiterbahnen + Vias umwandeln (geglaettet, kollineare Punkte
    zusammengefasst). Gibt die neuen Objekte zurueck."""
    runs = [[path[0]]]
    vias = []
    for a, c in zip(path, path[1:]):
        if c[2] != a[2]:
            vias.append(to_uv(a[0], a[1]))
            runs.append([c])
        else:
            runs[-1].append(c)
    segs = []
    for run in runs:
        if grid is not None and len(run) > 2:
            run = smooth(grid, run, free)
        segs.append((run[0][2], [to_uv(c[0], c[1]) for c in run]))
    made = []
    for layer, p in segs:
        if len(p) < 2:
            continue
        simp = [p[0]]
        for k in range(1, len(p) - 1):
            d1 = (p[k][0] - simp[-1][0], p[k][1] - simp[-1][1])
            d2 = (p[k + 1][0] - p[k][0], p[k + 1][1] - p[k][1])
            if abs(d1[0] * d2[1] - d1[1] * d2[0]) > 1e-9:
                simp.append(p[k])
        simp.append(p[-1])
        made += bd.track(net, simp, width, layer=LAYERS[layer], locked=locked)
    for v in vias:
        made.append(bd.via(net, v[0], v[1], locked=locked))
    return made


def rip_conflicts(b, grid, net, new_items):
    """Netze bestimmen, deren Autorouter-Bahnen mit dem neuen Kupfer kollidieren, und deren
    gesamte (nicht gesperrte) Verdrahtung entfernen. Gibt die betroffenen Netznamen zurueck."""
    shapes = {'F': [], 'B': [], 'via': []}
    for t in new_items:
        if t.Type() == pcbnew.PCB_VIA_T:
            shapes['via'].append(track_shape(t))
        else:
            shapes[geom.CU[t.GetLayer()]].append(track_shape(t))
    u = {k: unary_union(v) if v else None for k, v in shapes.items()}
    victims = set()
    for t in grid.soft_items:
        n = t.GetNetname()
        shp = track_shape(t).buffer(clear(net, n) + MARGIN)
        if t.Type() == pcbnew.PCB_VIA_T:
            hit = any(g is not None and g.intersects(shp) for g in u.values())
        else:
            key = geom.CU.get(t.GetLayer())
            hit = (u.get(key) is not None and u[key].intersects(shp)) or \
                  (u['via'] is not None and u['via'].intersects(shp))
        if hit:
            victims.add(n)
    for t in list(b.GetTracks()):
        if t.GetNetname() in victims and not t.IsLocked():
            b.Delete(t)
    return victims


def drop_redundant(b):
    """Autorouter-Stuecke entfernen, die vollstaendig in gleichnamigem gesperrtem Kupfer
    (Pads, Handverdrahtung) liegen; sie erzeugen sonst "dangling track"-Meldungen."""
    fixed = {}
    for fp in b.GetFootprints():
        for pad in fp.Pads():
            for key, lay in LAYERS.items():
                if pad.GetNetname() and pad.IsOnLayer(lay):
                    fixed.setdefault((pad.GetNetname(), key), []).append(geom.pad_polygon(pad, lay))
    for t in b.GetTracks():
        if t.IsLocked() and t.Type() != pcbnew.PCB_VIA_T and geom.CU.get(t.GetLayer()) in LAYERS:
            fixed.setdefault((t.GetNetname(), geom.CU[t.GetLayer()]), []).append(
                track_shape(t).buffer(0.001))
    union = {k: unary_union(v) for k, v in fixed.items()}
    dropped = 0
    for t in list(b.GetTracks()):
        if t.IsLocked() or t.Type() == pcbnew.PCB_VIA_T:
            continue
        key = (t.GetNetname(), geom.CU.get(t.GetLayer()))
        if key not in union:
            continue
        a, e = geom.to_uv(t.GetStart()), geom.to_uv(t.GetEnd())
        shp = LineString([a, e]) if a != e else Point(a)
        if union[key].contains(shp):
            b.Delete(t)
            dropped += 1
    return dropped


def drop_islands(b, net):
    """Kupferinseln eines Netzes ohne Pad entfernen (Reste nach Rip-up)."""
    items = net_items(b, net)
    n = 0
    for g in clusters(items):
        if not any(items[i][2] == 'pad' for i in g):
            for i in g:
                t = items[i][3]
                if not t.IsLocked() and t.GetNetname() == net:
                    try:
                        b.Delete(t)
                        n += 1
                    except Exception:
                        pass
    return n


def open_nets(b, nets):
    """Netze, deren Kupfer in mehr als eine Insel zerfaellt."""
    return [n for n in nets if len(clusters(net_items(b, n))) > 1]


def connect_net(bd, net, w, ripup):
    """Alle Inseln eines Netzes verbinden. Rueckgabe: (ok, betroffene Netze)."""
    b = bd.board
    victims = set()
    while True:
        drop_islands(b, net)
        items = net_items(b, net)
        groups = clusters(items)
        if len(groups) < 2:
            return True, victims
        groups.sort(key=len)
        a = groups[0]
        rest = [i for gr in groups[1:] for i in gr]
        grid = Grid(b, net, w)
        starts, goals = set(), set()
        for i in a:
            starts |= grid.cells(items[i][0], items[i][1])
        for i in rest:
            goals |= grid.cells(items[i][0], items[i][1])
        path = grid.astar(starts, goals)
        if not path and ripup:
            path = grid.astar(starts, goals, ripup=True)
            if path:
                new = add_path(bd, net, path, w, grid, starts | goals, locked=True)
                victims |= rip_conflicts(b, grid, net, new)
                continue
        if not path:
            return False, victims
        add_path(bd, net, path, w, grid, starts | goals, locked=False)


def connect_all(bd, nets, width_of, ripup=True, max_rounds=20):
    """Offene Netze verbinden. Nur die urspruenglich offenen Netze duerfen aufreissen; die
    dabei aufgerissenen Netze werden ohne weiteres Aufreissen neu verbunden (keine Kaskade).
    Die Bahnen eines aufreissenden Netzes werden gesperrt."""
    report = []
    queue = [(n, ripup) for n in nets]
    rounds = 0
    while queue and rounds < max_rounds:
        net, may_rip = queue.pop(0)
        rounds += 1
        ok, victims = connect_net(bd, net, width_of(net), ripup=may_rip)
        report.append((net, 'verbunden' if ok else 'kein Weg gefunden',
                       sorted(victims) if victims else ''))
        for v in sorted(victims):
            if v != net and all(v != q[0] for q in queue):
                queue.append((v, False))
    for net, _ in queue:
        report.append((net, 'nicht mehr bearbeitet (Rundenlimit)', ''))
    return report


def prune_dangling(b, nets):
    """Offene Enden entfernen (nur Signalnetze ohne Kupferflaeche): eine Bahn faellt weg, wenn ein Ende nichts beruehrt und an
    ihrem uebrigen Verlauf nichts angeschlossen ist; ein Via faellt weg, wenn es nur auf einer
    Lage Kontakt hat. Wiederholt, bis sich nichts mehr aendert."""
    nets = set(nets)
    removed = 0
    while True:
        items = {}
        for fp in b.GetFootprints():
            for pad in fp.Pads():
                n = pad.GetNetname()
                if n in nets:
                    for key, lay in LAYERS.items():
                        if pad.IsOnLayer(lay):
                            items.setdefault(n, []).append((geom.pad_polygon(pad, lay), {key}, pad))
        for t in b.GetTracks():
            n = t.GetNetname()
            if n in nets:
                keys = set(LAYERS) if t.Type() == pcbnew.PCB_VIA_T else {geom.CU.get(t.GetLayer())}
                items.setdefault(n, []).append((track_shape(t), keys, t))
        kill = []
        for n, lst in items.items():
            for shp, keys, obj in lst:
                if not isinstance(obj, pcbnew.PCB_TRACK):
                    continue
                others = [(s2, k2) for s2, k2, o2 in lst if o2 is not obj]
                if obj.Type() == pcbnew.PCB_VIA_T:
                    touched = set()
                    for s2, k2 in others:
                        if shp.intersects(s2):
                            touched |= k2
                    if len(touched & set(LAYERS)) < 2:
                        kill.append(obj)
                    continue
                key = next(iter(keys))
                w = pcbnew.ToMM(obj.GetWidth()) / 2
                ends = [Point(geom.to_uv(obj.GetStart())), Point(geom.to_uv(obj.GetEnd()))]
                touching = [s2 for s2, k2 in others if key in k2 and s2.intersects(shp)]
                free = [e for e in ends if not any(s2.intersects(e.buffer(0.001)) for s2 in touching)]
                if not free:
                    continue
                if len(free) == 2:
                    kill.append(obj)
                    continue
                # ein freies Ende: nur entfernen, wenn alles Angeschlossene am anderen Ende sitzt
                fixed_end = ends[0] if free[0] is ends[1] else ends[1]
                zone = fixed_end.buffer(w + 0.05)
                if all(s2.intersection(shp).within(zone) for s2 in touching):
                    kill.append(obj)
        if not kill:
            return removed
        for t in kill:
            b.Delete(t)
            removed += 1
