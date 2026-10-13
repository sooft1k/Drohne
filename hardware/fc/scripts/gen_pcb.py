# Erzeugt fc.kicad_pcb: Lagenaufbau, Umriss, Bohrungen, Bauteilplatzierung,
# Kupferflaechen, Sperrflaechen, Beschriftung und Vorverdrahtung (Abblockung).
# Koordinaten in diesem Skript: (u, v) in mm relativ zur Platinenmitte,
# v nach unten (wie KiCad). KiCad-Koordinate = (100 + u, 100 + v).
import json
import math
import os
import sys
sys.path.insert(0, os.path.dirname(__file__))
import pcbnew
import design
import lcsc
from gen_sch import uid
from placement import PLACE, BOTTOM_TEXT, TOP_TEXT
import prerouting

PRJ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
KIFP = '/usr/share/kicad/footprints'
OX, OY = 100.0, 100.0
W, H, RADIUS = 42.0, 41.0, 2.5
HOLE = 15.25

NETCLASSES = {
    # Name: (Leiterbahnbreite, Abstand, Via-Durchmesser, Bohrung, Netze)
    'Default': (0.2, 0.15, 0.6, 0.3, []),
    'Motor': (0.25, 0.15, 0.6, 0.3, ['M1', 'M2', 'M3', 'M4']),
    'P3V3': (0.5, 0.15, 0.6, 0.3, ['+3V3', '+3V3A']),
    'P5V': (0.6, 0.2, 0.6, 0.3, ['+5V']),
    'USB5V': (0.5, 0.15, 0.6, 0.3, ['VBUS', 'ESC_5V']),
    'VBAT': (1.0, 0.2, 0.6, 0.3, ['VBAT', 'VBAT_IN']),
    'GND': (0.5, 0.15, 0.6, 0.3, ['GND']),
    'Buck': (0.6, 0.2, 0.6, 0.3, ['BUCK_SW']),
}


def mm(v):
    return pcbnew.FromMM(v)


def P(u, v):
    return pcbnew.VECTOR2I(mm(OX + u), mm(OY + v))


def fp_path(lib, name):
    if lib == 'fc_custom':
        return os.path.join(PRJ, 'fc_custom.pretty')
    return os.path.join(KIFP, lib + '.pretty')


