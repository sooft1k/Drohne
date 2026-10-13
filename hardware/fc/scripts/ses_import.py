# Importiert eine Freerouting-Session (.ses) in ein KiCad-Board.
# Die KiCad-7-Python-API bietet ImportSpecctraSES nur mit geoeffnetem Editor,
# deshalb werden Leiterbahnen und Vias hier direkt angelegt.
import os
import re
import sys
sys.path.insert(0, os.path.dirname(__file__))
import pcbnew
from sexp import parse, find, find1

LAYERS = {'F.Cu': pcbnew.F_Cu, 'In1.Cu': pcbnew.In1_Cu, 'In2.Cu': pcbnew.In2_Cu,
          'B.Cu': pcbnew.B_Cu}


def import_ses(board, ses_path, replace_unlocked=True):
    t = parse(open(ses_path).read())[0]
    routes = find1(t, 'routes')
    res = find1(routes, 'resolution')
    scale = 1000.0 / float(res[2]) if str(res[1]) == 'um' else 1.0   # SES-Einheit -> nm

    def nm(v):
        return int(round(float(v) * scale))

    if replace_unlocked:
        for tr in list(board.GetTracks()):
            if not tr.IsLocked():
                board.Remove(tr)
    n_wires = n_vias = 0
    for net in find(find1(routes, 'network_out'), 'net'):
        name = net[1]
        ni = board.FindNet(name)
        if ni is None:
            raise SystemExit('Netz unbekannt: %s' % name)
        for w in find(net, 'wire'):
            path = find1(w, 'path')
            layer = LAYERS[str(path[1])]
            width = nm(path[2])
            pts = [(nm(path[i]), -nm(path[i + 1])) for i in range(3, len(path), 2)]
            for a, c in zip(pts, pts[1:]):
                if a == c:
                    continue
                tr = pcbnew.PCB_TRACK(board)
                tr.SetStart(pcbnew.VECTOR2I(*a))
                tr.SetEnd(pcbnew.VECTOR2I(*c))
                tr.SetWidth(width)
                tr.SetLayer(layer)
                tr.SetNet(ni)
                board.Add(tr)
                n_wires += 1
        for v in find(net, 'via'):
            m = re.search(r'_(\d+):(\d+)_um', v[1])
            dia, drill = int(m.group(1)) * 1000, int(m.group(2)) * 1000
            via = pcbnew.PCB_VIA(board)
            via.SetPosition(pcbnew.VECTOR2I(nm(v[2]), -nm(v[3])))
            via.SetWidth(dia)
            via.SetDrill(drill)
            via.SetViaType(pcbnew.VIATYPE_THROUGH)
            via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            via.SetNet(ni)
            board.Add(via)
            n_vias += 1
    return n_wires, n_vias


if __name__ == '__main__':
    b = pcbnew.LoadBoard(sys.argv[1])
    print(import_ses(b, sys.argv[2]))
    pcbnew.SaveBoard(sys.argv[3], b)
