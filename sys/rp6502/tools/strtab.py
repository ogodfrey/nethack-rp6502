#!/usr/bin/env python3
"""Rewrite the string literals of NetHack's C sources to MSG(id, "text") calls.

    strtab.py SRCROOT OUTDIR LISTFILE

LISTFILE names the sources to rewrite, relative to SRCROOT. Each one is
written to OUTDIR/<same path> with a leading #include "msg.h". Literals in
function bodies become MSG(id, "text"). Identical strings share an id. The
strings are written to OUTDIR/msg.dat (each with its NUL) and their offsets
to OUTDIR/msg.idx (little-endian uint32, one more entry than strings). The
run-time reader is sys/rp6502/msgfetch.c.

Literals that must stay constants are left alone: file-scope and static
initializers (tables), char-array initializers, #define bodies, and #if 0
code. Those are reported in the summary.
"""

import os
import re
import struct
import sys

IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
NUMBER = re.compile(r'[0-9][0-9A-Za-z_.]*')
SIMPLE_ESCAPE = {'n': 10, 't': 9, 'r': 13, 'a': 7, 'b': 8, 'f': 12, 'v': 11,
                 '\\': 92, '"': 34, "'": 39, '?': 63}


def decode(body):
    """Bytes of a C string literal body (the text between the quotes)."""
    out = bytearray()
    i = 0
    while i < len(body):
        c = body[i]
        if c != '\\':
            out.append(ord(c))
            i += 1
            continue
        i += 1
        e = body[i]
        if e in '01234567':
            v, k = 0, 0
            while i < len(body) and k < 3 and body[i] in '01234567':
                v = v * 8 + int(body[i])
                i += 1
                k += 1
            out.append(v & 0xFF)
        elif e == 'x':
            i += 1
            v = 0
            while i < len(body) and body[i] in '0123456789abcdefABCDEF':
                v = v * 16 + int(body[i], 16)
                i += 1
            out.append(v & 0xFF)
        elif e in SIMPLE_ESCAPE:
            out.append(SIMPLE_ESCAPE[e])
            i += 1
        else:
            raise ValueError('unknown escape \\%s in literal %r' % (e, body))
    return bytes(out)


def scan_quoted(text, i, quote):
    """Index just past the quoted run that starts at text[i]."""
    j = i + 1
    while True:
        c = text[j]
        if c == '\\':
            j += 2
        elif c == quote:
            return j + 1
        elif c == '\n':
            raise ValueError('unterminated literal at offset %d' % i)
        else:
            j += 1


def skip_space(text, i):
    """Index past whitespace and comments starting at text[i]."""
    n = len(text)
    while i < n:
        if text[i] in ' \t\r\n\f\v':
            i += 1
        elif text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            break
    return i


def directive_end(text, i):
    """Index of the newline that ends the preprocessor line at text[i].

    Comments may run across lines inside a directive, and a backslash at the
    end of a line continues the directive.
    """
    n = len(text)
    pos = i
    while True:
        k = pos
        while k < n and text[k] != '\n':
            if text.startswith('/*', k):
                e = text.find('*/', k + 2)
                k = n if e < 0 else e + 2
            else:
                k += 1
        if k >= n:
            return n
        back = text[k - 1] if k > 0 else ''
        if back == '\\' or (back == '\r' and k > 1 and text[k - 2] == '\\'):
            pos = k + 1
            continue
        return k


def directive_code(line):
    """The directive text with comments removed."""
    code = re.sub(r'/\*.*?\*/', ' ', line, flags=re.S)
    code = re.sub(r'//.*', '', code)
    return code.strip()


class Table:
    def __init__(self):
        self.ids = {}
        self.strings = []

    def id_for(self, data):
        if data not in self.ids:
            self.ids[data] = len(self.strings)
            self.strings.append(data)
        return self.ids[data]


class Stats:
    def __init__(self):
        self.rewritten = 0
        self.kept = {}
        self.kept_bytes = 0

    def keep(self, why, nbytes):
        self.kept[why] = self.kept.get(why, 0) + 1
        self.kept_bytes += nbytes


