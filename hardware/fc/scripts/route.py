# Routing-Pipeline:
#   fc.kicad_pcb (platziert + vorverdrahtet) -> DSN -> Freerouting -> SES
#   -> Import -> Masseflaechen aussen -> Zonen fuellen -> fc.kicad_pcb
#
# Aufruf: python3 route.py [--passes N] [--skip-route]
import argparse
import os
import shutil
import subprocess
import sys
sys.path.insert(0, os.path.dirname(__file__))
import pcbnew
import ses_import

PRJ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BUILD = os.path.join(PRJ, 'build')
FREEROUTING = os.environ.get('FREEROUTING_JAR', '/opt/freerouting/freerouting-2.0.1.jar')


# Parametersaetze fuer den ersten Freerouting-Lauf (der erste vollstaendige gewinnt,
# sonst der mit den wenigsten offenen Netzen)
VARIANTS = [
    {},
    {'router.via_costs': 30},
    {'router.via_costs': 80},
    {'router.start_ripup_costs': 50},
    {'router.start_ripup_costs': 200},
    {'router.via_costs': 30, 'router.start_ripup_costs': 50},
]


def run_freerouting(dsn, ses, passes, opts=None):
    cmd = ['java', '-Djava.awt.headless=true', '-jar', FREEROUTING, '-de', dsn, '-do', ses,
           '-mp', str(passes), '--gui.enabled=false',
           '--usage_and_diagnostic_data.disable_analytics=true', '--api_server.enabled=false']
    cmd += ['--%s=%s' % kv for kv in (opts or {}).items()]
    # Freerouting 2.0.1 schreibt die SES-Datei im Headless-Betrieb gelegentlich leer -> wiederholen
    for attempt in range(3):
        if os.path.exists(ses):
            os.remove(ses)
        with open(os.path.join(BUILD, 'freerouting.log'), 'w') as log:
            subprocess.run(cmd, check=True, stdout=log, stderr=subprocess.STDOUT, cwd=BUILD)
        if os.path.exists(ses) and os.path.getsize(ses) > 0:
            return
        print('Freerouting: leere SES-Datei, neuer Versuch')
    raise SystemExit('Freerouting lieferte keine SES-Datei')


def sync_texts(board):
    """Freie Texte (Bestueckungsdruck) aus placement.py neu setzen."""
    import importlib
    import placement
    importlib.reload(placement)
    import gen_pcb
    for d in list(board.GetDrawings()):
        if d.Type() == pcbnew.PCB_TEXT_T or (d.Type() == pcbnew.PCB_SHAPE_T and
                                            d.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS)):
            board.Delete(d)
    bd = gen_pcb.Builder.__new__(gen_pcb.Builder)
    bd.board = board
    for t, u, v, size, ang in placement.TOP_TEXT:
        bd.text(t, u, v, pcbnew.F_SilkS, size, ang)
    for t, u, v, size, ang in placement.BOTTOM_TEXT:
        bd.text(t, u, v, pcbnew.B_SilkS, size, ang, mirror=True)
    for a in getattr(placement, 'FRONT_ARROWS', []):
        bd.arrow(*a)


