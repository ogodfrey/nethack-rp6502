#!/usr/bin/env python3
"""Overlay packer for the RP6502 NetHack port.

Reads the llvm-mos assembly of every module (clang -S -ffunction-sections
-fdata-sections -fno-lto), splits it into items (one function with its jump
tables, or one data object), and rewrites it into a resident program plus
overlays that run in the slots managed by sys/rp6502/ovl/ovlrt.s.

    ovlpack.py stats ASMDIR [CALLGRAPH]
    ovlpack.py plan  ASMDIR CALLGRAPH OUTDIR [options]
    ovlpack.py emit  ASMDIR PLAN.json OUTDIR
    ovlpack.py link  OUTDIR [resident link inputs]

See ovlrt.s for the runtime design.  Symbols that are local to a module
(static functions and variables, .L labels) are renamed so that items from
different modules can be put together in one assembly file:

    static foo  ->  __l.MODULE.foo         (made global in resident code)
    .LBB0_3     ->  .LMODULE.BB0_3
"""
import collections
import json
import os
import re
import sys

# --------------------------------------------------------------------------
# Parsing


IDENT = re.compile(r'(?<![\w.$@])([A-Za-z_.][\w.$]*)')
ZP_RE = re.compile(r'\s*\.zeropage\s+(\S+)')
SECTION_RE = re.compile(r'\s*\.section\s+("[^"]*"|[^,\s]+)\s*(,.*)?$')
ALIAS_RE = re.compile(r'^([A-Za-z_.][\w.$]*)\s*=\s*(.*)$')
LABEL_RE = re.compile(r'^([A-Za-z_.][\w.$]*):(.*)$')
DIRECTIVE_RE = re.compile(r'^\s*(\.[\w.]+)\s*(.*)$')
INTERNAL_LABEL = re.compile(r'^\.L(BB|tmp|func_end|JTI|_MOS)')


def split_comment(line):
    """Return (code, comment) with string literals respected."""
    in_str = False
    i = 0
    while i < len(line):
        c = line[i]
        if in_str:
            if c == '\\':
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == ';':
            return line[:i].rstrip(), line[i:]
        i += 1
    return line.rstrip(), ''


def subst_idents(code, fn):
    """Apply fn to every identifier in code outside string literals.

    The first token (mnemonic or directive) is left alone."""
    m = re.match(r'(\s*\S+)(.*)$', code)
    if not m:
        return code
    head, rest = m.group(1), m.group(2)
    if LABEL_RE.match(code.strip()):
        return code
    out = []
    parts = re.split(r'("(?:[^"\\]|\\.)*")', rest)

    def one(mm, p):
        name = mm.group(1)
        if name in OPERATORS:
            return name
        if name in ('x', 'y', 'X', 'Y') and \
                p[:mm.start()].rstrip().endswith(','):
            return name
        return fn(name)

    for i, p in enumerate(parts):
        if i % 2:
            out.append(p)
        else:
            out.append(IDENT.sub(lambda mm, p=p: one(mm, p), p))
    return head + ''.join(out)


OPERATORS = {'mos16lo', 'mos16hi', 'mos8', 'mos16', 'mos24', 'mos24bank',
             'mos24segment', 'mos24lo', 'mos24hi', 'mos13', 'mos24segmentlo',
             'mos24segmenthi'}


def idents(code):
    found = []
    subst_idents(code, lambda s: found.append(s) or s)
    return found


class Item:
    """One function (with its jump tables) or one data object."""

    def __init__(self, mod, name, kind):
        self.mod = mod
        self.name = name        # original name in its module
        self.kind = kind        # text, rodata, data, bss, zp
        self.blocks = []        # [section header, [lines]]
        self.size = 0
        self.glob = False
        self.refs = set()       # original names referenced
        self.calls = set()      # subset of refs used only as jsr/jmp target
        self.addr_refs = set()  # refs used other than as jsr/jmp target
        self.gname = name       # global (renamed) name
        self.merge = False      # in a mergeable (string) section

    def key(self):
        return self.gname

    def __repr__(self):
        return 'Item(%s %s %d)' % (self.kind, self.gname, self.size)