def rewrite(text, table, stats):
    out = []
    n = len(text)
    i = 0
    at_bol = True
    skipping = 0          # depth of #if 0 nesting while skipping dead code
    skip_on = False
    stack = []            # brace kinds: 'func', 'init', 'localinit', 'block'
    stmt_static = False   # inside a statement that began with 'static'
    stmt_bracket = False  # the statement so far has a '[' (array declarator)
    stmt_eq = False       # file scope: this statement has an '='
    stmt_struct = False   # file scope: this statement has struct/union/enum
    cond = []             # braces open at each #if, restored at #else/#elif
    last = ''             # last significant token

    while i < n:
        c = text[i]
        if c == '\n':
            out.append(c)
            i += 1
            at_bol = True
            continue
        if c in ' \t\r\f\v':
            out.append(c)
            i += 1
            continue
        if text.startswith('//', i) or text.startswith('/*', i):
            j = skip_space(text, i)
            out.append(text[i:j])
            i = j
            continue
        if c == '#' and at_bol:
            end = directive_end(text, i)
            line = text[i:end]
            code = directive_code(line)
            name = re.match(r'#\s*(\w+)', code)
            word = name.group(1) if name else ''
            if skip_on:
                if word in ('if', 'ifdef', 'ifndef'):
                    skipping += 1
                elif word == 'endif':
                    if skipping == 0:
                        skip_on = False
                        cond.pop()
                    else:
                        skipping -= 1
                elif word in ('else', 'elif') and skipping == 0:
                    skip_on = False
            else:
                # Each branch of a conditional starts from the braces open at its
                # #if, since #ifdef blocks often open a brace in one branch only.
                if word in ('if', 'ifdef', 'ifndef'):
                    cond.append(list(stack))
                    if re.match(r'#\s*if\s+0$', code):
                        skip_on = True
                        skipping = 0
                elif word in ('else', 'elif'):
                    if cond:
                        stack[:] = cond[-1]
                elif word == 'endif':
                    if cond:
                        cond.pop()
            out.append(line)
            i = end
            continue
        at_bol = False
        if skip_on:
            out.append(c)
            i += 1
            continue
        if c == '"':
            start = i
            j = scan_quoted(text, i, '"')
            parts = [text[i + 1:j - 1]]
            k = skip_space(text, j)
            while k < n and text[k] == '"':
                e = scan_quoted(text, k, '"')
                parts.append(text[k + 1:e - 1])
                j = e
                k = skip_space(text, j)
            body = ''.join(parts)
            # A backslash at the end of a line splices the line into the literal.
            body = body.replace('\\\r\n', '').replace('\\\n', '')
            if not stack:
                why = 'file-scope'
            elif 'init' in stack:
                why = 'static-initializer'
            elif stmt_static:
                why = 'static-local'
            elif 'func' not in stack:
                why = 'outside-function'
            elif last == '=' and stmt_bracket:
                why = 'char-array-initializer'
            else:
                why = None
            if why is None:
                out.append('MSG(%d, "%s")' % (table.id_for(decode(body)), body))
                stats.rewritten += 1
            else:
                out.append(text[start:j])
                stats.keep(why, len(body) + 1)
            i = j
            last = 'STR'
            continue
        if c == "'":
            j = scan_quoted(text, i, "'")
            out.append(text[i:j])
            i = j
            last = 'CHR'
            continue
        m = IDENT.match(text, i)
        if m:
            word = m.group(0)
            out.append(word)
            i = m.end()
            if word == 'static':
                stmt_static = True
            if not stack and word in ('struct', 'union', 'enum'):
                stmt_struct = True
            last = word
            continue
        m = NUMBER.match(text, i)
        if m:
            out.append(m.group(0))
            i = m.end()
            last = 'NUM'
            continue
        # Operators: '==' and the assignment forms are one token each.
        if c == '=' and text.startswith('==', i):
            out.append('==')
            i += 2
            last = '=='
            continue
        if c in '+-*/%&|^!<>' and (text.startswith('<<=', i) or text.startswith('>>=', i)):
            out.append(text[i:i + 3])
            i += 3
            last = 'op='
            continue
        if c in '+-*/%&|^!<>' and i + 1 < n and text[i + 1] == '=':
            out.append(text[i:i + 2])
            i += 2
            last = 'op='
            continue
        if c == '{':
            if not stack:
                # A file-scope '{' opens a type or initializer when its statement
                # has '=' or struct/union/enum; otherwise it opens a function body
                # (K&R parameter declarations end in ';' before the body).
                kind = 'init' if (stmt_eq or stmt_struct) else 'func'
            elif stmt_static or 'init' in stack:
                kind = 'init'
            elif last == '=':
                kind = 'localinit'
            else:
                kind = 'block'
            stack.append(kind)
        elif c == '}':
            if stack:
                stack.pop()
            if not stack:
                stmt_static = False
                stmt_bracket = False
                stmt_eq = False
                stmt_struct = False
        elif c == ';':
            # A ';' inside a static or initializer body (a struct definition)
            # ends a member, not the statement.
            if not stack or stack[-1] in ('func', 'block', 'localinit'):
                stmt_static = False
                stmt_bracket = False
                if not stack:
                    stmt_eq = False
                    stmt_struct = False
        elif c == '[':
            stmt_bracket = True
        elif c == '=' and not stack:
            stmt_eq = True
        out.append(c)
        i += 1
        last = c
    return ''.join(out)


def write_tables(outdir, table):
    dat = bytearray()
    idx = []
    for s in table.strings:
        idx.append(len(dat))
        dat += s + b'\0'
    idx.append(len(dat))
    with open(os.path.join(outdir, 'msg.dat'), 'wb') as f:
        f.write(dat)
    with open(os.path.join(outdir, 'msg.idx'), 'wb') as f:
        f.write(b''.join(struct.pack('<I', v) for v in idx))


def main(argv):
    if len(argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    srcroot, outdir, listfile = argv[1:]
    with open(listfile) as f:
        names = [l.strip() for l in f if l.strip() and not l.startswith('#')]
    table = Table()
    stats = Stats()
    for name in names:
        with open(os.path.join(srcroot, name), 'rb') as f:
            text = f.read().decode('latin-1')
        try:
            new = rewrite(text, table, stats)
        except (ValueError, IndexError) as e:
            raise SystemExit('strtab: %s: %s' % (name, e))
        dest = os.path.join(outdir, name)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, 'wb') as f:
            f.write(('#include "msg.h"\n' + new).encode('latin-1'))
    write_tables(outdir, table)
    print('files: %d  rewritten literals: %d  distinct strings: %d  table bytes: %d'
          % (len(names), stats.rewritten, len(table.strings),
             sum(len(s) + 1 for s in table.strings)))
    for why in sorted(stats.kept):
        print('  kept %-24s %d' % (why, stats.kept[why]))
    print('  kept literal bytes (stay in RAM): %d' % stats.kept_bytes)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
