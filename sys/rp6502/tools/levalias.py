#!/usr/bin/env python3
"""Replace a local pointer to a level cell by the cell itself.

    struct rm *lev = &levl[x][y];      (or  lev = &levl[x][y];)
    ... lev->typ ...

becomes  ... levl[x][y].typ ...  (levrw.py then turns that into lev_get and
lev_put). The change is made only where it is exact:
  - every use of the pointer in its range is "P->field";
  - the index expressions are not changed in the range (x, y, u.ux, ...);
  - the range runs from an assignment to the next assignment of P in the
    function, or to the end of the block that holds the assignment.
A pointer with any other use (passed to a function, compared, incremented,
"*P", "P += ROWNO") is reported and left alone, and so is a declaration with
several declarators. The masked copy of the source (comments, literals and
preprocessor lines blanked) keeps matches out of those places.

Usage: levalias.py [--dry-run] FILE...
"""
import argparse
import re
import sys

ASSIGN = re.compile(r'\b(\w+)\s*=\s*&\s*\(?\s*levl\s*\[\s*([^\[\];]+?)\s*\]\s*'
                    r'\[\s*([^\[\];]+?)\s*\]\s*\)?\s*;')
DECL_PREFIX = re.compile(r'(?:register\s+)?struct\s+rm\s*\*\s*$')


def mask(s):
    """Same-length copy with comments, literals and directives blanked."""
    out = list(s)
    n = len(s)

    def blank(a, b):
        for k in range(a, b):
            if out[k] != '\n':
                out[k] = ' '

    i = 0
    while i < n:
        if s.startswith('/*', i):
            j = s.find('*/', i + 2)
            j = n if j < 0 else j + 2
            blank(i, j)
            i = j
        elif s.startswith('//', i):
            j = s.find('\n', i)
            j = n if j < 0 else j
            blank(i, j)
            i = j
        elif s[i] in '"\'':
            q = s[i]
            j = i + 1
            while j < n and s[j] != q and s[j] != '\n':
                j += 2 if s[j] == '\\' else 1
            j = min(j + 1, n)
            blank(i, j)
            i = j
        elif s[i] == '#' and s[s.rfind('\n', 0, i) + 1:i].strip() == '':
            j = i
            while True:
                k = s.find('\n', j)
                if k < 0:
                    k = n
                    break
                if s[k - 1] == '\\' or s[k - 2:k] == '\\\r':
                    j = k + 1
                    continue
                break
            blank(i, k)
            i = k
        else:
            i += 1
    return ''.join(out)


def braces(m):
    """(open, close) index pairs of the braces in masked text m."""
    stack, pairs = [], []
    for i, ch in enumerate(m):
        if ch == '{':
            stack.append(i)
        elif ch == '}' and stack:
            pairs.append((stack.pop(), i))
    return pairs


def innermost(pairs, p):
    best = None
    for o, c in pairs:
        if o < p < c and (best is None or o > best[0]):
            best = (o, c)
    return best


def outermost(pairs, p):
    best = None
    for o, c in pairs:
        if o < p < c and (best is None or o < best[0]):
            best = (o, c)
    return best


def changes(text, name):
    """True if the index expression name may change in text."""
    base = re.escape(name)
    pats = [r'(?<![\w.])%s\s*(?:[-+*/%%&|^]|<<|>>)?=(?!\s*=)' % base,
            r'(?:\+\+|--)\s*%s(?![\w])' % base,
            r'(?<![\w.])%s\s*(?:\+\+|--)' % base,
            r'(?<![&\w])&(?!&)\s*%s(?![\w])' % base]
    return any(re.search(p, text) for p in pats)


