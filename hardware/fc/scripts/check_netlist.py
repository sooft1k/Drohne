# Vergleicht die aus dem Schaltplan exportierte Netzliste mit design.py.
# Aufruf: python3 check_netlist.py fc.net
import os
import sys
sys.path.insert(0, os.path.dirname(__file__))
from sexp import parse, find, find1
import design


def read_netlist(path):
    t = parse(open(path).read())[0]
    nets = {}
    for n in find(find1(t, 'nets'), 'net'):
        name = find1(n, 'name')[1]
        for node in find(n, 'node'):
            nets[(find1(node, 'ref')[1], find1(node, 'pin')[1])] = name
    comps = {}
    for c in find(find1(t, 'components'), 'comp'):
        comps[find1(c, 'ref')[1]] = dict(value=find1(c, 'value')[1],
                                         fp=(find1(c, 'footprint') or [None, ''])[1])
    return nets, comps


def main(path):
    nets, comps = read_netlist(path)
    errors = []
    for p in design.PARTS:
        ref = p['ref']
        if not p['pins']:
            continue
        if ref not in comps:
            errors.append('Bauteil fehlt im Schaltplan: ' + ref)
            continue
        if comps[ref]['fp'] != p['fp']:
            errors.append('%s Footprint %s != %s' % (ref, comps[ref]['fp'], p['fp']))
        if comps[ref]['value'] != p['value']:
            errors.append('%s Wert %s != %s' % (ref, comps[ref]['value'], p['value']))
        for pin, want in p['pins'].items():
            got = nets.get((ref, pin))
            if want is None:
                if got is not None and not got.startswith('unconnected-'):
                    errors.append('%s.%s soll offen sein, ist aber %s' % (ref, pin, got))
            else:
                if got is None or got.lstrip('/') != want:
                    errors.append('%s.%s soll %s sein, ist %s' % (ref, pin, want, got))
    extra = {k for k in nets if k[0] not in {p['ref'] for p in design.PARTS}}
    for k in sorted(extra):
        if not k[0].startswith('#'):
            errors.append('unbekannter Knoten %s' % (k,))
    n_nodes = sum(1 for p in design.PARTS for v in p['pins'].values() if v)
    print('Knoten geprueft:', n_nodes, 'Fehler:', len(errors))
    for e in errors:
        print('  ', e)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
