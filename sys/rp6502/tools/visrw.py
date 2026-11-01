#!/usr/bin/env python3
"""Turn accesses to the vision planes into accessor calls (see src/vision.c).

    NAME[a][b] = v;      becomes  PUT(a, b, v);   (a plain '=' statement)
    NAME[a][b]           becomes  GET(a, b)       (every other read)

NAME is one of the planes in MAP. Compound assignment and ++/-- on a plane
are refused, since they would need a read-modify-write. Declarations and
single-index uses are not touched; edit those by hand before running this.

Usage: visrw.py [--dry-run] FILE
"""
import argparse
import re
import sys

MAP = {
    'viz_clear': ('vcl_get', 'vcl_put'),
    'viz_clear_rows': ('vcl_get', 'vcl_put'),
    'left_ptrs': ('lpt_get', 'lpt_put'),
    'right_ptrs': ('rpt_get', 'rpt_put'),
}
NAME = re.compile(r'(%s)\s*\[' % '|'.join(MAP))
TWO = re.compile(r'\s*\[')
ASSIGN = re.compile(r'\s*=(?!=)')
COMPOUND = re.compile(r'\s*(?:[-+*/%&|^]|<<|>>)=|\s*(?:\+\+|--)')


class Converter:
    def __init__(self, path):
        self.path = path
        self.gets = 0
        self.puts = 0
        self.single = []  # (line, text) for one-index uses, left alone

    def bracket_end(self, s, i):
        """Index after the ']' that closes the '[' at s[i]."""
        depth = 0
        j = i
        while j < len(s):
            if s[j] == '[':
                depth += 1
            elif s[j] == ']':
                depth -= 1
                if depth == 0:
                    return j + 1
            j += 1
        raise SystemExit('%s: unbalanced brackets' % self.path)

    def expr_end(self, s, i):
        """Index of the ';' (or ')' / ',' closing an outer clause) at depth 0."""
        depth = 0
        j = i
        while j < len(s):
            ch = s[j]
            if s.startswith('/*', j):
                k = s.find('*/', j + 2)
                j = len(s) if k < 0 else k + 2
                continue
            if ch in '([{':
                depth += 1
            elif ch in ')]}':
                if depth == 0:
                    return j
                depth -= 1
            elif ch in ';,' and depth == 0:
                return j
            j += 1
        raise SystemExit('%s: unterminated expression' % self.path)

    def convert(self, s):
        out = []
        i = 0
        n = len(s)
        while i < n:
            if s.startswith('/*', i):
                k = s.find('*/', i + 2)
                k = n if k < 0 else k + 2
                out.append(s[i:k])
                i = k
                continue
            if s.startswith('//', i):
                k = s.find('\n', i)
                k = n if k < 0 else k
                out.append(s[i:k])
                i = k
                continue
            if s[i] == '"':
                k = i + 1
                while k < n and s[k] != '"':
                    k += 2 if s[k] == '\\' else 1
                out.append(s[i:k + 1])
                i = k + 1
                continue
            m = NAME.match(s, i)
            if m and (i == 0 or not (s[i - 1].isalnum() or s[i - 1] in '_.>')):
                name = m.group(1)
                get, put = MAP[name]
                b1 = m.end() - 1
                e1 = self.bracket_end(s, b1)
                m2 = TWO.match(s, e1)
                if not m2:
                    line = s.count('\n', 0, i) + 1
                    self.single.append((line, s[i:e1]))
                    out.append(s[i:e1])
                    i = e1
                    continue
                b2 = m2.end() - 1
                e2 = self.bracket_end(s, b2)
                a = s[b1 + 1:e1 - 1].strip()
                b = s[b2 + 1:e2 - 1].strip()
                if COMPOUND.match(s, e2):
                    line = s.count('\n', 0, i) + 1
                    raise SystemExit('%s:%d: compound update of %s' %
                                     (self.path, line, name))
                am = ASSIGN.match(s, e2)
                if am:
                    rhs_start = am.end()
                    rhs_end = self.expr_end(s, rhs_start)
                    rhs = self.convert(s[rhs_start:rhs_end]).strip()
                    out.append('%s(%s, %s, %s)' % (put, a, b, rhs))
                    self.puts += 1
                    i = rhs_end
                    continue
                out.append('%s(%s, %s)' % (get, a, b))
                self.gets += 1
                i = e2
                continue
            out.append(s[i])
            i += 1
        return ''.join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('file')
    a = ap.parse_args()
    with open(a.file, 'rb') as f:
        raw = f.read().decode('latin-1')
    conv = Converter(a.file)
    new = conv.convert(raw)
    if not a.dry_run and new != raw:
        with open(a.file, 'wb') as f:
            f.write(new.encode('latin-1'))
    print('%s: %d reads, %d writes' % (a.file, conv.gets, conv.puts))
    for line, text in conv.single:
        print('  single-index use left alone, line %d: %s' % (line, text))
    return 0


if __name__ == '__main__':
    sys.exit(main())