class Builder:
    def __init__(self):
        self.board = pcbnew.BOARD()
        b = self.board
        b.SetCopperLayerCount(4)
        b.SetLayerName(pcbnew.In1_Cu, 'In1.Cu')
        b.SetLayerName(pcbnew.In2_Cu, 'In2.Cu')
        b.SetLayerType(pcbnew.In1_Cu, pcbnew.LT_POWER)
        b.SetLayerType(pcbnew.In2_Cu, pcbnew.LT_POWER)
        ds = b.GetDesignSettings()
        ds.m_MinClearance = mm(0.15)
        ds.m_TrackMinWidth = mm(0.15)
        ds.m_ViasMinSize = mm(0.6)
        ds.m_MinThroughDrill = mm(0.3)
        ds.m_CopperEdgeClearance = mm(0.5)
        ds.m_HoleToHoleMin = mm(0.25)
        ds.m_HoleClearance = mm(0.2)
        self.nets = {}
        for n in sorted(design.nets()):
            ni = pcbnew.NETINFO_ITEM(b, n)
            b.Add(ni)
            self.nets[n] = ni
        self.fps = {}

    # ------------------------------------------------------------- Umriss
    def outline(self):
        b = self.board
        x0, x1, y0, y1, r = -W / 2, W / 2, -H / 2, H / 2, RADIUS
        segs = [((x0 + r, y0), (x1 - r, y0)), ((x1, y0 + r), (x1, y1 - r)),
                ((x1 - r, y1), (x0 + r, y1)), ((x0, y1 - r), (x0, y0 + r))]
        for a, c in segs:
            s = pcbnew.PCB_SHAPE(b, pcbnew.SHAPE_T_SEGMENT)
            s.SetStart(P(*a))
            s.SetEnd(P(*c))
            s.SetLayer(pcbnew.Edge_Cuts)
            s.SetWidth(mm(0.1))
            b.Add(s)
        for cx, cy, a0 in ((x0 + r, y0 + r, 180), (x1 - r, y0 + r, 270),
                           (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90)):
            s = pcbnew.PCB_SHAPE(b, pcbnew.SHAPE_T_ARC)
            s.SetCenter(P(cx, cy))
            s.SetStart(P(cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0))))
            s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T), True)
            s.SetLayer(pcbnew.Edge_Cuts)
            s.SetWidth(mm(0.1))
            b.Add(s)

    # ------------------------------------------------------------- Bauteile
    def place_parts(self):
        b = self.board
        for p in design.PARTS:
            ref = p['ref']
            if ref not in PLACE:
                raise SystemExit('keine Position fuer ' + ref)
            u, v, rot, side = PLACE[ref]
            lib, name = p['fp'].split(':')
            fp = pcbnew.FootprintLoad(fp_path(lib, name), name)
            if fp is None:
                raise SystemExit('Footprint fehlt: ' + p['fp'])
            fp.SetFPID(pcbnew.LIB_ID(lib, name))
            fp.SetReference(ref)
            fp.SetValue(p['value'])
            fp.SetPath(pcbnew.KIID_PATH('/' + uid('s', ref)))
            b.Add(fp)
            fp.SetPosition(P(u, v))
            fp.SetOrientationDegrees(rot)
            if side == 'bottom':
                fp.Flip(fp.GetPosition(), True)
            for pad in fp.Pads():
                num = pad.GetNumber()
                net = p['pins'].get(num)
                if net:
                    pad.SetNet(self.nets[net])
            # Referenz auf die Fab-Lage (Bestueckungsdruck bleibt lesbar: nur
            # Steckverbinder-Namen, Polaritaet und Pin-1-Markierungen), Wert unsichtbar
            ref_t = fp.Reference()
            ref_t.SetTextSize(pcbnew.VECTOR2I(mm(0.5), mm(0.5)))
            ref_t.SetTextThickness(mm(0.08))
            ref_t.SetLayer(pcbnew.B_Fab if side == 'bottom' else pcbnew.F_Fab)
            fp.Value().SetVisible(False)
            if ref in lcsc.REF:
                fp.SetProperty('LCSC', lcsc.REF[ref])
            self.fps[ref] = fp

    def pad_pos_obj(self, pad):
        c = pad.GetPosition()
        return (pcbnew.ToMM(c.x) - OX, pcbnew.ToMM(c.y) - OY)

    def pad_pos(self, ref, num):
        for pad in self.fps[ref].Pads():
            if pad.GetNumber() == num:
                c = pad.GetPosition()
                return (pcbnew.ToMM(c.x) - OX, pcbnew.ToMM(c.y) - OY)
        raise KeyError((ref, num))

    # ------------------------------------------------------------- Texte
    def text(self, s, u, v, layer, size=0.8, angle=0, mirror=False, bold=False):
        t = pcbnew.PCB_TEXT(self.board)
        t.SetText(s)
        t.SetPosition(P(u, v))
        t.SetLayer(layer)
        w, h = size if isinstance(size, tuple) else (size, size)
        t.SetTextSize(pcbnew.VECTOR2I(mm(w), mm(h)))
        t.SetTextThickness(mm(max(0.16, h * 0.16)))   # JLCPCB: >= 1,0 mm / 0,153 mm
        t.SetBold(bold)
        t.SetTextAngleDegrees(angle)
        if mirror:
            t.SetMirrored(True)
        self.board.Add(t)

    # ------------------------------------------------------------- Zonen
    def zone(self, net, layer, poly, priority=0, name=None, thermal=0.4, clearance=0.25,
             min_width=0.2, fill_mode_solid_for_pads=False):
        z = pcbnew.ZONE(self.board)
        z.SetLayer(layer)
        if net:
            z.SetNet(self.nets[net])
        ol = z.Outline()
        ol.NewOutline()
        for u, v in poly:
            ol.Append(mm(OX + u), mm(OY + v))
        z.SetAssignedPriority(priority)
        z.SetLocalClearance(mm(clearance))
        z.SetMinThickness(mm(min_width))
        z.SetThermalReliefGap(mm(thermal))
        z.SetThermalReliefSpokeWidth(mm(0.35))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        z.SetMinIslandArea(0)
        if name:
            z.SetZoneName(name)
        self.board.Add(z)
        return z

    def keepout(self, poly, layers, tracks=True, vias=True, pads=False, pours=True, name=None):
        z = pcbnew.ZONE(self.board)
        z.SetIsRuleArea(True)
        ls = pcbnew.LSET()
        for l in layers:
            ls.AddLayer(l)
        z.SetLayerSet(ls)
        ol = z.Outline()
        ol.NewOutline()
        for u, v in poly:
            ol.Append(mm(OX + u), mm(OY + v))
        z.SetDoNotAllowTracks(tracks)
        z.SetDoNotAllowVias(vias)
        z.SetDoNotAllowPads(pads)
        z.SetDoNotAllowCopperPour(pours)
        z.SetDoNotAllowFootprints(False)
        if name:
            z.SetZoneName(name)
        self.board.Add(z)
        return z

    # ------------------------------------------------------------- Leiterbahnen
    def track(self, net, pts, width, layer=pcbnew.F_Cu, locked=True):
        made = []
        for a, c in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(self.board)
            t.SetStart(P(*a))
            t.SetEnd(P(*c))
            t.SetWidth(mm(width))
            t.SetLayer(layer)
            t.SetNet(self.nets[net])
            t.SetLocked(locked)
            self.board.Add(t)
            made.append(t)
        return made

    def via(self, net, u, v, dia=0.6, drill=0.3, locked=True):
        t = pcbnew.PCB_VIA(self.board)
        t.SetPosition(P(u, v))
        t.SetWidth(mm(dia))
        t.SetDrill(mm(drill))
        t.SetViaType(pcbnew.VIATYPE_THROUGH)
        t.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        t.SetNet(self.nets[net])
        t.SetLocked(locked)
        self.board.Add(t)
        return t


