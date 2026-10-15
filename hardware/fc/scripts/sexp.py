# Minimal S-expression parser/writer for KiCad files
import re
class Sym(str):
    pass
def parse(text):
    tokens = re.findall(r'"(?:[^"\\]|\\.)*"|\(|\)|[^\s()"]+', text)
    stack = [[]]
    for t in tokens:
        if t == '(':
            stack.append([])
        elif t == ')':
            x = stack.pop(); stack[-1].append(x)
        elif t.startswith('"'):
            stack[-1].append(t[1:-1].replace('\\"', '"').replace('\\\\', '\\'))
        else:
            stack[-1].append(Sym(t))
    return stack[0]
def find(lst, key):
    return [x for x in lst if isinstance(x, list) and x and x[0] == key]
def find1(lst, key):
    r = find(lst, key); return r[0] if r else None
def dump(x, indent=0):
    if isinstance(x, list):
        if all(not isinstance(e, list) for e in x):
            return '(' + ' '.join(dump(e) for e in x) + ')'
        s = '(' + dump(x[0])
        for e in x[1:]:
            if isinstance(e, list):
                s += '\n' + '  ' * (indent + 1) + dump(e, indent + 1)
            else:
                s += ' ' + dump(e)
        return s + '\n' + '  ' * indent + ')'
    if isinstance(x, Sym):
        return str(x)
    if isinstance(x, bool):
        return 'yes' if x else 'no'
    if isinstance(x, (int, float)):
        if isinstance(x, float):
            s = ('%.6f' % x).rstrip('0').rstrip('.')
            return s if s not in ('-0', '') else '0'
        return str(x)
    return '"' + str(x).replace('\\', '\\\\').replace('"', '\\"') + '"'
