#!/usr/bin/env python3
"""Find the accesses to struct fields that the RP6502 port moves to XRAM.

    farscan.py [--struct NAME]... [--list] FILE.c...

Parses each file with libclang (pip install libclang) with the RP6502
configuration and reports every member access whose base has one of the
given struct types (default: permonst, objclass): the field, whether it is
read, written or has its address taken, and where it is spelled (in the .c
file or in a macro in a header).
"""
import collections
import ctypes
import os
import re
import sys

import clang.cindex as ci

SRC = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..',
                                   '..'))
ARGS = ['-DRP6502', '-DRP6502_HOST', '-I' + os.path.join(SRC, 'include'),
        '-I' + os.path.join(SRC, 'build', 'host', 'gen', 'include'),
        '-Wno-everything']
for _d in ('/usr/lib/gcc/aarch64-linux-gnu/14/include',
           '/usr/lib/gcc/x86_64-linux-gnu/14/include'):
    if os.path.isdir(_d):
        ARGS.append('-isystem' + _d)
K = ci.CursorKind
_lib = ci.conf.lib
for _f in ('clang_getCursorBinaryOperatorKind',
           'clang_getCursorUnaryOperatorKind'):
    getattr(_lib, _f).argtypes = [ci.Cursor]
    getattr(_lib, _f).restype = ctypes.c_int
for _f in ('clang_getBinaryOperatorKindSpelling',
           'clang_getUnaryOperatorKindSpelling'):
    getattr(_lib, _f).argtypes = [ctypes.c_int]
    getattr(_lib, _f).restype = ci._CXString
    getattr(_lib, _f).errcheck = ci._CXString.from_result
BINOP = {i: _lib.clang_getBinaryOperatorKindSpelling(i) for i in range(1, 34)}
UNOP = {i: _lib.clang_getUnaryOperatorKindSpelling(i) for i in range(1, 15)}


def binop(cur):
    return BINOP.get(_lib.clang_getCursorBinaryOperatorKind(cur))


def unop(cur):
    return UNOP.get(_lib.clang_getCursorUnaryOperatorKind(cur))


def base_struct(cur):
    """The struct name of the base of a MEMBER_REF_EXPR, or None."""
    kids = list(cur.get_children())
    if not kids:
        return None
    t = kids[0].type.get_canonical()
    if t.kind == ci.TypeKind.POINTER:
        t = t.get_pointee().get_canonical()
    if t.kind == ci.TypeKind.RECORD:
        return t.spelling.replace('struct ', '').replace('const ', '')
    return None


ASSIGN_OPS = {'=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=',
              '>>='}


def usage(parents, cur):
    """'read', 'write' or 'addr' for the member expression cur."""
    child = cur
    for p in reversed(parents):
        if p.kind == K.PAREN_EXPR:
            child = p
            continue
        if p.kind == K.MEMBER_REF_EXPR or p.kind == K.ARRAY_SUBSCRIPT_EXPR:
            # x.f.g or x.f[i]: the use of the whole decides, unless f is a
            # pointer (then x.f itself is only read)
            if child.type.get_canonical().kind == ci.TypeKind.POINTER:
                return 'read'
            child = p
            continue
        if p.kind == K.UNARY_OPERATOR:
            op = unop(p)
            if op == '&':
                return 'addr'
            if op in ('++', '--'):
                return 'write'
            return 'read'
        if p.kind == K.BINARY_OPERATOR or p.kind == K.COMPOUND_ASSIGNMENT_OPERATOR:
            kids = list(p.get_children())
            if kids and kids[0] == child and binop(p) in ASSIGN_OPS:
                return 'write'
            return 'read'
        if p.kind == K.UNEXPOSED_EXPR:
            child = p
            continue
        return 'read'
    return 'read'


IDENT = re.compile(r'[A-Za-z_]\w*')
_src = {}


def source(path):
    if path not in _src:
        with open(path, errors='replace') as f:
            _src[path] = f.read()
    return _src[path]


TABLES = {'mons': 'permonst', 'objects': 'objclass'}


def record_of(t):
    t = t.get_canonical()
    if t.kind == ci.TypeKind.RECORD:
        return t.spelling.replace('struct ', '').replace('const ', '')
    return None