def circle(cx, cy, r, n=32):
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n))
            for i in range(n)]


def edge_band(m):
    x0, x1, y0, y1, r = -W / 2, W / 2, -H / 2, H / 2, RADIUS
    e = 1.0  # ueber den Rand hinaus
    out = [[(x0 - e, y0 - e), (x1 + e, y0 - e), (x1 + e, y0 + m), (x0 - e, y0 + m)],
           [(x0 - e, y1 - m), (x1 + e, y1 - m), (x1 + e, y1 + e), (x0 - e, y1 + e)],
           [(x0 - e, y0 - e), (x0 + m, y0 - e), (x0 + m, y1 + e), (x0 - e, y1 + e)],
           [(x1 - m, y0 - e), (x1 + e, y0 - e), (x1 + e, y1 + e), (x1 - m, y1 + e)]]
    for cx, cy, a0, sx, sy in ((x0 + r, y0 + r, 180, x0 - e, y0 - e), (x1 - r, y0 + r, 270, x1 + e, y0 - e),
                               (x1 - r, y1 - r, 0, x1 + e, y1 + e), (x0 + r, y1 - r, 90, x0 - e, y1 + e)):
        pts = [(sx, sy)]
        for i in range(9):
            a = math.radians(a0 + 90 * i / 8)
            pts.append((cx + (r - m) * math.cos(a), cy + (r - m) * math.sin(a)))
        out.append(pts)
    return out


def rounded_rect(x0, y0, x1, y1, r, n=8):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90),
                       (x0 + r, y0 + r, 180)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def write_project(path):
    d = json.load(open(path))
    cls = []
    pats = []
    for name, (tw, cl, vd, vdr, nets) in NETCLASSES.items():
        cls.append({'bus_width': 12, 'clearance': cl, 'diff_pair_gap': 0.2,
                    'diff_pair_via_gap': 0.25, 'diff_pair_width': 0.2, 'line_style': 0,
                    'microvia_diameter': 0.3, 'microvia_drill': 0.1, 'name': name,
                    'pcb_color': 'rgba(0, 0, 0, 0.000)',
                    'schematic_color': 'rgba(0, 0, 0, 0.000)', 'track_width': tw,
                    'via_diameter': vd, 'via_drill': vdr, 'wire_width': 6})
        for n in nets:
            pats.append({'netclass': name, 'pattern': n})
    d['net_settings']['classes'] = cls
    d['net_settings']['netclass_patterns'] = pats
    r = d['board']['design_settings']['rules']
    r.update({'min_clearance': 0.15, 'min_track_width': 0.15, 'min_via_diameter': 0.6,
              'min_through_hole_diameter': 0.3, 'min_via_annular_width': 0.13,
              'min_copper_edge_clearance': 0.5, 'min_hole_to_hole': 0.25,
              'min_hole_clearance': 0.2, 'min_silk_clearance': 0.0,
              'min_text_height': 1.0, 'min_text_thickness': 0.15,
              'min_resolved_spokes': 1})
    ds = d['board']['design_settings']
    ds['track_widths'] = [0.0, 0.15, 0.2, 0.25, 0.3, 0.5, 0.8, 1.0]
    ds['via_dimensions'] = [{'diameter': 0.0, 'drill': 0.0}, {'diameter': 0.6, 'drill': 0.3}]
    sev = ds.setdefault('rule_severities', {})
    d['meta'] = {'filename': 'fc.kicad_pro', 'version': 1}
    # Schaltplan-Teil des Projekts
    d.setdefault('schematic', {})['legacy_lib_dir'] = ''
    d['sheets'] = [[uid('root-sheet'), '']]
    json.dump(d, open(path, 'w'), indent=2, sort_keys=True)


