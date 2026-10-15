# Geometrie-Hilfen fuer Fanout und Pruefungen (shapely, Einheiten mm, Board-Koordinaten u/v).
import pcbnew
from shapely.geometry import Point, LineString, Polygon
from shapely.ops import unary_union

OX, OY = 100.0, 100.0
CU = {pcbnew.F_Cu: 'F', pcbnew.In1_Cu: 'I1', pcbnew.In2_Cu: 'I2', pcbnew.B_Cu: 'B'}
ALL = ('F', 'I1', 'I2', 'B')


def to_uv(p):
    return (pcbnew.ToMM(p.x) - OX, pcbnew.ToMM(p.y) - OY)


def pad_polygon(pad, layer):
    ps = pcbnew.SHAPE_POLY_SET()
    pad.TransformShapeToPolygon(ps, layer, 0, pcbnew.FromMM(0.005))
    ol = ps.Outline(0)
    return Polygon([to_uv(ol.CPoint(i)) for i in range(ol.PointCount())])


def zones_of(board):
    """Alle Zonen (KiCad 7: board.Zones() ist ueber SWIG nicht iterierbar)."""
    return [board.GetArea(i) for i in range(board.GetAreaCount())]


class Obstacles:
    """Kupferobjekte je Lage: Liste von (shape, netname, kind, ref)."""

    def __init__(self, board):
        self.items = {k: [] for k in ALL}
        self.vias = []   # (u, v, net)
        self.smd_pads = []   # (poly, net, ref, num, side)
        self.no_via = []     # Polygone ohne Vias (alle Lagen)
        self.no_copper = {k: [] for k in ALL}
        for fp in board.GetFootprints():
            for pad in fp.Pads():
                net = pad.GetNetname()
                attr = pad.GetAttribute()
                for layer, key in CU.items():
                    if pad.IsOnLayer(layer):
                        if attr == pcbnew.PAD_ATTRIB_NPTH:
                            r = pcbnew.ToMM(pad.GetDrillSize().x) / 2
                            shp = Point(to_uv(pad.GetPosition())).buffer(r)
                        else:
                            shp = pad_polygon(pad, layer)
                        self.items[key].append((shp, net, 'pad', fp.GetReference()))
                if attr == pcbnew.PAD_ATTRIB_SMD:
                    side = 'F' if pad.IsOnLayer(pcbnew.F_Cu) else 'B'
                    lay = pcbnew.F_Cu if side == 'F' else pcbnew.B_Cu
                    self.smd_pads.append((pad_polygon(pad, lay), net, fp.GetReference(),
                                          pad.GetNumber(), side))
                if attr in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                    r = pcbnew.ToMM(pad.GetDrillSize().x) / 2
                    self.vias.append((to_uv(pad.GetPosition()), net, r))
        for t in board.GetTracks():
            self.add_track(t)
        for z in zones_of(board):
            if z.GetIsRuleArea():
                ol = z.Outline().Outline(0)
                poly = Polygon([to_uv(ol.CPoint(i)) for i in range(ol.PointCount())])
                if z.GetDoNotAllowVias():
                    self.no_via.append(poly)
                if z.GetDoNotAllowTracks():
                    for layer, key in CU.items():
                        if z.IsOnLayer(layer):
                            self.no_copper[key].append(poly)

    def add_track(self, t):
        net = t.GetNetname()
        if t.Type() == pcbnew.PCB_VIA_T:
            c = to_uv(t.GetPosition())
            r = pcbnew.ToMM(t.GetWidth()) / 2
            for key in ALL:
                self.items[key].append((Point(c).buffer(r), net, 'via', None))
            self.vias.append((c, net, pcbnew.ToMM(t.GetDrill()) / 2))
        else:
            a, b = to_uv(t.GetStart()), to_uv(t.GetEnd())
            w = pcbnew.ToMM(t.GetWidth())
            key = CU[t.GetLayer()]
            self.items[key].append((LineString([a, b]).buffer(w / 2), net, 'track', None))

    def conflicts(self, shape, net, layers, clearance):
        sb = shape.buffer(clearance)
        for key in layers:
            for shp, n, kind, ref in self.items[key]:
                if n == net and n:
                    continue
                if shp.intersects(sb):
                    return True
        return False
