#!/usr/bin/env python3
"""Sum the kept input sections of a link map by kind.

The map is the one lld writes for the nethack target (CMake adds -Wl,-Map in
the XRAM-tables configuration). Input-section lines end in "file:(section)",
so output-section lines, which have no such name, are skipped. The sizes are
the sections the linker kept, after --gc-sections.

Usage: mapsizes.py NETHACK.MAP
"""
import re
import sys

KINDS = [
    ('text', re.compile(r'\.text')),
    ('rodata', re.compile(r'\.rodata')),
    ('data', re.compile(r'\.data')),
    ('bss', re.compile(r'\.bss|\.noinit')),
]
LINE = re.compile(r'^\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(\d+)\s+(\S.*)$')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    totals = {k: 0 for k, _ in KINDS}
    with open(sys.argv[1], encoding='latin-1') as f:
        for line in f:
            m = LINE.match(line.rstrip('\n'))
            if not m:
                continue
            name = m.group(5)
            if ':(' not in name:
                continue
            size = int(m.group(3), 16)
            for kind, pat in KINDS:
                if pat.search(name):
                    totals[kind] += size
                    break
    for kind, _ in KINDS:
        print('%-7s %8d' % (kind, totals[kind]))


if __name__ == '__main__':
    main()