def build(out=None, with_zones=True):
    bd = Builder()
    bd.outline()
    bd.place_parts()
    b = bd.board

    # Sperrflaechen um die drei nicht angeschlossenen Bohrungen (Abstandshalter)
    for (u, v) in ((-HOLE, HOLE), (HOLE, -HOLE), (-HOLE, -HOLE)):
        bd.keepout(circle(u, v, 3.3), [pcbnew.F_Cu, pcbnew.B_Cu], name='Abstandshalter')
        bd.keepout(circle(u, v, 2.3), [pcbnew.In1_Cu, pcbnew.In2_Cu], name='Bohrung innen')
    # Masse-Bohrung: Kupferring auf allen Lagen, keine fremden Leiter
    bd.keepout(circle(HOLE, HOLE, 3.4), [pcbnew.F_Cu, pcbnew.B_Cu, pcbnew.In2_Cu],
               tracks=True, vias=True, pours=False, name='Masse-Bohrung')
    # Randabstand 0,5 mm fuer Kupfer (als Sperrflaeche, damit der Autorouter ihn kennt)
    m = 0.55
    allcu = [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
    for poly in edge_band(m):
        bd.keepout(poly, [pcbnew.F_Cu, pcbnew.B_Cu], tracks=True, vias=True, pours=False,
                   name='Randabstand')
    # Fiducials: 1 mm Kupferfreiheit
    for ref in ('FID1', 'FID2', 'FID3'):
        u, v = PLACE[ref][0], PLACE[ref][1]
        bd.keepout(circle(u, v, 1.1, 16), [pcbnew.F_Cu], tracks=True, vias=True, pours=True,
                   name='Fiducial')
    # Keine Durchkontaktierungen unter dem Gyro
    gx, gy = PLACE['U2'][0], PLACE['U2'][1]
    bd.keepout([(gx - 1.9, gy - 1.6), (gx + 1.9, gy - 1.6), (gx + 1.9, gy + 1.6),
                (gx - 1.9, gy + 1.6)],
               [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu],
               tracks=False, vias=True, pours=False, name='Gyro ohne Vias')

    # Beschriftung
    for s, u, v, size, ang in TOP_TEXT:
        bd.text(s, u, v, pcbnew.F_SilkS, size, ang)
    for s, u, v, size, ang in BOTTOM_TEXT:
        bd.text(s, u, v, pcbnew.B_SilkS, size, ang, mirror=True)

    # Vorverdrahtung: Abblockkondensatoren, Massevias, Waermevias
    prerouting.apply(bd)
    import fanout
    done, failed = fanout.fanout(bd)
    bd.fanout_failed = failed
    print('Fanout: %d Pads mit Via, %d ohne Platz/falscher Bereich' % (len(done), len(failed)))
    for f in failed:
        print('   ohne Via:', f)

    if with_zones:
        import zones
        zones.apply(bd)

    out = out or os.path.join(PRJ, 'fc.kicad_pcb')
    pcbnew.SaveBoard(out, b)
    write_project(os.path.splitext(out)[0] + '.kicad_pro')
    # Ausgangsstand fuer route.py (vorverdrahtet, ohne Autorouter-Bahnen)
    os.makedirs(os.path.join(PRJ, 'build'), exist_ok=True)
    pre = os.path.join(PRJ, 'build', 'fc_prerouted.kicad_pcb')
    pcbnew.SaveBoard(pre, b)
    write_project(os.path.splitext(pre)[0] + '.kicad_pro')
    return bd


if __name__ == '__main__':
    build()
    print('ok')