def section_kind(name, flags):
    n = name.strip('"')
    fl = flags.split(',')[1].strip('"') if flags.count(',') >= 1 else ''
    if n.startswith('.text') or 'x' in fl:
        return 'text'
    if n.startswith('.zp'):
        return 'zp'
    if n.startswith('.rodata'):
        return 'rodata'
    if n.startswith('.bss') or n.startswith('.noinit'):
        return 'bss'
    if n.startswith('.data'):
        return 'data'
    if n.startswith('.init_array') or n.startswith('.fini_array'):
        return 'data'
    raise ValueError('unknown section %s' % name)


TRAILER = ('.ident', '.addrsig', '.addrsig_sym', '.file')


class Module:
    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path)[:-2]
        self.items = []
        self.globls = set()
        self.extra_globls = set()   # .globl of CRT marker symbols
        self.defined = {}           # name -> Item
        self.parse()

    def parse(self):
        pending = []
        sect = None             # (header line, kind)
        cur = None
        with open(self.path) as f:
            lines = f.read().split('\n')
        for raw in lines:
            code, _ = split_comment(raw)
            s = code.strip()
            if not s:
                continue
            if ZP_RE.match(s):
                continue
            m = SECTION_RE.match(s)
            if m:
                nm, fl = m.group(1), (m.group(2) or '')
                if nm.strip('"') == '.note.GNU-stack':
                    sect = None
                    continue
                kind = section_kind(nm, fl)
                sect = ('\t.section\t%s%s' % (nm, fl), kind, nm.strip('"'))
                if kind == 'text':
                    # A function section: the function is the label that
                    # follows; until then lines are pending.
                    cur = None
                elif cur is not None and cur.kind == 'text':
                    cur = None
                continue
            if s in ('.text', '.data', '.bss'):
                raise ValueError('%s: bare %s' % (self.path, s))
            m = ALIAS_RE.match(s)
            if m:
                it = Item(self.name, m.group(1), 'alias')
                it.blocks.append(['', [s]])
                self.items.append(it)
                self.defined[it.name] = it
                cur = None
                continue
            m = DIRECTIVE_RE.match(s)
            if m and not LABEL_RE.match(s):
                d, arg = m.group(1), m.group(2)
                if d in TRAILER:
                    continue
                if d == '.globl':
                    if arg.startswith('__do_'):
                        self.extra_globls.add(arg)
                    else:
                        self.globls.add(arg)
                    continue
                if d == '.type':
                    pending.append(s)
                    continue
                if d == '.p2align' and (cur is None or cur.kind != 'text'):
                    pending.append(s)
                    continue
                if d == '.size':
                    sym, expr = [x.strip() for x in arg.split(',', 1)]
                    it = self.defined.get(sym)
                    if it is None:
                        raise ValueError('%s: .size of unknown %s' %
                                         (self.path, sym))
                    if expr.isdigit() and it.kind != 'alias':
                        it.size = int(expr)
                    it.blocks[-1][1].append(s)
                    if it.kind != 'text':
                        cur = None
                    continue
                if cur is None:
                    raise ValueError('%s: stray %s' % (self.path, s))
                self._add(cur, s)
                continue
            m = LABEL_RE.match(s)
            if m and not m.group(1).startswith('.LBB') and \
                    not m.group(1).startswith('.Ltmp') and \
                    not m.group(1).startswith('.Lfunc_end'):
                name = m.group(1)
                if sect is None:
                    raise ValueError('%s: label %s outside section' %
                                     (self.path, name))
                if name.startswith('.LJTI'):
                    # Jump table of the function named by the section.
                    owner = sect[2][len('.rodata.'):]
                    it = self.defined.get(owner)
                    if it is None or it.kind != 'text':
                        raise ValueError('%s: jump table %s in %s' %
                                         (self.path, name, sect[2]))
                    it.blocks.append([sect[0], []])
                    it.blocks[-1][1].extend(pending)
                    pending = []
                    it.blocks[-1][1].append(s)
                    cur = it
                    self.defined[name] = it
                    continue
                it = Item(self.name, name, sect[1])
                it.merge = ',"aMS"' in sect[0] or ',"aM"' in sect[0]
                it.blocks.append([sect[0], pending + [s]])
                pending = []
                self.items.append(it)
                self.defined[name] = it
                cur = it
                continue
            if cur is None:
                raise ValueError('%s: stray line %r' % (self.path, s))
            self._add(cur, s)
        for it in self.items:
            it.glob = it.name in self.globls
        self._refs()

    def _add(self, it, line):
        if it.kind == 'text' and it.blocks and \
                not it.blocks[-1][0].startswith('\t.section\t.text'):
            # back in the function after a jump table
            pass
        it.blocks[-1][1].append(line)

    def _refs(self):
        for it in self.items:
            labels = set()
            for _, lines in it.blocks:
                for ln in lines:
                    m = LABEL_RE.match(ln)
                    if m:
                        labels.add(m.group(1))
            for _, lines in it.blocks:
                for ln in lines:
                    if LABEL_RE.match(ln):
                        continue
                    m = re.match(r'\s*(\S+)\s+(.*)$', ln)
                    if not m:
                        continue
                    op, arg = m.group(1), m.group(2)
                    if op in ('.size', '.type'):
                        continue
                    ids = [x for x in idents(ln) if x not in labels]
                    for x in ids:
                        it.refs.add(x)
                        if op in ('jsr', 'jmp') and arg.strip() == x:
                            it.calls.add(x)
                        else:
                            it.addr_refs.add(x)