def set_origin(board):
    """Bohr-/Bestueckungsursprung auf die linke untere Platinenecke (fuer Gerber, Bohrdatei
    und CPL mit gemeinsamem, positivem Koordinatensystem)."""
    import gen_pcb
    o = gen_pcb.P(-gen_pcb.W / 2, gen_pcb.H / 2)
    board.GetDesignSettings().SetAuxOrigin(o)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--passes', type=int, default=60)
    ap.add_argument('--skip-route', action='store_true', help='vorhandene build/fc.ses verwenden')
    ap.add_argument('--variants', type=int, default=len(VARIANTS),
                    help='Anzahl der Freerouting-Parametersaetze fuer den ersten Lauf')
    ap.add_argument('--extra-runs', type=int, default=3,
                    help='weitere Freerouting-Laeufe, solange Verbindungen offen sind')
    ap.add_argument('--input', default=None,
                    help='platzierte, vorverdrahtete Platine (Standard: build/fc_prerouted.kicad_pcb, '
                         'wird von gen_pcb.py geschrieben)')
    ap.add_argument('--texts-only', action='store_true',
                    help='nur Beschriftung der fertigen Platine aus placement.py erneuern')
    args = ap.parse_args()
    if args.texts_only:
        board = pcbnew.LoadBoard(os.path.join(PRJ, 'fc.kicad_pcb'))
        sync_texts(board)
        set_origin(board)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        pcbnew.SaveBoard(os.path.join(PRJ, 'fc.kicad_pcb'), board)
        print('Beschriftung aktualisiert')
        return
    os.makedirs(BUILD, exist_ok=True)
    pre = os.path.join(BUILD, 'fc_prerouted.kicad_pcb')
    if args.input is None:
        args.input = pre
    if os.path.abspath(args.input) != pre:
        shutil.copy(args.input, pre)
        shutil.copy(os.path.join(PRJ, 'fc.kicad_pro'), os.path.join(BUILD, 'fc_prerouted.kicad_pro'))
    import zones
    import gen_pcb
    import patch_route
    import geom
    board = pcbnew.LoadBoard(pre)
    dsn = os.path.join(BUILD, 'fc.dsn')
    ses = os.path.join(BUILD, 'fc.ses')
    plane_nets = {z.GetNetname() for z in geom.zones_of(board) if not z.GetIsRuleArea()}
    sig = sorted({p.GetNetname() for fp in board.GetFootprints() for p in fp.Pads()
                  if p.GetNetname() and not p.GetNetname().startswith('unconnected')} - plane_nets)
    if not args.skip_route:
        assert pcbnew.ExportSpecctraDSN(board, dsn)
        best = None
        for vi, opts in enumerate(VARIANTS[:args.variants]):
            ses_v = os.path.join(BUILD, 'fc_v%d.ses' % vi)
            run_freerouting(dsn, ses_v, args.passes, opts)
            bv = pcbnew.LoadBoard(pre)
            ses_import.import_ses(bv, ses_v)
            patch_route.drop_redundant(bv)
            open_v = patch_route.open_nets(bv, sig)
            print('Variante %d %s: offen %s' % (vi, opts or 'Standard', open_v))
            if best is None or len(open_v) < len(best[1]):
                best = (vi, open_v)
            if not open_v:
                break
        shutil.copy(os.path.join(BUILD, 'fc_v%d.ses' % best[0]), ses)
        print('gewaehlt: Variante', best[0])
    print('SES-Import: %d Leiterbahnsegmente, %d Vias' % ses_import.import_ses(board, ses))
    print('redundante Autorouter-Stuecke entfernt:', patch_route.drop_redundant(board))
    todo = patch_route.open_nets(board, sig)
    print('offen nach Autorouter:', todo)
    # Weitere Freerouting-Laeufe auf dem gerouteten Stand: die vorhandenen Bahnen bleiben
    # (nicht fixiert) erhalten, der Router verlegt nur die Reste mit Rip-up nach
    for k in range(2, 2 + args.extra_runs):
        if not todo or args.skip_route:
            break
        tmp = os.path.join(BUILD, 'fc_lauf%d.kicad_pcb' % (k - 1))
        pcbnew.SaveBoard(tmp, board)
        shutil.copy(os.path.join(PRJ, 'fc.kicad_pro'), tmp.replace('.kicad_pcb', '.kicad_pro'))
        board = pcbnew.LoadBoard(tmp)
        dsn_k = os.path.join(BUILD, 'fc_lauf%d.dsn' % k)
        ses_k = os.path.join(BUILD, 'fc_lauf%d.ses' % k)
        assert pcbnew.ExportSpecctraDSN(board, dsn_k)
        run_freerouting(dsn_k, ses_k, args.passes)
        print('Lauf %d: %d Leiterbahnsegmente, %d Vias' % ((k,) + ses_import.import_ses(board, ses_k)))
        print('  redundant entfernt:', patch_route.drop_redundant(board))
        todo = patch_route.open_nets(board, sig)
        print('  offen:', todo)

    class Nets(dict):
        def __missing__(self, k):
            return board.FindNet(k)
    bd = gen_pcb.Builder.__new__(gen_pcb.Builder)
    bd.board = board
    bd.nets = Nets()
    # Letzte Reste mit dem eigenen Nachrouter schliessen
    # (nur Netze ohne eigene Kupferflaeche; Flaechennetze haengen an den Ebenen)
    width = {n: c[0] for c in gen_pcb.NETCLASSES.values() for n in c[4]}
    wof = lambda n: width.get(n, 0.2)
    if todo:
        snap = os.path.join(BUILD, 'fc_vor_nachrouter.kicad_pcb')
        pcbnew.SaveBoard(snap, board)
        shutil.copy(os.path.join(PRJ, 'fc.kicad_pro'), snap.replace('.kicad_pcb', '.kicad_pro'))
        for r in patch_route.connect_all(bd, todo, wof):
            print('  Nachrouter:', r)
        left = patch_route.open_nets(board, sig)
        if len(left) >= len(todo):
            # Rip-up hat nichts gebracht: Stand zuruecknehmen, nur ohne Aufreissen versuchen
            print('  Rip-up verworfen, offen waeren:', left)
            board = pcbnew.LoadBoard(snap)
            bd.board = board
            for r in patch_route.connect_all(bd, todo, wof, ripup=False):
                print('  Nachrouter (ohne Rip-up):', r)
            left = patch_route.open_nets(board, sig)
        print('offen nach Nachrouter:', left)
    pre_prune = os.path.join(BUILD, 'fc_vor_bereinigung.kicad_pcb')
    pcbnew.SaveBoard(pre_prune, board)
    shutil.copy(os.path.join(PRJ, 'fc.kicad_pro'), pre_prune.replace('.kicad_pcb', '.kicad_pro'))
    print('offene Enden / einseitige Vias entfernt:', patch_route.prune_dangling(board, sig))
    left = patch_route.open_nets(board, sig)
    if left:
        print('ACHTUNG, offen:', left)
    # Masseflaechen aussen (nach dem Routing, damit der Router freie Bahn hat)
    zones.outer_gnd(bd)
    sync_texts(board)
    set_origin(board)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = os.path.join(PRJ, 'fc.kicad_pcb')
    pcbnew.SaveBoard(out, board)
    print('gespeichert:', out)


if __name__ == '__main__':
    main()
