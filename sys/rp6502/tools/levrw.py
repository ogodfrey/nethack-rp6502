#!/usr/bin/env python3
"""Rewrite level-map accesses (levl[x][y]) to the lev_get()/lev_put() API.

The level map can live in XRAM, where C cannot take the address of a cell or
assign to one of its fields. This tool rewrites the uses it can express as
plain calls, in place:

  levl[x][y]            lev_get(x, y)
  levl[x][y].f          lev_get(x, y).f
  levl[x][y].f = v      ({ int _lv = (v); int _lx = (x), _ly = (y);
                           struct rm _lr = lev_get(_lx, _ly); _lr.f = _lv;
                           lev_put(_lx, _ly, _lr); _lr.f; })
  levl[x][y].f OP= v    as above, with _lr.f OP _lv
  levl[x][y].f++        as above, with _lr.f++ (statement context only)
  levl[x][y] = v        ({ __auto_type _lv = (v); lev_put(x, y, _lv); })
                        (statement context only)

Not rewritten; reported as manual sites with their file and line:
  - taking an address: &levl[x][y] and &levl[x][y].f
  - a whole-cell write or a postfix ++/-- whose value is used
  - uses in a preprocessor line (macro bodies) or in a file's #define
Those are fixed by hand with a local copy:  struct rm r = lev_get(x, y); ...
lev_put(x, y, r);

The scanner skips comments, string and character literals. Nested uses (in
an index or a right side) are rewritten too. Running it again on rewritten
text does nothing, because no levl[ is left to match.

Usage: levrw.py [--dry-run] FILE...
Files are rewritten in place. Bytes are kept (CRLF stays CRLF).
"""
import argparse
import re
import sys

CELL = re.compile(r'(?:levl|level\s*\.\s*locations)\s*\[')
ASSIGN = re.compile(r'(<<=|>>=|[|&^+\-*/%]?=)(?!=)')
IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
OPEN = '([{'
CLOSE = ')]}'