def load_modules(asmdir):
    mods = []
    for fn in sorted(os.listdir(asmdir)):
        if fn.endswith('.s'):
            mods.append(Module(os.path.join(asmdir, fn)))
    return mods


def nm_sizes(objdir, mod):
    """Function sizes from the module's object file."""
    import subprocess
    nm = os.environ.get('LLVM_NM', tool('llvm-nm'))
    if not os.path.exists(os.path.join(objdir, mod + '.o')):
        return {}
    out = subprocess.run([nm, '-S', '--defined-only',
                          os.path.join(objdir, mod + '.o')],
                         capture_output=True, text=True, check=True).stdout
    sizes = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in 'tTdDbBrR':
            sizes[p[3]] = int(p[1], 16)
    return sizes


def text_size(it):
    """Estimate of a function's size from its instructions."""
    n = 0
    for hdr, lines in it.blocks:
        for ln in lines:
            m = re.match(r'\s*(\S+)\s*(.*)$', ln)
            if not m or LABEL_RE.match(ln):
                continue
            op, arg = m.group(1), m.group(2)
            if op == '.short':
                n += 2
            elif op == '.byte':
                n += 1
            elif op.startswith('.'):
                continue
            else:
                n += insn_size(op, arg)
    return n


IMPLIED = {'rts', 'rti', 'pha', 'pla', 'phx', 'plx', 'phy', 'ply', 'php',
           'plp', 'tax', 'txa', 'tay', 'tya', 'tsx', 'txs', 'inx', 'iny',
           'dex', 'dey', 'clc', 'sec', 'cli', 'sei', 'clv', 'cld', 'sed',
           'nop', 'brk', 'wai', 'stp'}
ACC = {'asl', 'lsr', 'rol', 'ror', 'inc', 'dec'}
BRANCH = {'bcc', 'bcs', 'beq', 'bne', 'bmi', 'bpl', 'bvc', 'bvs', 'bra'}


def insn_size(op, arg):
    arg = arg.strip()
    if op in IMPLIED or (op in ACC and arg in ('', 'a')):
        return 1
    if op in BRANCH:
        return 2
    if op in ('jsr', 'jmp'):
        return 3
    if arg.startswith('#'):
        return 2
    base = arg.split(',')[0].strip('()')
    if base.startswith('__rc') or base.startswith('mos8('):
        return 2
    return 3


