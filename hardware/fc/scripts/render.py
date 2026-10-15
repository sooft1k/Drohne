# Rendert Ober- und Unterseite als PNG (fuer die Platzierungskontrolle).
import os, subprocess, sys
pcb = sys.argv[1]
out = sys.argv[2]
sets = {
    'top': 'F.Cu,F.SilkS,F.CrtYd,F.Fab,Edge.Cuts',
    'bot': 'B.Cu,B.SilkS,B.CrtYd,B.Fab,Edge.Cuts',
    'in1': 'In1.Cu,Edge.Cuts',
    'in2': 'In2.Cu,Edge.Cuts',
}
which = sys.argv[3].split(',') if len(sys.argv) > 3 else ['top', 'bot']
for k in which:
    svg = '%s_%s.svg' % (out, k)
    args = ['kicad-cli', 'pcb', 'export', 'svg', '--exclude-drawing-sheet', '--page-size-mode', '2',
            '-l', sets[k], '-o', svg, pcb]
    if k == 'bot':
        args.insert(4, '--mirror')
    subprocess.run(args, check=True, capture_output=True)
    subprocess.run(['rsvg-convert', '-w', '1400', '-b', 'white', svg, '-o', '%s_%s.png' % (out, k)], check=True)
    print('%s_%s.png' % (out, k))