def process(text):
    m = mask(text)
    pairs = braces(m)
    edits = {}      # start -> (end, replacement)
    manual = []     # (line, reason)
    handled = set()
    for am in ASSIGN.finditer(m):
        P, X, Y = am.group(1), am.group(2).strip(), am.group(3).strip()
        fn = outermost(pairs, am.start())
        if fn is None or (fn, P) in handled:
            continue
        handled.add((fn, P))
        body = m[fn[0]:fn[1] + 1]
        decl_re = re.compile(r'(?:register\s+)?struct\s+rm\s*\*\s*%s\b' % re.escape(P))
        if not decl_re.search(body):
            continue
        line = text.count('\n', 0, am.start()) + 1
        allas = [a for a in ASSIGN.finditer(body) if a.group(1) == P]
        if len(list(decl_re.finditer(body))) != 1:
            manual.append((line, 'pointer %s declared more than once' % P))
            continue
        # Each use of P must be inside a declaration, an assignment, or be
        # "P->field" inside a range that one of the assignments governs.
        decl_spans = [(d.start(), d.end()) for d in decl_re.finditer(body)]
        assign_spans = [(a.start(), a.end()) for a in allas]
        bad = None
        for mo in re.finditer(r'\b%s\b' % re.escape(P), body):
            if any(s <= mo.start() < e for s, e in decl_spans + assign_spans):
                continue
            if re.match(r'\s*->\s*\w', body[mo.end():]):
                continue
            bad = text.count('\n', 0, fn[0] + mo.start()) + 1
            break
        if bad is not None:
            manual.append((line, 'pointer %s used other than P->field (line %d)'
                           % (P, bad)))
            continue
        # Ranges where each assignment's index names hold.
        ranges = []
        for k, a in enumerate(allas):
            blk = innermost(pairs, fn[0] + a.start())
            end = blk[1] if blk else fn[1]
            if k + 1 < len(allas):
                end = min(end, fn[0] + allas[k + 1].start())
            ranges.append((fn[0] + a.start(), fn[0] + a.end(), end, a))
        ok = True
        for a0, a1, end, a in ranges:
            rng = m[a1:end]
            ax, ay = a.group(2).strip(), a.group(3).strip()
            if changes(rng, ax) or changes(rng, ay):
                manual.append((line, 'index %s or %s changes while %s is used'
                               % (ax, ay, P)))
                ok = False
                break
        if not ok:
            continue
        for mo in re.finditer(r'\b%s\s*->' % re.escape(P), body):
            pos = fn[0] + mo.start()
            if not any(a1 <= pos < end for a0, a1, end, a in ranges):
                manual.append((line, 'pointer %s used before an assignment'
                               % P))
                ok = False
                break
        if not ok:
            continue
        for a0, a1, end, a in ranges:
            ax, ay = a.group(2).strip(), a.group(3).strip()
            for mo in re.finditer(r'\b%s\s*->\s*(\w+)' % re.escape(P), m[a1:end]):
                s, e = a1 + mo.start(), a1 + mo.end()
                edits[s] = (e, 'levl[%s][%s].%s' % (ax, ay, mo.group(1)))
            # The assignment (or the whole declaration with it) goes.
            pre = text[:a0]
            dm = DECL_PREFIX.search(pre)
            if dm:
                start = dm.start()
            else:
                start = a0
            edits[start] = (a1, '')
        # Declarations without an initializer that are now unused.
        for dmo in decl_re.finditer(body):
            ds = fn[0] + dmo.start()
            semi = m.find(';', fn[0] + dmo.end())
            stmt = m[ds:semi + 1]
            if ',' in stmt:
                manual.append((line, 'declaration of %s has more declarators' % P))
                continue
            if ds not in edits and not any(s <= ds < e for s, (e, _) in edits.items()):
                edits[ds] = (semi + 1, '')
    # Apply the edits from the end; drop any edit inside an earlier one.
    out = text
    last = None
    for s in sorted(edits, reverse=True):
        e, rep = edits[s]
        if last is not None and e > last:
            continue
        out = out[:s] + rep + out[e:]
        last = s
    return out, manual, len(edits)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    for path in a.files:
        with open(path, 'rb') as f:
            text = f.read().decode('latin-1')
        new, manual, n = process(text)
        if new != text and not a.dry_run:
            with open(path, 'wb') as f:
                f.write(new.encode('latin-1'))
        print('%s: %d edits, %d manual' % (path, n, len(manual)))
        for line, reason in manual:
            print('  MANUAL %s:%d: %s' % (path, line, reason))
    return 0


if __name__ == '__main__':
    sys.exit(main())