def cmd_stats(argv):
    asmdir = argv[0]
    objdir = os.path.join(os.path.dirname(asmdir.rstrip('/')), 'obj')
    mods = load_modules(asmdir)
    tot = collections.Counter()
    funcs = []
    permod = []
    for m in mods:
        sizes = nm_sizes(objdir, m.name)
        mc = collections.Counter()
        for it in m.items:
            if it.kind == 'text':
                jt = sum(2 * sum(1 for ln in lines if '.short' in ln)
                         for hdr, lines in it.blocks[1:])
                it.size = sizes.get(it.name, text_size(it)) + jt
                funcs.append((it.size, m.name, it.name))
            mc[it.kind] += it.size
        permod.append((mc['text'], m.name, mc))
        tot += mc
    for k in ('text', 'rodata', 'data', 'bss', 'zp'):
        print('%-7s %7d' % (k, tot[k]))
    print()
    print('largest modules (text rodata data bss):')
    for t, n, mc in sorted(permod, reverse=True)[:25]:
        print('  %-10s %6d %6d %6d %6d' % (n, t, mc['rodata'], mc['data'],
                                          mc['bss']))
    print()
    print('largest functions:')
    for s, m, n in sorted(funcs, reverse=True)[:40]:
        print('  %6d %s/%s' % (s, m, n))
    print()
    data = []
    for m in mods:
        for it in m.items:
            if it.kind in ('data', 'bss', 'rodata'):
                data.append((it.size, it.kind, m.name, it.name))
    print('largest data objects:')
    for s, k, m, n in sorted(data, reverse=True)[:40]:
        print('  %6d %-6s %s/%s' % (s, k, m, n))


# --------------------------------------------------------------------------
# Emitting the resident program and the overlays

ZP_HEADER = ''.join('\t.zeropage\t__rc%d\n' % i for i in range(32))
RESIDENT = 'R'


class Program:
    """All modules, with global names and a unit for every item."""

    def __init__(self, mods):
        self.mods = mods
        self.gdef = {}
        self.items = []
        self.crt = set()
        for m in mods:
            self.crt |= m.extra_globls
            for it in m.items:
                if it.glob:
                    it.gname = it.name
                    if it.name in self.gdef:
                        raise ValueError('%s defined in %s and %s' % (
                            it.name, self.gdef[it.name].mod, m.name))
                    self.gdef[it.name] = it
                else:
                    it.gname = '__l.%s.%s' % (m.name, it.name)
                it.unit = RESIDENT
                self.items.append(it)
        self.bymod = {m.name: m for m in mods}

    def resolve(self, m, x):
        """(item, new name) for identifier x in module m; item is None for
        internal labels and symbols defined outside the program."""
        it = m.defined.get(x)
        if it is not None:
            if it.name != x:            # jump table label inside a function
                return None, '.L%s.%s' % (m.name, x[2:])
            return it, it.gname
        if x.startswith('.L'):
            return None, '.L%s.%s' % (m.name, x[2:])
        it = self.gdef.get(x)
        if it is not None:
            return it, it.gname
        return None, x

    def find(self, spec):
        """Items named by 'module/name', 'module/*' or a global name."""
        if '/' in spec:
            mod, name = spec.split('/', 1)
            m = self.bymod.get(mod)
            if m is None:
                raise ValueError('no module %s' % mod)
            if name == '*':
                return [it for it in m.items if it.kind == 'text']
            it = m.defined.get(name)
            if it is None or it.kind != 'text':
                raise ValueError('no function %s' % spec)
            return [it]
        it = self.gdef.get(spec)
        if it is None or it.kind != 'text':
            raise ValueError('no function %s' % spec)
        return [it]