def oddity(cur, parents, structs):
    """Why cur is a use of a far table that accessors do not cover."""
    p = [x for x in parents if x.kind not in (K.PAREN_EXPR,
                                                K.UNEXPOSED_EXPR)]
    if cur.kind == K.DECL_REF_EXPR and TABLES.get(cur.spelling) in structs:
        if p and p[-1].kind == K.ARRAY_SUBSCRIPT_EXPR:
            if len(p) > 1 and p[-2].kind == K.MEMBER_REF_EXPR:
                return None
            if len(p) > 1 and p[-2].kind == K.UNARY_OPERATOR and \
                    unop(p[-2]) == '&':
                return None
            return '%s[] used as a value' % cur.spelling
        if p and p[-1].kind == K.BINARY_OPERATOR:
            return None  # mons + n, ptr - mons, comparisons
        return '%s used as a pointer' % cur.spelling
    if cur.kind == K.MEMBER_REF_EXPR and base_struct(cur) in structs:
        if cur.type.get_canonical().kind == ci.TypeKind.CONSTANTARRAY:
            if not (p and p[-1].kind == K.ARRAY_SUBSCRIPT_EXPR):
                return 'array field %s decays to a pointer' % cur.spelling
        if record_of(cur.type) and not (p and p[-1].kind in
                                        (K.MEMBER_REF_EXPR,)):
            return 'struct field %s used whole' % cur.spelling
    if cur.kind == K.UNARY_OPERATOR and unop(cur) == '*':
        kids = list(cur.get_children())
        if kids:
            t = kids[0].type.get_canonical()
            if t.kind == ci.TypeKind.POINTER and \
                    record_of(t.get_pointee()) in structs:
                return 'whole struct %s dereferenced' % \
                    record_of(t.get_pointee())
    return None


def scan(path, structs, out, odd=None):
    idx = ci.Index.create()
    tu = idx.parse(path, args=ARGS)
    for d in tu.diagnostics:
        if d.severity >= ci.Diagnostic.Error:
            print('%s: %s' % (path, d), file=sys.stderr)

    def walk(cur, parents):
        if odd is not None:
            why = oddity(cur, parents, structs)
            if why:
                loc = cur.location
                odd.append('%s:%d: %s' % (
                    os.path.relpath(loc.file.name if loc.file else '?', SRC),
                    loc.line, why))
        if cur.kind == K.MEMBER_REF_EXPR:
            s = base_struct(cur)
            if s in structs:
                loc = cur.location
                fname = loc.file.name if loc.file else '?'
                text = source(fname)
                m = IDENT.match(text, loc.offset)
                word = m.group(0) if m else '?'
                # In a macro expansion the location is the macro's name.
                macro = '' if word == cur.spelling else word
                out.append((s, cur.spelling, usage(parents, cur),
                            os.path.relpath(fname, SRC), loc.line,
                            macro))
        parents.append(cur)
        for c in cur.get_children():
            if c.location.file is None or c.location.file.name == path \
                    or c.kind != K.FUNCTION_DECL:
                walk(c, parents)
        parents.pop()

    walk(tu.cursor, [])


def main():
    argv = sys.argv[1:]
    structs = set()
    listing = False
    files = []
    while argv:
        a = argv.pop(0)
        if a == '--struct':
            structs.add(argv.pop(0))
        elif a == '--list':
            listing = True
        elif a == '--odd':
            listing = 'odd'
        else:
            files.append(os.path.abspath(a))
    structs = structs or {'permonst', 'objclass'}
    out = []
    odd = [] if listing == 'odd' else None
    for f in files:
        scan(f, structs, out, odd)
    if odd is not None:
        for line in sorted(set(odd)):
            print(line)
        return
    if listing:
        for r in out:
            print('\t'.join(map(str, r)))
        return
    by = collections.Counter((s, f, u) for s, f, u, _, _, _ in out)
    where = collections.Counter((s, bool(mac)) for s, _, _, _, _, mac
                                in out)
    macros = collections.Counter((s, mac) for s, _, _, _, _, mac in out
                                 if mac)
    for s in sorted(structs):
        print('struct %s: %d direct, %d in macros' %
              (s, where[s, False], where[s, True]))
        print('  macros: ' + ' '.join('%s:%d' % (m, n) for (t, m), n in
                                      macros.most_common() if t == s))
        fields = sorted({f for (t, f, u) in by if t == s},
                        key=lambda f: -sum(by[s, f, u] for u in
                                           ('read', 'write', 'addr')))
        for f in fields:
            print('  %-16s read %5d  write %4d  addr %4d' %
                  (f, by[s, f, 'read'], by[s, f, 'write'], by[s, f, 'addr']))


if __name__ == '__main__':
    main()