class Rewriter:
    def __init__(self, text, path):
        self.s = text
        self.path = path
        self.manual = []  # (line, reason, snippet)
        self.count = 0

    def line_of(self, i):
        return self.s.count('\n', 0, i) + 1

    def manual_site(self, i, reason, end):
        snippet = ' '.join(self.s[i:min(end, i + 80)].split())
        self.manual.append((self.line_of(i), reason, snippet))

    def skip_literal(self, i):
        """Index after a comment, string or character literal at i, or None."""
        s = self.s
        if s.startswith('/*', i):
            j = s.find('*/', i + 2)
            return len(s) if j < 0 else j + 2
        if s.startswith('//', i):
            j = s.find('\n', i)
            return len(s) if j < 0 else j
        if s[i] in '"\'':
            q = s[i]
            j = i + 1
            while j < len(s) and s[j] != q and s[j] != '\n':
                j += 2 if s[j] == '\\' else 1
            return min(j + 1, len(s))
        return None

    def line_is_directive(self, i):
        j = self.s.rfind('\n', 0, i) + 1
        return self.s[j:i].strip() == '' and self.s[i:i + 1] == '#'

    def directive_end(self, i):
        j = i
        while True:
            k = self.s.find('\n', j)
            if k < 0:
                return len(self.s)
            if self.s[k - 1:k] == '\\' or self.s[k - 2:k] == '\\\r':
                j = k + 1
                continue
            return k

    def match_bracket(self, i):
        """Index after the bracket that closes the one at i, or -1."""
        s = self.s
        depth = 0
        j = i
        while j < len(s):
            lit = self.skip_literal(j)
            if lit is not None:
                j = lit
                continue
            ch = s[j]
            if ch in OPEN:
                depth += 1
            elif ch in CLOSE:
                depth -= 1
                if depth == 0:
                    return j + 1
            j += 1
        return -1

    def expr_end(self, i):
        """End of the expression starting at i: the first ';' ',' or closing
        bracket at depth 0."""
        s = self.s
        depth = 0
        j = i
        while j < len(s):
            lit = self.skip_literal(j)
            if lit is not None:
                j = lit
                continue
            ch = s[j]
            if ch in OPEN:
                depth += 1
            elif ch in CLOSE:
                if depth == 0:
                    return j
                depth -= 1
            elif ch in ';,' and depth == 0:
                return j
            j += 1
        return j

    def prev_char(self, i):
        j = i - 1
        while j >= 0 and self.s[j] in ' \t\r\n':
            j -= 1
        return (self.s[j], j) if j >= 0 else ('', -1)

    def rewrite_text(self, a, b):
        """Rewrite the sub-expression in self.s[a:b], returning the new text."""
        saved = self.s
        sub = Rewriter(saved[a:b], self.path)
        out = sub.run()
        self.manual.extend((self.line_of(a) + ln - 1, r, t)
                           for ln, r, t in sub.manual)
        self.count += sub.count
        return out

    def run(self):
        s = self.s
        out = []
        i = 0
        n = len(s)
        while i < n:
            if s[i] == '#' and self.line_is_directive(i):
                end = self.directive_end(i)
                seg = s[i:end]
                if CELL.search(seg):
                    self.manual_site(i, 'use in a preprocessor line', end)
                out.append(seg)
                i = end
                continue
            lit = self.skip_literal(i)
            if lit is not None:
                out.append(s[i:lit])
                i = lit
                continue
            m = CELL.match(s, i)
            if m and (i == 0 or not (s[i - 1].isalnum() or s[i - 1] in '_.>')):
                res = self.cell(i, m)
                if res is not None:
                    text, i = res
                    out.append(text)
                    continue
            out.append(s[i])
            i += 1
        return ''.join(out)

    def cell(self, i, m):
        """Handle one access at i; return (replacement, end) or None if it is
        not a cell access that the scanner recognises."""
        s = self.s
        b1 = m.end() - 1
        e1 = self.match_bracket(b1)
        if e1 < 0:
            return None
        m2 = re.compile(r'\s*\[').match(s, e1)
        if not m2:
            return None
        b2 = m2.end() - 1
        e2 = self.match_bracket(b2)
        if e2 < 0:
            return None
        X = self.rewrite_text(b1 + 1, e1 - 1)
        Y = self.rewrite_text(b2 + 1, e2 - 1)
        cell_end = e2
        # Unary & before the cell (possibly through parentheses): an address,
        # not a logical and.
        j0 = i - 1
        while j0 >= 0 and s[j0] in ' \t\r\n(':
            j0 -= 1
        if j0 >= 0 and s[j0] == '&' and not (j0 > 0 and s[j0 - 1] == '&'):
            self.manual_site(i, 'address of a cell', e2)
            return None
        j = cell_end
        while j < len(s) and s[j] in ' \t':
            j += 1
        fm = re.compile(r'\.\s*([A-Za-z_]\w*)').match(s, j)
        if fm:
            field = fm.group(1)
            k = fm.end()
            while k < len(s) and s[k] in ' \t':
                k += 1
            am = ASSIGN.match(s, k)
            if am and not s.startswith('==', k):
                op = am.group(1)
                rs = am.end()
                while rs < len(s) and s[rs] in ' \t\r\n':
                    rs += 1
                re_ = self.expr_end(rs)
                rhs = self.rewrite_text(rs, re_)
                body = ('_lr.%s = _lv;' % field) if op == '=' else \
                    ('_lr.%s %s _lv;' % (field, op))
                self.count += 1
                repl = ('({ int _lv = (%s); int _lx = (%s), _ly = (%s); '
                        'struct rm _lr = lev_get(_lx, _ly); %s '
                        'lev_put(_lx, _ly, _lr); _lr.%s; })'
                        % (rhs, X, Y, body, field))
                return (repl, re_)
            incm = re.compile(r'\+\+|--').match(s, k)
            if incm:
                pc2, _ = self.prev_char(i)
                if pc2 not in (';', '{', '}', ')', ''):
                    self.manual_site(i, 'postfix ++/-- used as a value', k + 2)
                    return None
                self.count += 1
                repl = ('({ int _lx = (%s), _ly = (%s); struct rm _lr = '
                        'lev_get(_lx, _ly); _lr.%s%s; lev_put(_lx, _ly, _lr); '
                        '_lr.%s; })' % (X, Y, field, incm.group(0), field))
                return (repl, k + 2)
            self.count += 1
            return ('lev_get(%s, %s)' % (X, Y), cell_end)
        am = ASSIGN.match(s, j)
        if am and not s.startswith('==', j):
            if am.group(1) != '=':
                self.manual_site(i, 'compound assignment of a whole cell',
                                 am.end())
                return None
            pc2, _ = self.prev_char(i)
            if pc2 not in (';', '{', '}', ')', ''):
                self.manual_site(i, 'whole-cell write used as a value', am.end())
                return None
            rs = am.end()
            while rs < len(s) and s[rs] in ' \t\r\n':
                rs += 1
            re_ = self.expr_end(rs)
            rhs = self.rewrite_text(rs, re_)
            self.count += 1
            repl = ('({ __auto_type _lv = (%s); lev_put(%s, %s, _lv); })'
                    % (rhs, X, Y))
            return (repl, re_)
        self.count += 1
        return ('lev_get(%s, %s)' % (X, Y), cell_end)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    total_manual = []
    for path in a.files:
        with open(path, 'rb') as f:
            raw = f.read()
        text = raw.decode('latin-1')
        rw = Rewriter(text, path)
        new = rw.run()
        if not a.dry_run and new != text:
            with open(path, 'wb') as f:
                f.write(new.encode('latin-1'))
        print('%s: %d accesses rewritten, %d manual' %
              (path, rw.count, len(rw.manual)))
        for line, reason, snip in rw.manual:
            total_manual.append((path, line, reason, snip))
    for path, line, reason, snip in total_manual:
        print('  MANUAL %s:%d: %s: %s' % (path, line, reason, snip[:90]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