def assign_rodata(prog):
    """Rodata stays resident if resident code or data refers to it (or to
    rodata that refers to it); other rodata is copied into each overlay that
    refers to it."""
    work = [it for it in prog.items if it.unit == RESIDENT
            and it.kind != 'rodata']
    for it in prog.items:
        if it.kind == 'rodata':
            it.unit = None
    seen = set()
    while work:
        it = work.pop()
        if id(it) in seen:
            continue
        seen.add(id(it))
        m = prog.bymod[it.mod]
        for x in it.refs:
            t, _ = prog.resolve(m, x)
            if t is not None and t.kind == 'rodata' and t.unit is None:
                t.unit = RESIDENT
                work.append(t)
    return


def rodata_closure(prog, items):
    out = []
    seen = set(id(it) for it in items)
    work = list(items)
    while work:
        it = work.pop()
        m = prog.bymod[it.mod]
        for x in sorted(it.refs):
            t, _ = prog.resolve(m, x)
            if t is not None and t.kind == 'rodata' and t.unit is None \
                    and id(t) not in seen:
                seen.add(id(t))
                out.append(t)
                work.append(t)
    return out


class Emitter:
    def __init__(self, prog, cfg):
        self.prog = prog
        self.cfg = cfg
        self.desc = {}          # id(item) -> descriptor index
        self.desc_items = []
        self.thunks = {}        # id(item) -> item
        self.undef = set()      # resident symbols the overlays need

    def desc_addr(self, t):
        k = id(t)
        if k not in self.desc:
            self.desc[k] = len(self.desc_items)
            self.desc_items.append(t)
        return self.cfg['desc_xram'] + 3 * self.desc[k]

    def thunk(self, t):
        self.thunks[id(t)] = t
        self.desc_addr(t)
        return '__ovt_' + t.gname

    def rewrite(self, it, unit, local_names):
        """Lines of item it, rewritten for the unit it is emitted in."""
        prog = self.prog
        m = prog.bymod[it.mod]
        out = []
        for hdr, lines in it.blocks:
            if hdr:
                out.append(hdr)
            for ln in lines:
                lm = LABEL_RE.match(ln)
                if lm:
                    _, nm = prog.resolve(m, lm.group(1))
                    out.append('%s:' % nm)
                    continue
                am = ALIAS_RE.match(ln)
                if am:
                    _, nm = prog.resolve(m, am.group(1))
                    rhs = subst_idents('= ' + am.group(2),
                                       lambda x: self.ref(m, it, unit, x,
                                                          False, local_names))
                    out.append('%s %s' % (nm, rhs))
                    continue
                om = re.match(r'\s*(\S+)\s*(.*)$', ln)
                op, arg = om.group(1), om.group(2).strip()
                if op in ('jsr', 'jmp') and IDENT.fullmatch(arg):
                    t, nm = prog.resolve(m, arg)
                    if t is not None and t.kind == 'text' and \
                            t.unit != RESIDENT and t.unit != unit:
                        out.append('\tjsr\t__ovl_far')
                        out.append('\t.short\t%d' % self.desc_addr(t))
                        if op == 'jmp':
                            out.append('\trts')
                        continue
                    out.append('\t%s\t%s' % (
                        op, self.ref(m, it, unit, arg, True, local_names)))
                    continue
                meta = op in ('.size', '.type', '.globl')
                if op in BRANCH:
                    t, _ = prog.resolve(m, arg)
                    if t is not None and t is not it:
                        raise ValueError('%s/%s: branch to %s' %
                                         (it.mod, it.name, arg))
                out.append('\t' + subst_idents(
                    ln.strip(), lambda x: self.ref(m, it, unit, x, meta,
                                                   local_names)))
        return out

    def ref(self, m, it, unit, x, direct, local_names):
        t, nm = self.prog.resolve(m, x)
        if t is None:
            if not nm.startswith('.L') and unit != RESIDENT:
                self.undef.add(nm)
            return nm
        if t.kind == 'text' and t.unit != RESIDENT and \
                not (direct and t.unit == unit):
            th = self.thunk(t)
            if unit != RESIDENT:
                self.undef.add(th)
            return th
        if unit != RESIDENT and id(t) not in local_names:
            self.undef.add(nm)
        return nm

    def emit_unit(self, items, unit, path, extra=''):
        local = set(id(it) for it in items)
        body = []
        for it in items:
            body.append('')
            if unit == RESIDENT and it.kind != 'alias':
                body.append('\t.globl\t%s' % it.gname)
            elif unit == RESIDENT:
                body.append('\t.globl\t%s' % self.prog.resolve(
                    self.prog.bymod[it.mod], it.name)[1])
            body.extend(self.rewrite(it, unit, local))
        with open(path, 'w') as f:
            f.write(ZP_HEADER)
            f.write('\n'.join(body))
            f.write('\n' + extra)


def load_plan(prog, plan):
    ovls = []
    for i, specs in enumerate(plan['overlays']):
        items = []
        for spec in specs:
            for it in prog.find(spec):
                if it.unit != RESIDENT:
                    raise ValueError('%s in two overlays' % spec)
                it.unit = i
                items.append(it)
        ovls.append(items)
    if len(ovls) > 255:
        raise ValueError('too many overlays')
    return ovls


def cmd_emit(argv):
    asmdir, planfile, out = argv[0], argv[1], argv[2]
    with open(planfile) as f:
        plan = json.load(f)
    cfg = {'desc_xram': int(str(plan.get('desc_xram', 0xA000)), 0),
           'dir_xram': int(str(plan.get('dir_xram', 0xC000)), 0)}
    prog = Program(load_modules(asmdir))
    ovls = load_plan(prog, plan)
    assign_rodata(prog)
    os.makedirs(out, exist_ok=True)
    em = Emitter(prog, cfg)
    meta = {'overlays': [], 'nslots': plan['nslots'],
            'slot_size': plan.get('slot_size', 0),
            'desc_xram': cfg['desc_xram'], 'dir_xram': cfg['dir_xram'],
            'file': plan.get('file', 'nethack.ovl')}
    for i, items in enumerate(ovls):
        ro = rodata_closure(prog, items)
        em.emit_unit(items + ro, i, os.path.join(out, 'ovl%03d.s' % i))
        meta['overlays'].append([it.gname for it in items])
    res = [it for it in prog.items if it.unit == RESIDENT]
    # Thunks are only known after every unit is rewritten, so the resident
    # unit (which may also add thunks) is emitted with them last.
    tmp = os.path.join(out, 'resident.s')
    em.emit_unit(res, RESIDENT, tmp)
    nslots = plan['nslots']
    extra = ['', '\t.section\t.text.__ovl_thunks,"ax",@progbits']
    for t in sorted(em.thunks.values(), key=lambda t: t.gname):
        extra += ['\t.globl\t__ovt_%s' % t.gname, '__ovt_%s:' % t.gname,
                  '\tjsr\t__ovl_thunk',
                  '\t.short\t%d' % (cfg['desc_xram'] + 3 * em.desc[id(t)])]
    extra += ['', '\t.globl\t__ovl_dir_xram',
              '__ovl_dir_xram = %d' % cfg['dir_xram']]
    for c in sorted(prog.crt):
        extra.append('\t.globl\t%s' % c)
    with open(tmp, 'a') as f:
        f.write('\n'.join(extra) + '\n')
    meta['desc'] = [[it.gname, it.unit] for it in em.desc_items]
    meta['undef'] = sorted(em.undef)
    with open(os.path.join(out, 'ovlmeta.json'), 'w') as f:
        json.dump(meta, f, indent=1)
    with open(os.path.join(out, 'ovlcfg.h'), 'w') as f:
        f.write('#define OVL_NSLOTS %d\n#define OVL_FILE "%s"\n' %
                (nslots, meta['file']))
    nres = sum(1 for it in res if it.kind == 'text')
    print('%d overlays, %d resident functions, %d descriptors, %d thunks' %
          (len(ovls), nres, len(em.desc_items), len(em.thunks)))


# --------------------------------------------------------------------------
# Linking

def tool(name):
    d = os.environ.get('MOS_BIN',
                       os.path.expanduser('~/.rp6502/llvm-mos/bin'))
    p = os.path.join(d, name)
    return p if os.path.exists(p) else name


def run(cmd):
    import subprocess
    r = subprocess.run(cmd, capture_output=True, text=True)
    err = '\n'.join(l for l in r.stderr.splitlines()
                    if 'mallinfo2' not in l)
    if r.returncode:
        sys.exit('failed: %s\n%s%s' % (' '.join(cmd), r.stdout, err))
    if err.strip():
        print(err, file=sys.stderr)
    return r.stdout


def symbols(elf):
    out = run([tool('llvm-nm'), elf])
    syms = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return syms


def cmd_link(argv):
    """link OUTDIR [extra objects or flags for the resident link]"""
    import struct
    out = argv[0]
    extra = argv[1:]
    here = os.path.dirname(os.path.abspath(__file__))
    ovldir = os.path.join(here, '..', 'ovl')
    with open(os.path.join(out, 'ovlmeta.json')) as f:
        meta = json.load(f)
    nslots = meta['nslots']
    cc = tool('mos-rp6502-clang')
    novl = len(meta['overlays'])
    for i in range(novl):
        run([cc, '-c', os.path.join(out, 'ovl%03d.s' % i),
             '-o', os.path.join(out, 'ovl%03d.o' % i)])
    # Slot size: the largest overlay, rounded up to a page.
    sizes = []
    for i in range(novl):
        txt = run([tool('llvm-size'), '-A', os.path.join(out, 'ovl%03d.o' % i)])
        n = 0
        for line in txt.splitlines():
            p = line.split()
            if len(p) >= 2 and (p[0].startswith('.text') or
                                p[0].startswith('.rodata')):
                n += int(p[1])
            elif len(p) >= 2 and (p[0].startswith('.data') or
                                  p[0].startswith('.bss') or
                                  p[0].startswith('.zp') or
                                  p[0].startswith('.noinit')) and int(p[1]):
                sys.exit('overlay %d has %s' % (i, p[0]))
        sizes.append(n)
    slot = meta['slot_size'] or (max(sizes) + 255) // 256 * 256
    with open(os.path.join(out, 'ovlcfg.inc'), 'w') as f:
        f.write('OVL_NSLOTS = %d\nOVL_DEPTH = %d\n' % (nslots, 64))
    with open(os.path.join(out, 'slots.s'), 'w') as f:
        f.write('\t.section\t.noinit.__ovl_slots,"aw",@nobits\n'
                '\t.p2align\t8\n\t.globl\t__ovl_slot0\n__ovl_slot0:\n'
                '\t.zero\t%d\n' % (nslots * slot))
        f.write('\t.section\t.rodata.__ovl_slot_page,"a",@progbits\n'
                '\t.globl\t__ovl_slot_page\n__ovl_slot_page:\n')
        for i in range(nslots):
            f.write('\t.byte\t%d\n' % (i * slot // 256))
    objs = []
    for src in ('resident.s', 'slots.s'):
        o = os.path.join(out, src[:-2] + '.o')
        run([cc, '-c', os.path.join(out, src), '-o', o])
        objs.append(o)
    o = os.path.join(out, 'ovlrt.o')
    run([cc, '-c', '-I', out, os.path.join(ovldir, 'ovlrt.s'), '-o', o])
    objs.append(o)
    o = os.path.join(out, 'ovlload.o')
    run([cc, '-Os', '-fno-lto', '-c', '-I', out,
         os.path.join(ovldir, 'ovlload.c'), '-o', o])
    objs.append(o)
    rsp = os.path.join(out, 'undef.rsp')
    with open(rsp, 'w') as f:
        for u in meta['undef']:
            f.write('--undefined=%s\n' % u)
    exe = os.path.join(out, 'resident')
    run([cc, '-Os', '-fno-lto', '-o', exe] + objs + extra +
        ['-Wl,@' + rsp, '-Wl,-Map,' + exe + '.map'])
    rsyms = symbols(exe + '.elf')
    slot0 = rsyms['__ovl_slot0']
    # Link every overlay at slot 0 and one page higher; the bytes that
    # differ hold the high byte of an address inside the overlay.
    blob = bytearray()
    dirs = bytearray()
    entries = {}
    for i in range(novl):
        imgs = []
        for base in (slot0, slot0 + 256):
            ld = os.path.join(out, 'ovl.ld')
            with open(ld, 'w') as f:
                f.write('SECTIONS {\n  .ovl 0x%x : { *(.text .text.*) '
                        '*(.rodata .rodata.*) }\n'
                        '  /DISCARD/ : { *(.note.GNU-stack) *(.comment) }\n}\n' % base)
            elf = os.path.join(out, 'ovl%03d.elf' % i)
            run([tool('ld.lld'), '-T', ld, '--just-symbols=' + exe + '.elf',
                 '--no-check-sections',
                 os.path.join(out, 'ovl%03d.o' % i), '-o', elf])
            binf = elf[:-4] + '.bin'
            run([tool('llvm-objcopy'), '-O', 'binary', elf, binf])
            with open(binf, 'rb') as f:
                imgs.append(f.read())
            if base == slot0:
                syms = symbols(elf)
                for name in meta['overlays'][i]:
                    if name in syms:
                        entries[name] = syms[name]
        a, b = imgs
        if len(a) != len(b):
            sys.exit('overlay %d: images differ in size' % i)
        if len(a) > slot:
            sys.exit('overlay %d: %d bytes > slot %d' % (i, len(a), slot))
        rel = []
        for k in range(len(a)):
            if a[k] != b[k]:
                if (a[k] + 1) & 0xFF != b[k]:
                    sys.exit('overlay %d: odd relocation at %d' % (i, k))
                rel.append(k)
        dirs += struct.pack('<IHH', len(blob), len(a), len(rel))
        blob += a
        for k in rel:
            blob += struct.pack('<H', k)
    desc = bytearray()
    for name, unit in meta['desc']:
        e = entries[name] - slot0
        desc += bytes([unit, e & 0xFF, e >> 8])
    for name, data in ((meta['file'], blob), ('ovldesc.bin', desc),
                       ('ovldir.bin', dirs)):
        with open(os.path.join(out, name), 'wb') as f:
            f.write(data)
    with open(exe, 'rb') as f:
        rsize = len(f.read())
    print('resident %d bytes, slot0 $%04X, %d slots x %d, overlays %d '
          '(largest %d), file %d bytes' % (rsize, slot0, nslots, slot, novl,
                                           max(sizes) if sizes else 0,
                                           len(blob)))
    # The ROM: program, overlay file asset, XRAM tables.
    tools = os.path.join(here, '..', '..', '..', 'tools', 'rp6502.py')
    parts = []
    for name, addr in ((meta['file'], meta['file']),
                       ('ovldesc.bin', 'XRAM(0x%x)' % meta['desc_xram']),
                       ('ovldir.bin', 'XRAM(0x%x)' % meta['dir_xram'])):
        rom = os.path.join(out, name + '.rp6502')
        run([sys.executable, tools, 'create', '-a', addr, '-o', rom,
             os.path.join(out, name)])
        parts.append(rom)
    run([sys.executable, tools, 'create', '-a', 'file', '-r', 'file', '-o',
         os.path.join(out, 'program.rp6502'), exe] + parts)


def main():
    cmd = sys.argv[1]
    if cmd == 'stats':
        cmd_stats(sys.argv[2:])
    elif cmd == 'emit':
        cmd_emit(sys.argv[2:])
    elif cmd == 'link':
        cmd_link(sys.argv[2:])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
